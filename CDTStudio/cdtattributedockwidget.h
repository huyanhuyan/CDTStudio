#ifndef CDTATTRIBUTEDOCKWIDGET_H
#define CDTATTRIBUTEDOCKWIDGET_H

#include "cdtdockwidget.h"
#include "dialogdbconnection.h"

class QToolBar;
class QMenuBar;
class QPainter;
class QWidget;
class CDTSegmentationLayer;


namespace Ui {
class CDTAttributeDockWidget;
}

class CDTObjectIDDelegate:public QItemDelegate
{
    Q_OBJECT
public:
    CDTObjectIDDelegate ( QObject * parent = 0 ):QItemDelegate(parent){}
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const
    {
        QString text = index.data(Qt::DisplayRole).toString();
        painter->save();
        QRect cellRect = option.rect;
        painter->setPen(QColor(255,0,0));

        QFont font = index.data(Qt::FontRole).value<QFont>();
        font.setBold(true);
        painter->setFont(font);

        painter->drawText(cellRect, Qt::AlignHCenter|Qt::AlignVCenter, text);
        painter->restore();
    }
};

class CDTAttributeDockWidget : public CDTDockWidget
{
    Q_OBJECT

public:
    explicit CDTAttributeDockWidget(QWidget *parent = 0);
    ~CDTAttributeDockWidget();

    CDTSegmentationLayer *segmentationLayer()const;

signals:
    void databaseURLChanged(CDTDatabaseConnInfo);

public slots:    
    void setCurrentLayer(CDTBaseObject* layer);
    void onCurrentProjectClosed();

    void setDatabaseURL(CDTDatabaseConnInfo url);    
    void updateTable();
    void clearTables();
    void clear();

private slots:
    void onActionEditDataSourceTriggered();
    void onActionGenerateAttributesTriggered();
    void onDatabaseChanged(CDTDatabaseConnInfo connInfo);
    void onItemClicked(QModelIndex index);
private:
    static QStringList attributeNames();
private:
    Ui::CDTAttributeDockWidget *ui;
    CDTDatabaseConnInfo _dbConnInfo;
    CDTSegmentationLayer* _segmentationLayer;
};

#endif // CDTATTRIBUTEDOCKWIDGET_H
