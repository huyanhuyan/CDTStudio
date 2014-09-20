#include "mstsegmenter.h"
#include <exception>
#include <algorithm>
#include <iostream>
#include <QtCore>
#include "polygonizer.h"
#include "graphkruskal.h"
#include <gdal_priv.h>

MSTSegmenter::MSTSegmenter(const QString &inputImagePath,
                           const QString &outputImagePath,
                           const QString &shapefilePath,
//                           int MergeRule,
                           double threshold,
                           int minObjectSize,
                           bool shield,
                           QVector<double> layerWeights,
                           QObject *parent)
    : _inputImagePath(inputImagePath),
      _outputImagePath(outputImagePath),
      _shapefilePath(shapefilePath),
//      _mergeRule(MergeRule),
      _threshold(threshold),
      _minObjectSize(minObjectSize),
      shield_0_255(shield),
      _layerWeights(layerWeights),
      CDTBaseThread(parent)
{
    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","YES");

#ifdef Q_OS_WIN
    stxxl::disk_config disk((QDir::tempPath()+"\\cdtstudio_seg_stxxl").toLocal8Bit().constData(), 800 * 1024 * 1024, "wincall delete");
#endif
//#ifdef  Q_OS_UNIX
//    stxxl::disk_config disk((QDir::tempPath()+"\\cdtstudio_seg_stxxl").toLocal8Bit().constData(), 800 * 1024 * 1024, "syscall unlink");
//#endif
//    stxxl::config * cfg = stxxl::config::get_instance();
//    cfg->add_disk(disk);
}

MSTSegmenter::~MSTSegmenter()
{
    GDALDataset* poSrcDS = (GDALDataset*)srcDS;
    GDALDataset* poDstDS = (GDALDataset*)dstDS;
    GDALClose(poSrcDS);
    GDALClose(poDstDS);
}

void MSTSegmenter::run()
{    

    clock_t tSatart = clock();

    if (_CheckAndInit() == false)
    {
        emit aborted();
        return ;
    }

    GDALDataset* poSrcDS = (GDALDataset*)srcDS;
    GDALDataset* poDstDS = (GDALDataset*)dstDS;

    clock_t timer = clock();
    unsigned memory_to_use = 500 * 1024 * 1024;
    EdgeVector *vecEdge = new EdgeVector(edge_comparator(),memory_to_use);

    qDebug()<<"Init EdgeVector cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();

    emit currentProgressChanged(tr("Create edges of graph"));
    _CreateEdgeWeights(vecEdge);

    qDebug()<<"Create Edge Weights cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();
    emit currentProgressChanged(tr("Sort the edges"));

    vecEdge->sort();

    qDebug()<<"Sort Edge Weights cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();

    GraphKruskal* graph = NULL;
    emit currentProgressChanged(tr("Merging"));
    _ObjectMerge(graph,vecEdge,poSrcDS->GetRasterXSize()*poSrcDS->GetRasterYSize(),_threshold);

    qDebug()<<"Merge Object cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();

    emit currentProgressChanged(tr("Eliminate small object"));
    _EliminateSmallArea(graph,vecEdge,_minObjectSize);

    qDebug()<<"Eliminate Small Area cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();

    vecEdge->clear();
    delete vecEdge;

    emit currentProgressChanged(tr("Generating index"));
    //    std::map<unsigned,unsigned> mapRootidObjectid;
    QMap<unsigned,unsigned> mapRootidObjectid;

    GDALRasterBand* poMaskBand = poDstDS->GetRasterBand(1)->GetMaskBand();
    graph->GetMapNodeidObjectid(poMaskBand, mapRootidObjectid);

    qDebug()<<"Get Map Nodeid Objectid cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";
    timer = clock();

    emit currentProgressChanged(tr("Generating result"));
    _GenerateFlagImage(graph,mapRootidObjectid);
    delete graph;

    qDebug()<<"Generate Flag Image cost:"<<(clock()-timer)/(double)CLOCKS_PER_SEC<<"s";

    GDALClose(poSrcDS);
    GDALClose(poDstDS);
    Polygonizer* polygonizer = new Polygonizer(_outputImagePath,_shapefilePath);
    connect(polygonizer,SIGNAL(finished()),polygonizer,SLOT(deleteLater()));
    connect(polygonizer,SIGNAL(currentProgressChanged(QString)),this,SIGNAL(currentProgressChanged(QString)));
    connect(polygonizer,SIGNAL(progressBarSizeChanged(int,int)),this,SIGNAL(progressBarSizeChanged(int,int)));
    connect(polygonizer,SIGNAL(showNormalMessage(QString)),this,SIGNAL(showNormalMessage(QString)));
    connect(polygonizer,SIGNAL(showWarningMessage(QString)),this,SIGNAL(showWarningMessage(QString)));
    polygonizer->run();

    qDebug()<<"mst segmentation cost: "<<(clock()-tSatart)/(double)CLOCKS_PER_SEC<<"s";
    emit completed();
}



