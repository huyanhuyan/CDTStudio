#include "cdtattributedockwidget.h"
#include "ui_cdtattributedockwidget.h"
#include "stable.h"
#include "cdtimagelayer.h"
#include "cdtsegmentationlayer.h"
#include "dialoggenerateattributes.h"
#include "cdtattributesinterface.h"

extern QList<CDTAttributesInterface *>     attributesPlugins;

CDTAttributeDockWidget::CDTAttributeDockWidget(QWidget *parent) :
    CDTDockWidget(parent),
    ui(new Ui::CDTAttributeDockWidget),
    _segmentationLayer(NULL)
{
    ui->setupUi(this);

    setWindowTitle(tr("Attributes Manager"));

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(24,24));
    QAction *actionEditDataSource = new QAction(QIcon(":/Icon/DataSource.png"),tr("Edit Data Source"),toolBar);
    connect(actionEditDataSource,SIGNAL(triggered()),this,SLOT(onActionEditDataSourceTriggered()));
    toolBar->addAction(actionEditDataSource);

    QAction *actionGenerateAttributes = new QAction(QIcon(":/Icon/AddProperty.png"),tr("Generate Attributes"),toolBar);
    connect(actionGenerateAttributes,SIGNAL(triggered()),this,SLOT(onActionGenerateAttributesTriggered()));
    toolBar->addAction(actionGenerateAttributes);

    ui->horizontalLayout->setMenuBar(toolBar);
    connect(this,SIGNAL(databaseURLChanged(CDTDatabaseConnInfo)),SLOT(onDatabaseChanged(CDTDatabaseConnInfo)));

    QSettings settings("WHU","CDTStudio");
    ui->splitter->restoreGeometry(settings.value("CDTAttributeDockWidget/geometry").toByteArray());
    ui->splitter->restoreState(settings.value("CDTAttributeDockWidget/windowState").toByteArray());
}

CDTAttributeDockWidget::~CDTAttributeDockWidget()
{
    QSettings settings("WHU","CDTStudio");
    settings.setValue("CDTAttributeDockWidget/geometry", ui->splitter->saveGeometry());
    settings.setValue("CDTAttributeDockWidget/windowState", ui->splitter->saveState());
    delete ui;
}

CDTSegmentationLayer *CDTAttributeDockWidget::segmentationLayer() const
{
    return _segmentationLayer;
}

void CDTAttributeDockWidget::setCurrentLayer(CDTBaseObject *layer)
{
    if (_segmentationLayer == layer)
        return;

    //TODO  Process other layer type;

//    if (_segmentationLayer)
//    {
//        disconnect(_segmentationLayer,SIGNAL(destroyed()),this,SLOT(clear()));
//    }

    _segmentationLayer = qobject_cast<CDTSegmentationLayer *>(layer);
//    connect(_segmentationLayer,SIGNAL(destroyed()),this,SLOT(clear()));
    setDatabaseURL(_segmentationLayer->databaseURL());
    this->setEnabled(true);
}

void CDTAttributeDockWidget::onCurrentProjectClosed()
{
    clear();
}

void CDTAttributeDockWidget::setDatabaseURL(CDTDatabaseConnInfo url)
{
    if (_dbConnInfo == url)return;
    _dbConnInfo = url;
    emit databaseURLChanged(url);
}

void CDTAttributeDockWidget::updateTable()
{   
    QStringList attributes = attributeNames();
    QStringList originalTables = QSqlDatabase::database("attribute").tables();
    QStringList tableNames;
    foreach (QString name, originalTables) {
        if (attributes.contains(name))
            tableNames<<name;
    }
    foreach (QString tableName, tableNames) {
        QTableView* widget = new QTableView(ui->tabWidget);
        QSqlTableModel* model = new QSqlTableModel(widget,QSqlDatabase::database("attribute"));
        model->setTable(tableName);
        model->select();
        widget->setModel(model);
        widget->resizeColumnsToContents();
        widget->resizeRowsToContents();
        widget->setEditTriggers(QTableView::NoEditTriggers);
        widget->setItemDelegateForColumn(0,new CDTObjectIDDelegate(this));
        widget->verticalHeader()->hide();

        ui->tabWidget->addTab(widget,tableName);
        connect(widget,SIGNAL(clicked(QModelIndex)),SLOT(onItemClicked(QModelIndex)));
    }
}

void CDTAttributeDockWidget::clear()
{
    this->setEnabled(false);
    _dbConnInfo = CDTDatabaseConnInfo();
    clearTables();
    ui->qwtPlot->clear();
    _segmentationLayer =NULL;
}

void CDTAttributeDockWidget::onActionEditDataSourceTriggered()
{
    DialogDBConnection dlg(_dbConnInfo);
    if (dlg.exec()==QDialog::Accepted)
    {
        if (dlg.dbConnectInfo() == _dbConnInfo)
            return;
        _segmentationLayer->setDatabaseURL(dlg.dbConnectInfo());
        setDatabaseURL(dlg.dbConnectInfo());
    }
}

void CDTAttributeDockWidget::onActionGenerateAttributesTriggered()
{
    clearTables();
    CDTImageLayer* layer = (CDTImageLayer*)(segmentationLayer()->parent());
    DialogGenerateAttributes dlg(segmentationLayer()->id(),layer->bandCount());
    dlg.exec();
    updateTable();
}

void CDTAttributeDockWidget::onDatabaseChanged(CDTDatabaseConnInfo connInfo)
{
    clearTables();
    if (connInfo.isNull())
        return;

    _dbConnInfo = connInfo;
    QSqlDatabase db = QSqlDatabase::addDatabase(connInfo.dbType,"attribute");
    db.setDatabaseName(connInfo.dbName);
    db.setHostName(connInfo.hostName);
    db.setPort(connInfo.port);

    if (!db.open(connInfo.username, connInfo.password)) {
        QSqlError err = db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase("attribute");
        QMessageBox::critical(this,tr("Error"),tr("Open database failed!\n information:")+err.text());
    }
    else
    {
        updateTable();
    }
}

void CDTAttributeDockWidget::onItemClicked(QModelIndex index)
{    
    QTableView *view = (QTableView *)(ui->tabWidget->currentWidget());
    if (view==NULL)return;

    QString tableName = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
    QString featureName = view->model()->headerData(index.column(),Qt::Horizontal).toString();

    if (featureName.toLower() == "objectid")
        return;
    ui->qwtPlot->setDatabase(QSqlDatabase::database("attribute"));
    ui->qwtPlot->setTableName(tableName);
    ui->qwtPlot->setFieldName(featureName);

    ui->qwtPlot->replot();
}

QStringList CDTAttributeDockWidget::attributeNames()
{
    QStringList list;
    foreach (CDTAttributesInterface *interface, attributesPlugins) {
        list<<interface->tableName();
    }
    return list;
}

void CDTAttributeDockWidget::clearTables()
{
    while (ui->tabWidget->widget(0))
    {
        QWidget *widget = ui->tabWidget->widget(0);
        ui->tabWidget->removeTab(0);
        delete widget;
    }
}
