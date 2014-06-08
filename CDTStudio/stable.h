#ifndef STABLE_H
#define STABLE_H

//Qt Component
#include <QtCore>
#include <QtGui>
#include <QtXml>
#include <QtSql>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif

//GDAL
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

//OpenCV
#include <opencv2/opencv.hpp>

//QGIS
#include <qgis.h>
#include <qgisgui.h>

//Log4Qt
#include "log4qt/logger.h"
#include "log4qt/basicconfigurator.h"

#endif // STABLE_H