bool MSTSegmenter::_CheckAndInit()
{
    //1.Check the parameters
    if (_threshold<=0)
    {
        emit showWarningMessage(tr("Segmentation parameter \"threshold\" must greater than 0"));
        return false;
    }

    if (_minObjectSize <= 0)
    {
        emit showWarningMessage(tr("Segmentation parameter \"minObjectSize\" must greater than 0"));
        return false;
    }


    
    //2.Check Input Image
    GDALDataset* poSrcDS = ( GDALDataset*)GDALOpen(_inputImagePath.toUtf8().constData(),GA_ReadOnly);
    if (poSrcDS == NULL)
    {
        emit showWarningMessage(tr("Open Image ")+_inputImagePath+tr(" Failed!"));
        return false;
    }
    srcDS = poSrcDS;

    if (_layerWeights.size()==0)
    {
        _layerWeights.resize(poSrcDS->GetRasterCount());
        //        std::for_each(_layerWeights.begin(),_layerWeights.end(),[&](double &data)
        //        {
        //            data = 1./poSrcDS->GetRasterCount();
        //        });
        for(QVector<double>::Iterator iter = _layerWeights.begin();iter != _layerWeights.end();++iter)
        {
            *iter = 1./poSrcDS->GetRasterCount();
        }
    }

    //3.Init Output Image
    GDALDriver* poDriver = (GDALDriver*)GDALGetDriverByName("GTiff");
    GDALDataset* poDstDS = poDriver->Create(_outputImagePath.toUtf8().constData(),poSrcDS->GetRasterXSize(),poSrcDS->GetRasterYSize(),1,GDT_Int32,NULL);
    if (poDstDS == NULL)
    {
        emit showWarningMessage(tr("Create Image ")+_outputImagePath+tr(" Failed!"));
        return false;
    }
    dstDS = poDstDS;

    poDstDS->CreateMaskBand(GMF_PER_DATASET);
    GDALRasterBand* poBand = poDstDS->GetRasterBand(1)->GetMaskBand();
    for (int i=0;i<poDstDS->GetRasterYSize();++i)
    {
        std::vector<uchar> mask(poDstDS->GetRasterXSize(),1);
        if (poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte && shield_0_255)
        {
            uchar* image = new uchar[mask.size()];
            for (int k=0;k<poSrcDS->GetRasterCount();++k)
            {
                poSrcDS->GetRasterBand(k+1)->RasterIO(GF_Read,0,i,mask.size(),1,image,mask.size(),1,GDT_Byte,0,0);
                for (int j=0;j<mask.size();++j)
                {
                    if (image[j]==0 || image[j]==255)
                        mask[j]=0;
                }
            }
            delete []image;
        }

        poBand->RasterIO(GF_Write, 0,i,mask.size(),1,&mask[0],mask.size(),1,GDT_Byte,0,0);
    }

    double adfGeoTransform[6];
    poDstDS->SetProjection(poSrcDS->GetProjectionRef());
    poSrcDS->GetGeoTransform(adfGeoTransform);
    poDstDS->SetGeoTransform(adfGeoTransform);
    return true;
}

void MSTSegmenter::onComputeEdgeWeight(unsigned nodeID1, unsigned nodeID2, const QVector<double> &data1, const QVector<double> &data2, const QVector<double> &layerWeight, void *p)
{    
    EdgeVector* vecEdge = (EdgeVector*)p;
    double  difs=0;
    for (unsigned k=0;k<data1.size();++k)
    {
        difs += ((data1[k]-data2[k])*layerWeight[k])*((data1[k]-data2[k])*layerWeight[k]);
    }
    vecEdge->push(edge(nodeID1,nodeID2,(float)sqrt(difs)));
}

