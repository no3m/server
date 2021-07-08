/*!
    Software RX Switching E. Tichansky NO3M 2021
    v0.1
 */

#pragma once

#include "defines.hpp"
#include "ui_mainwindow.h"
#include "serial.hpp"
#include "delegates.hpp"

const int tmpStatusMsgDelay = 2000;
const int timerPeriod = 50;

typedef struct {
  QPushButton *button;
  int antenna;
} AntennaButton;

class RigSerial;

class MainWindow : public QMainWindow, private Ui::MainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private:
  QLabel        *rs485RcvdDataLabel;
  QSerialPort   *rs485Serial;
  QSettings     *settings;
  QSqlDatabase  db;
  QSqlTableModel *bandsTableModel;
  QSqlTableModel *groupsTableModel;
  QSqlTableModel *antennasTableModel;
  QSqlRelationalTableModel *band_groupTableModel;
  QSqlRelationalTableModel *group_antennaTableModel;
  QSqlRelationalTableModel *cronTableModel;
  QThread       *catThread[NRIG];
  QErrorMessage *errorBox;
  QTimer  mainTimer{this};
  QFileDialog directoryDialog{this};
  struct cronTimer {
    QTimer *timer;
    int cronjob;
    cronTimer(QTimer *t, int c) : timer(t), cronjob(c) {};
  };
  //QList<cronTimer> cronTimers;
  QList<QTimer*> cronTimers;
  void cronExecute(int);
  void cronStart();
  void cronStop();

  // websockets
  QWebSocketServer *webSocketServer;
  //QList<QWebSocket *> m_clients;
  struct clientinfo {
    QWebSocket *websocket;
    int radio;
    clientinfo(QWebSocket *ws, int r) : websocket(ws), radio(r) {};
  };
  QList<clientinfo> m_clients;
  void onNewConnection();
  void processMessage(const QByteArray&);
  void socketDisconnected();
  void sendRadioWindowData(int, const QJsonObject&, QWebSocket* = nullptr);
  void restartWebSocketServer();

  // radio window functions
  void radioNameSetText(int, QWebSocket* = nullptr);
  void radioConnectionStatus(int, bool, QWebSocket* = nullptr);
  void lbPttState(int, QWebSocket* = nullptr);
  void lbHpfSetText(int, QWebSocket* = nullptr);
  void lbBpfSetText(int, QWebSocket* = nullptr);
  void lbGainSetText(int, QWebSocket* = nullptr);
  void lbAuxSetText(int, QWebSocket* = nullptr);
  void cbBandSetEnabled(int, QWebSocket* = nullptr);
  void cbBandAddItems(int, QWebSocket* = nullptr);
  void cbBandSetText(int, QString, QWebSocket* = nullptr);
  void cbBandChanged(int, QString);
  void cbGroupSetEnabled(int,bool, QWebSocket* = nullptr);
  void cbGroupAddItems(int, QWebSocket* = nullptr);
  void cbGroupSetText(int, QString, QWebSocket* = nullptr);
  void cbGroupChanged(int, QString);
  void pbScanStatus(int, bool, QWebSocket* = nullptr);
  void pbScanSetEnabled(int,bool, QWebSocket* = nullptr);
  void pbLockStatus(int, bool, QWebSocket* = nullptr);
  void pbTrackSetEnabled(int,bool, QWebSocket* = nullptr);
  void pbTrackStatus(int, bool, QWebSocket* = nullptr);
  void cbLinkedSetEnabled(int,bool, QWebSocket* = nullptr);
  void cbLinkedAddItems(int, QWebSocket* = nullptr);
  void cbLinkedSetIndex(int, QWebSocket* = nullptr);
  void cbScanDelaySetEnabled(int,bool, QWebSocket* = nullptr);
  void cbScanDelaySetIndex(int, QWebSocket* = nullptr);
  void cbScanDelayAddItems(int, QWebSocket* = nullptr);
  void pbScanEnabledStatus(int, QWebSocket* = nullptr);
  void bearingLabelText(int, QWebSocket* = nullptr);
  void frameSetEnabled(int,bool, QWebSocket* = nullptr);

  QCheckBox *radioEnableCheckBox[NRIG];
  QLineEdit *radioNameLineEdit[NRIG];
  QSpinBox *radioScanDelaySpinBox[NRIG];
  QCheckBox *radioPauseScanCheckBox[NRIG];
  QCheckBox *radioBpfCheckBox[NRIG];
  QCheckBox *radioHpfCheckBox[NRIG];
  QCheckBox *radioAuxCheckBox[NRIG];
  QSpinBox *radioGainSpinBox[NRIG];
  QSpinBox *radioSubRxSpinBox[NRIG];
  QCheckBox *rigctldCheckbox[NRIG];
  QLineEdit *rigctldIpLineEdit[NRIG];
  QLineEdit *rigctldPortLineEdit[NRIG];
  QComboBox *radioManufComboBox[NRIG];
  QComboBox *radioModelComboBox[NRIG];
  QComboBox *radioSerialPortComboBox[NRIG];
  QComboBox *radioBaudRateComboBox[NRIG];
  QLineEdit *radioPollTimeLineEdit[NRIG];
  QComboBox *radioBandDecoderComboBox[NRIG];
  QFrame *radioCatFrame[NRIG];
  QFrame *radioProcFrame[NRIG];
  QFrame *radioGeneralFrame[NRIG];
  QPushButton *radioCatButton[NRIG];
  QLabel *radioFreqLabel[NRIG];
  QLabel *radioPttLabel[NRIG];
  QLabel *radioNameLabel[NRIG];
  QLabel *radioBandLabel[NRIG];
  QLabel *radioGroupLabel[NRIG];
  QLabel *radioAntennaLabel[NRIG];
  QLabel *clientsLabel[NRIG];

  RigSerial     *cat[NRIG];

  void initDatabase();
  void resetDatabase();
  void openDatabase();
  void populateSerialPortComboBox(QComboBox*);//, QString);
  void populateBaudRateComboBox(QComboBox*);//, QString);
  void rs485Connection();
  void rs485RcvdData();
  void rs485SendData(QByteArray);
  void setRadioFormFromSettings();
  void rejectSettings();
  void writeSettings();
  void saveSettingsButtonClicked();
  void statusPageInit();
  void initUiPtrs();
  void connectMainWindowSignals();
  void populateModelCombo(int,int);

  void radioEnableCheckBox_stateChanged(int);
  void rigctldCheckbox_stateChanged(int);
  void radioBandDecoderComboBoxChanged(int);
  void radioConnection(int);

  void about();
  void timeoutMainTimer();
  void addBand();
  void saveBand();
  void removeBand();
  void addGroup();
  void saveGroup();
  void removeGroup();
  void addAntenna();
  void saveAntenna();
  void removeAntenna();
  void addBandGroup();
  void saveBandGroup();
  void removeBandGroup();
  void addGroupAntenna();
  void saveGroupAntenna();
  void removeGroupAntenna();
  void addCron();
  void removeCron();
  void saveCron();
  void setupDatabaseModelsViews();

  PrioritySpinBoxDelegate priorityDelegate;
  CompassComboBoxItemDelegate compassDelegate;
  PortComboBoxItemDelegate portDelegate;
  VantComboBoxItemDelegate vantDelegate;
  DegreesSpinBoxDelegate degreesDelegate;
  YesNoComboBoxItemDelegate yesNoDelegate;
  FrequencySpinBoxDelegate frequencyDelegate;
  GainSpinBoxDelegate gainDelegate;
  AuxComboBoxItemDelegate auxDelegate;
  CatIdSpinBoxDelegate catIdDelegate;
  RadioComboBoxItemDelegate *radioDelegate;

  void bandChanged(int);
  void groupChanged(int);
  void groupNext(int);
  void groupPrev(int);
  void groupStep(int, bool);
  void antennaNext(int);
  void antennaPrev(int);
  void antennaStep(int,bool,bool=false);
  void antennaChanged(int);
  bool selectAntenna(int, bool=true, int=0);
  void bearingChanged(int);
  void updateBandComboSelection(int);
  void setAntennaLock(int, bool);
  void toggleAntennaLock(int);
  void toggleAntennaScanning(int);
  void toggleScanEnabled(int);
  void setAntennaScanning(int, bool);
  void toggleAntennaTracking(int);
  void setAntennaTracking(int,bool);
  int calcCenterBearing(int,int);
  void swapAntennas(int);

  int getDisplayMode(int);
  int getSwitchPort(int);

  int currentBand[NRIG];
  int currentAntenna[NRIG];
  int currentGroup[NRIG];
  int currentBearing[NRIG];
  int currentFreq[NRIG];
  bool currentPtt[NRIG];
  int currentTrackedRadio[NRIG];
  int currentScanDelay[NRIG];
  int currentGain[NRIG];
  bool scanState[NRIG];
  bool trackingState[NRIG];
  bool lockState[NRIG];
  bool radioConnected[NRIG];
  int scanCount[NRIG];
  int prevGroupTrack[NRIG];
  QString prevGroupLabel[NRIG];
  int prevAntennaTrack[NRIG];
  int prevBearingTrack[NRIG];

  int getAux(int);

  void createAntennaButtons(int, QWebSocket* = nullptr);
  void antennaButtonClicked(int, int);
  void updateAntennaButtonsSelection(int, QWebSocket* = nullptr);

  void updateGraphicsEllipse(int, QWebSocket* = nullptr);
  void updateGraphicsLines(int, QWebSocket* = nullptr);
  void updateGraphicsLabels(int, QWebSocket* = nullptr);

  void setLayoutIndex(int, int, QWebSocket* = nullptr);

  void bearingChangedMouse(int,int);

//public slots:

//signals:

//private slots:

protected:
  void closeEvent(QCloseEvent *) override;

};
