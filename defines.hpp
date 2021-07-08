/*!
    Software RX Switching E. Tichansky NO3M 2021
    v0.1
 */

#pragma once

#include <QAbstractSocket>
#include <QApplication>
#include <QBrush>
#include <QByteArray>
#include <QChar>
#include <QCloseEvent>
#include <QColor>
#include <QCollator>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QErrorMessage>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFlags>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QHostAddress>
#include <QItemDelegate>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QProgressDialog>
#include <QQueue>
#include <QReadWriteLock>
#include <QScreen>
#include <QSize>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlTableModel>
#include <QSqlRecord>
#include <QSettings>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTcpSocket>
#include <QtGlobal>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QtMath>
#include <QtSql>
#include <QtWebSockets>
#include <QtWidgets>
#include <QVariant>

#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QSize>

//#include <stdio.h>
#include <cstdio>
#include <cmath>
//#include <iostream>

// N4OGW:
// using C rather than C++ bindings for hamlib because I don't
// want to have to deal with exceptions
#include <hamlib/rig.h>
#include <hamlib/riglist.h>

const int NRIG=8;
const int kManual=0;
const int kCat=1;
const int kSubRx=2;
const int kDispCompass=1;
const int kDispList=0;
const int kDispNone=2;
const bool kPrevious=false;
const bool kNext=true;
const int kAvailable=0;
const int kSelected=1;
const int kUnavailable=2;

const QString kYes = QStringLiteral("Yes");
const QString kNo = QStringLiteral("No");
const QString kCompass = QStringLiteral("Compass");
const QString kList = QStringLiteral("List");
const QString kOff = QStringLiteral("Off");
const QString kOn = QStringLiteral("On");
const QString kSplit = QStringLiteral("Split");
//const QString kUser = QStringLiteral("User");
//const QColor hlClr = Qt::lightGray; // highlight color to set
const QColor hlClr = QColor("#e4e4e4");
const QColor txtClr = Qt::black; // highlighted text color to set

const int coChannel[NRIG] = { 1, 0, 3, 2, 5, 4, 7, 6 };

const QString websocketPort_def = QStringLiteral("7300");

//sql queries
// %2 additional matching, %3 sorting, %4 addditional statement (ie. join)
const QString kSelectGroupSQL = "SELECT groups.id as id, groups.name as name, groups.label as label,"
                            " groups.display_mode as display_mode from groups"
                            " inner join band_group_map on band_group_map.group_id = groups.id"
                            " inner join bands on bands.id = band_group_map.band_id %4"
                            " where groups.enabled = 1 and bands.id = %1 %2 %3";

// %2 additional matching, %3 sorting
const QString kSelectAntennaSQL = "select antennas.id as id, antennas.name as name, antennas.label as label,"
                              " antennas.start_deg as start_deg, antennas.stop_deg as stop_deg,"
                              " antennas.scan as scan, antennas.priority as priority,"
                              " antennas.switch_port as switch_port from antennas"
                              " inner join group_antenna_map on group_antenna_map.antenna_id = antennas.id"
                              " inner join groups on groups.id = group_antenna_map.group_id"
                              " where antennas.enabled = 1 and groups.id = %1 %2 %3";

const QString kAuxData = "AUX 0 %1 %2\r";

const QString s_radioBaudRate[NRIG]={"radios/radioBaudRate_1","radios/radioBaudRate_2",
                                     "radios/radioBaudRate_3","radios/radioBaudRate_4",
                                     "radios/radioBaudRate_5","radios/radioBaudRate_6",
                                     "radios/radioBaudRate_7","radios/radioBaudRate_8" };
const int s_radioBaudRate_def = 9600;
const QString s_radioBpf[NRIG]={"radios/radioBpf_1","radios/radioBpf_2",
                                "radios/radioBpf_3","radios/radioBpf_4",
                                "radios/radioBpf_5","radios/radioBpf_6",
                                "radios/radioBpf_7","radios/radioBpf_8" };
const bool s_radioBpf_def = false;
const QString s_radioHpf[NRIG]={"radios/radioHpf_1","radios/radioHpf_2",
                                "radios/radioHpf_3","radios/radioHpf_4",
                                "radios/radioHpf_5","radios/radioHpf_6",
                                "radios/radioHpf_7","radios/radioHpf_8" };
const bool s_radioHpf_def = false;
const QString s_radioAux[NRIG]={"radios/radioAux_1","radios/radioAux_2",
                                "radios/radioAux_3","radios/radioAux_4",
                                "radios/radioAux_5","radios/radioAux_6",
                                "radios/radioAux_7","radios/radioAux_8" };
const bool s_radioAux_def = false;
const QString s_radioEnable[NRIG]={"radios/radioEnable_1","radios/radioEnable_2",
                                   "radios/radioEnable_3","radios/radioEnable_4",
                                   "radios/radioEnable_5","radios/radioEnable_6",
                                   "radios/radioEnable_7","radios/radioEnable_8" };