bool MSTSegmenter::_CreateEdgeWeights(void *p)
{
    EdgeVector* vecEdge = (EdgeVector*)p;
    vecEdge->clear();

    GDALDataset* poSrcDS = (GDALDataset*)srcDS;
    GDALDataset* poDstDS = (GDALDataset*)dstDS;
    unsigned nBandCount = poSrcDS->GetRasterCount();
    unsigned width = poSrcDS->GetRasterXSize();
    unsigned height= poSrcDS->GetRasterYSize();
    GDALDataType gdalDataType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    unsigned pixelSize = GDALGetDataTypeSize(gdalDataType)/8;

    QVector<double> buffer1(nBandCount);
    QVector<double> buffer2(nBandCount);

    std::vector<unsigned char*> lineBufferUp(nBandCount);
    std::vector<unsigned char*> lineBufferDown(nBandCount);

    GDALRasterBand* poMask = poDstDS->GetRasterBand(1)->GetMaskBand();
    uchar* maskUp   = new uchar[width];
    uchar* maskDown = new uchar[width];
    for (unsigned k=0;k<nBandCount;++k)
    {
        lineBufferUp[k] = new unsigned char[width*pixelSize];
        lineBufferDown[k] = new unsigned char[width*pixelSize];
    }

    for (unsigned k=0;k<nBandCount;++k)
    {
        poSrcDS->GetRasterBand(k+1)->RasterIO(GF_Read,0,0,width,1,lineBufferUp[k],width,1,gdalDataType,0,0);
    }
    poMask->RasterIO(GF_Read, 0,0,width,1,maskUp,width,1,GDT_Byte,0,0);


    emit progressBarSizeChanged(0,height-1);
    const int progressGap = (height-1)/100;


    for (unsigned y=0;y<height-1;++y)
    {
        for (unsigned k=0;k<nBandCount;++k)
            poSrcDS->GetRasterBand(k+1)->RasterIO(GF_Read,0,y+1,width,1,lineBufferDown[k],width,1,gdalDataType,0,0);
        poMask->RasterIO(GF_Read, 0,y+1,width,1,maskDown,width,1,GDT_Byte,0,0);

        for(unsigned x=0;x<width;++x)
        {
            if (maskUp[x]==0) continue;

            unsigned nodeID1 = y*width+x;
            unsigned nodeIDNextLIne = nodeID1+width;
            if (x < width-1 )
            {
                if (maskUp[x+1]!=0)
                {
                    for(unsigned k=0;k<nBandCount;++k)
                    {
                        buffer1[k] = SRCVAL(lineBufferUp[k],gdalDataType,x);
                        buffer2[k] = SRCVAL(lineBufferUp[k],gdalDataType,x+1);
                    }
                    //                    emit computeEdgeWeight(nodeID1,nodeID1+1,buffer1,buffer2,_layerWeights,vecEdge);
                    //                    ++edgeSize;
                    onComputeEdgeWeight(nodeID1,nodeID1+1,buffer1,buffer2,_layerWeights,vecEdge);
                }
            }

            if(y < height-1)
            {
                if (maskDown[x]!=0)
                {
                    for(unsigned k=0;k<nBandCount;++k)
                    {
                        buffer1[k] = SRCVAL(lineBufferUp[k],gdalDataType,x);
                        buffer2[k] = SRCVAL(lineBufferDown[k],gdalDataType,x);
                    }
                    //                    emit computeEdgeWeight(nodeID1,nodeID1+1,buffer1,buffer2,_layerWeights,vecEdge);
                    //                    ++edgeSize;
                    onComputeEdgeWeight(nodeID1,nodeIDNextLIne,buffer1,buffer2,_layerWeights,vecEdge);
                }
            }
        }

        std::vector<unsigned char*> tempBuffer = lineBufferDown;
        lineBufferDown = lineBufferUp;
        lineBufferUp = tempBuffer;

        uchar* p    = maskUp;
        maskUp      = maskDown;
        maskDown    = p;

        //        ++pd;
        if (y%progressGap==0)
            emit progressBarValueChanged(y);

    }


    emit progressBarValueChanged(height-1);

    for (unsigned k=0;k<nBandCount;++k)
    {
        delete [](lineBufferUp[k]);
        delete [](lineBufferDown[k]);
    }
    delete []maskUp;
    delete []maskDown;




    return true;
}

