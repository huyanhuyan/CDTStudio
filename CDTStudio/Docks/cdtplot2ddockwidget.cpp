#include "cdtplot2ddockwidget.h"
#include "cdthistogramplot.h"

CDTPlot2DDockWidget::CDTPlot2DDockWidget(QWidget *parent) :
    CDTDockWidget(parent),
    qwtPlot(new CDTHistogramPlot(this))
{        
    this->setWindowTitle(tr("Plot2D Panel"));
    qwtPlot->setObjectName(QString::fromUtf8("qwtPlot"));
    qwtPlot->setProperty("autoReplot", false);
    qwtPlot->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    qwtPlot->setMinimumHeight(200);
    this->setWidget(qwtPlot);
    qwtPlot->show();
}

CDTPlot2DDockWidget::~CDTPlot2DDockWidget()
{
    qwtPlot->clear();
}

void CDTPlot2DDockWidget::setCurrentLayer(CDTBaseLayer *layer)
{
    Q_UNUSED(layer);
}

void CDTPlot2DDockWidget::onCurrentProjectClosed()
{
    qwtPlot->clear();
}

void CDTPlot2DDockWidget::setDataSource(QSqlDatabase db, QString tableName, QString fieldName)
{
    if (!db.isValid())
    {
        logger()->error("Database is invalid!");
        return;
    }
    if (!db.isOpen())
    {
        logger()->error("Database is not opened!");
        return;
    }
    if (!db.tables().contains(tableName))
    {
        logger()->error("Table %1 is not in database %2!",tableName,db.connectionName());
        return;
    }

    logger()->info("Data source is set. Connection name: %1 Table name: %2 Field name: %3",
                   db.connectionName(),tableName,fieldName);
    qwtPlot->setDatabase(db);
    qwtPlot->setTableName(tableName);
    qwtPlot->setFieldName(fieldName);
    qwtPlot->replot();

}