const bool s_radioEnable_def = false;
const QString s_radioGain[NRIG]={"radios/radioGain_1","radios/radioGain_2",
                                 "radios/radioGain_3","radios/radioGain_4",
                                 "radios/radioGain_5","radios/radioGain_6",
                                 "radios/radioGain_7","radios/radioGain_8" };
const int s_radioGain_def = 0;
const QString s_radioBandDecoder[NRIG]={"radios/radioBandDecoder_1","radios/radioBandDecoder_2",
                                        "radios/radioBandDecoder_3","radios/radioBandDecoder_4",
                                        "radios/radioBandDecoder_5","radios/radioBandDecoder_6",
                                        "radios/radioBandDecoder_7","radios/radioBandDecoder_8" };
const int s_radioBandDecoder_def = 0; // MANUAL
const QString s_radioSubRxNr[NRIG]={"radios/radioSubRxNr_1","radios/radioSubRxNr_2",
                                     "radios/radioSubRxNr_3","radios/radioSubRxNr_4",
                                     "radios/radioSubRxNr_5","radios/radioSubRxNr_6",
                                     "radios/radioSubRxNr_7","radios/radioSubRxNr_8" };
const int s_radioSubRxNr_def = 1;
const QString s_radioTrackNr[NRIG]={"radios/radioTrackNr_1","radios/radioTrackNr_2",
                                    "radios/radioTrackNr_3","radios/radioTrackNr_4",
                                    "radios/radioTrackNr_5","radios/radioTrackNr_6",
                                    "radios/radioTrackNr_7","radios/radioTrackNr_8" };
const int s_radioTrackNr_def = 1;
const QString s_radioModel[NRIG]={"radios/radioModel_1","radios/radioModel_2",
                                  "radios/radioModel_3","radios/radioModel_4",
                                  "radios/radioModel_5","radios/radioModel_6",
                                  "radios/radioModel_7","radios/radioModel_8" };
const int s_radioModel_def = RIG_MODEL_DUMMY;
const QString s_radioName[NRIG]={"radios/radioName_1","radios/radioName_2",
                                 "radios/radioName_3","radios/radioName_4",
                                 "radios/radioName_5","radios/radioName_6",
                                 "radios/radioName_7","radios/radioName_8" };
const QString s_radioName_def = "";
const QString s_radioPauseScan[NRIG]={"radios/radioPauseScan_1","radios/radioPauseScan_2",
                                      "radios/radioPauseScan_3","radios/radioPauseScan_4",
                                      "radios/radioPauseScan_5","radios/radioPauseScan_6",
                                      "radios/radioPauseScan_7","radios/radioPauseScan_8" };
const bool s_radioPauseScan_def = false;
const QString s_radioScanDelay[NRIG]={"radios/radioScanDelay_1","radios/radioScanDelay_2",
                                      "radios/radioScanDelay_3","radios/radioScanDelay_4",
                                      "radios/radioScanDelay_5","radios/radioScanDelay_6",
                                      "radios/radioScanDelay_7","radios/radioScanDelay_8" };
const int s_radioScanDelay_def = 500;
const QString s_radioSerialPort[NRIG]={"radios/radioSerialPort_1","radios/radioSerialPort_2",
                                       "radios/radioSerialPort_3","radios/radioSerialPort_4",
                                       "radios/radioSerialPort_5","radios/radioSerialPort_6",
                                       "radios/radioSerialPort_7","radios/radioSerialPort_8" };
const QString s_radioSerialPort_def = "/dev/ttyS0";
const QString s_rigctld[NRIG]={"radios/rigctld_1","radios/rigctld_2",
                               "radios/rigctld_3","radios/rigctld_4",
                               "radios/rigctld_5","radios/rigctld_6",
                               "radios/rigctld_7","radios/rigctld_8" };
const bool s_rigctld_def = false;
const QString s_rigctldIp[NRIG]={"radios/rigctldIp_1","radios/rigctldIp_2",
                                 "radios/rigctldIp_3","radios/rigctldIp_4",
                                 "radios/rigctldIp_5","radios/rigctldIp_6",
                                 "radios/rigctldIp_7","radios/rigctldIp_8" };
const QString s_rigctldIp_def = "localhost";
const QString s_rigctldPort[NRIG]={"radios/rigctldPort_1","radios/rigctldPort_2",
                                   "radios/rigctldPort_3","radios/rigctldPort_4",
                                   "radios/rigctldPort_5","radios/rigctldPort_6",
                                   "radios/rigctldPort_7","radios/rigctldPort_8" };
const int s_rigctldPort_def = 4532;
const QString s_radioPollTime[NRIG]={"radios/radioPollTime_1","radios/radioPollTime_2",
                                     "radios/radioPollTime_3","radios/radioPollTime_4",
                                     "radios/radioPollTime_5","radios/radioPollTime_6",
                                     "radios/radioPollTime_7","radios/radioPollTime_8" };
const int s_radioPollTime_def = 500;