bool MSTSegmenter::_ObjectMerge(GraphKruskal *&graph,
                                void *p,
                                unsigned num_vertices,
                                double threshold)
{
    EdgeVector* vecEdge = (EdgeVector*)p;
    graph = new GraphKruskal(num_vertices);

    //    boost::progress_display pd(vecEdge.size(),std::cout,"merging\n","","");
    //    for (unsigned k = 0; k < vecEdge.size(); ++k)
    int barSize = vecEdge->size();
    emit progressBarSizeChanged(0,barSize);
    int i=0;
    const int progressGap = barSize/100;

    while(!vecEdge->empty())
    {
        edge edge_temp = *(*vecEdge);
        unsigned a = graph->find(edge_temp.GetNode1());
        unsigned b = graph->find(edge_temp.GetNode2());

        int nPredict = 0;
        if ((a != b) && (graph->joinPredicate_sw(a,b,(float)threshold,edge_temp.GetWeight(),nPredict)==true))
        {
            graph->join_band_sw(a,b,edge_temp.GetWeight());
            graph->find(a);
        }
        ++(*vecEdge);
        if (i%progressGap==0)
            emit progressBarValueChanged(i);
        ++i;
        //        ++pd;
    }
    emit progressBarValueChanged(barSize);
    return true;
}

bool MSTSegmenter::_EliminateSmallArea(GraphKruskal * &graph,
                                       void *p,
                                       double _minObjectSize)
{
    EdgeVector* vecEdge = (EdgeVector*)p;
    vecEdge->rewind();
    int barSize = vecEdge->size();
    int i=0;
    const int progressGap = barSize/100;
    emit progressBarSizeChanged(0,barSize);
    //    boost::progress_display pd(vecEdge.size(),std::cout,"eliminating small area\n","","");
    //    for (unsigned k = 0; k < vecEdge.size(); ++k)
    while(!vecEdge->empty())
    {
        edge edge_temp = *(*vecEdge);
        unsigned a = graph->find(edge_temp.GetNode1());
        unsigned b = graph->find(edge_temp.GetNode2());

        if ((a != b) && ((graph->size(a) <_minObjectSize) || (graph->size(b) < _minObjectSize)) )
        {
            graph->join_band_sw(a,b,edge_temp.GetWeight());
            graph->find(a);
        }
        ++(*vecEdge);
        if (i%progressGap==0)
            emit progressBarValueChanged(i);
        //        ++pd;
        ++i;
    }
    emit progressBarValueChanged(barSize);

    return true;
}

bool MSTSegmenter::_GenerateFlagImage(GraphKruskal *&graph,const QMap<unsigned, unsigned> &mapRootidObjectid)
{    
    GDALDataset* poSrcDS = (GDALDataset*)srcDS;
    GDALDataset* poDstDS = (GDALDataset*)dstDS;

    unsigned width = poSrcDS->GetRasterXSize();
    unsigned height= poSrcDS->GetRasterYSize();

    //    boost::progress_display pd(height,std::cout,"generating result\n","","");

    const int progressGap = height/100;
    emit progressBarSizeChanged(0,height);
    GDALRasterBand* poFlagBand = poDstDS->GetRasterBand(1);
    GDALRasterBand* poMaskBand = poFlagBand->GetMaskBand();
    std::vector<uchar> mask(width);

    for(unsigned i=0,index =0;i<height;++i)
    {
        poMaskBand->RasterIO(GF_Read,0,i,width,1,&mask[0],width,1,GDT_Byte,0,0);
        for(unsigned j=0;j<width;++j,++index)
        {
            int objectID = 0;
            if (mask[j]!=0)
            {
                int root = graph->find(index);
                objectID = mapRootidObjectid.value(root);
            }
            poFlagBand->RasterIO(GF_Write,j,i,1,1,(int *)&objectID,1,1,GDT_Int32,0,0);
        }
        if (i%progressGap == 0)
            emit progressBarValueChanged(i);
        //        ++pd;
    }
    emit progressBarValueChanged(height);
    return true;
}

