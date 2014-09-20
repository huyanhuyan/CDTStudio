#include "cdttaskmanager.h"
#include <QtCore>
#include "cdttask_pbcdbinary.h"
#include "cdtprocessorapplication.h"

QMap<QString,QMetaObject> CDTTaskManager::availableTasks;

CDTTaskManager::CDTTaskManager(QObject *parent) :
    QObject(parent)
{
    CDTTaskManager::registerTask("PBCD_Binary",CDTTask_PBCDBinary::staticMetaObject);

    connect(this,SIGNAL(taskInfoUpdated(QString,CDTTaskInfo)),SLOT(onTaskInfoUpdated(QString,CDTTaskInfo)));
}

void CDTTaskManager::registerTask(QString taskName,QMetaObject metaObj)
{
    availableTasks.insert(taskName,metaObj);
}

bool CDTTaskManager::appendNewTask(QString name, QString id, QDomDocument params)
{
    if (availableTasks.contains(name))
    {
        QMetaObject metaObj = availableTasks.value(name);
        CDTTask* newTask = qobject_cast<CDTTask*>(metaObj.newInstance(Q_ARG(QString,id),Q_ARG(QDomDocument,params),Q_ARG(QObject*,this)));
        if (newTask==NULL)
        {
            qApp->returnDebugMessage("create new task failed!");
            return false;
        }
        tasks.insert(id,newTask);
        connect(newTask,SIGNAL(taskInfoUpdated(QString,CDTTaskInfo)),this,SIGNAL(taskInfoUpdated(QString,CDTTaskInfo)));

        waitingTasksQueue.enqueue(id);
        emit taskAppended(id);

        if (currentTaskID.isEmpty())//not busy
        {
            if (!waitingTasksQueue.empty())
            {
                QString newID = waitingTasksQueue.dequeue();
                currentTaskID = newID;
                tasks.value(newID)->start();
            }
        }

        return true;
    }
    else
    {
        qApp->returnDebugMessage(QString("not contain task: %1!").arg(name));
        return false;
    }
}
void CDTTaskManager::queryTaskInfo(QString id)
{

}

void CDTTaskManager::onTaskInfoUpdated(QString id, CDTTaskInfo info)
{
    if (info.status==CDTTaskInfo::COMPLETED)
    {
        currentTaskID.clear();
        if (!waitingTasksQueue.empty())
        {
            QString newID = waitingTasksQueue.dequeue();
            currentTaskID = newID;
            tasks.value(newID)->start();
        }
    }
}
