/*!
    Software RX Switching E. Tichansky NO3M 2021
    v0.1
 */

#include "mainwindow.hpp"
#include "cron.hpp"

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
{
  setupUi(this);
  setWindowTitle("NO3M RX Switching Server");

  errorBox = new QErrorMessage(this);
  errorBox->setModal(true);
  errorBox->setFont(QFont("Sans",10));

  initUiPtrs(); // GUI elements to arrays
  db = QSqlDatabase::addDatabase ("QSQLITE");
  openDatabase();
  initDatabase();

  // settings
  settings = new QSettings("softrx", "settings");
  //restore main window geometry and state
  restoreGeometry(settings->value("geometry").toByteArray());
  restoreState(settings->value("windowState").toByteArray());

  for (int i=0; i<NRIG; ++i) {
    populateSerialPortComboBox(radioSerialPortComboBox[i]);
    populateBaudRateComboBox(radioBaudRateComboBox[i]);
  }
  populateSerialPortComboBox(rs485PortComboBox);

  // CAT
  for (int i=0;i<NRIG;++i) {
    catThread[i] = new QThread;
    cat[i] = nullptr;
  }
  // setup first radio object to acquire hamlib rig models
  cat[0] = new RigSerial(0);

 // get hamlib supported manufacturer list
  for (int i = 0; i < cat[0]->hamlibNMfg(); ++i) {
    for (int j=0;j<NRIG;++j) {
      radioManufComboBox[j]->insertItem(i, cat[0]->hamlibMfgName(i));
    }
  }

  for (int i=0;i<NRIG;++i) {
    radioBandDecoderComboBox[i]->insertItem(kManual, "Manual");
    radioBandDecoderComboBox[i]->insertItem(kCat, "CAT");
    radioBandDecoderComboBox[i]->insertItem(kSubRx, "SubRX");
  }

  connectMainWindowSignals();
  setRadioFormFromSettings();

  for (int i=0; i<NRIG; ++i){
    radioEnableCheckBox_stateChanged(i);
    rigctldCheckbox_stateChanged(i);
    radioBandDecoderComboBoxChanged(i);
  }

  // setup other radios and threads
  cat[0]->moveToThread(catThread[0]);
  connect(catThread[0], &QThread::started, cat[0], &RigSerial::run);
  //connect(cat[0], &RigSerial::radioError, errorBox, qOverload<const QString &>(&QErrorMessage::showMessage));
  connect(cat[0], &RigSerial::radioError, errorBox, static_cast<void (QErrorMessage::*)(const QString &)>(&QErrorMessage::showMessage));
  // setup remaining radio objects and threads
  for (int i=1;i<NRIG;++i) {
    cat[i] = new RigSerial(i);
    cat[i]->moveToThread(catThread[i]);
    connect(catThread[i], &QThread::started, cat[i], &RigSerial::run);
    //connect(cat[i], &RigSerial::radioError, errorBox, qOverload<const QString &>(&QErrorMessage::showMessage));
    connect(cat[i], &RigSerial::radioError, errorBox, static_cast<void (QErrorMessage::*)(const QString &)>(&QErrorMessage::showMessage));
  }
  for (int i=0;i<NRIG;++i) {
    connect(qApp, &QApplication::aboutToQuit, cat[i], &RigSerial::stopSerial);
  }

  // rs485
  rs485Serial = new QSerialPort();
  rs485Button->setText("Connect");
  connect(rs485Button, &QPushButton::released, this, &MainWindow::rs485Connection);
  rs485RcvdDataLabel = new QLabel("");
  statusBar()->addPermanentWidget(rs485RcvdDataLabel);

  // start radio CAT connections
  // these are always "connected", unused radios should be on Hamlib Dummy
  for (int i=0;i<NRIG;++i) {
    //radioConnection(i);
  }

  // == database models and views ==
  bandsTableModel = new QSqlTableModel(this, db);
  groupsTableModel = new QSqlTableModel(this, db);
  antennasTableModel = new QSqlTableModel(this, db);
  band_groupTableModel = new QSqlRelationalTableModel(this, db);
  group_antennaTableModel = new QSqlRelationalTableModel(this, db);
  cronTableModel = new QSqlRelationalTableModel(this, db);
  setupDatabaseModelsViews();
  // == database ==

  hamlibVersionLabel->setText(QString(hamlib_version));
  radioStatusVLayout->addStretch(1);
  radioStatusVLayout_2->addStretch(1);
  lbCronStatus->setText("Cron stopped");
  lbCronStatus_2->setText("Cron stopped");
  QSqlQuery queryCron(db);
  queryCron.exec("update cron set next=''");
  cronTableModel->select();
  while (cronTableModel->canFetchMore()) {
    cronTableModel->fetchMore();
  }

  for (int i=0; i<NRIG; ++i) {
    radioSubRxSpinBox[i]->setPrefix("Radio ");
    radioGainSpinBox[i]->setSuffix(" dB");

    // save/restore linked radio....

    currentTrackedRadio[i] = settings->value(s_radioTrackNr[i], s_radioTrackNr_def).toInt();
    currentBand[i] = 0;
    currentAntenna[i] = 0;
    currentGroup[i] = 0;
    currentBearing[i] = -1;
    currentFreq[i] = 0;
    currentPtt[i] = false;
    currentGain[i] = 0;
    currentScanDelay[i] = settings->value(s_radioScanDelay[i], s_radioScanDelay_def).toInt();
    scanCount[i] = 0;
    setAntennaLock(i, false);
    scanState[i] = false;
    trackingState[i] = false;
    lockState[i] = false;
    //setAntennaScanning(i, false);
    //setAntennaTracking(i, false);
    radioConnected[i] = false;
    radioNameLabel[i]->setStyleSheet("QLabel { color : red; font-weight:600; }");
  }

  statusPageInit();

  if(settings->value("rs485auto", false).toBool()) {
    rs485Connection();
  }

  connect(pbRestartWebSocket, &QPushButton::released, this, &MainWindow::restartWebSocketServer);
  webSocketServer = new QWebSocketServer(QStringLiteral("softrx"),
                                          QWebSocketServer::NonSecureMode,
                                          this);
  if ( webSocketServer->listen(QHostAddress::Any, settings->value("websocketPort", websocketPort_def).toInt())) {
    //qDebug() << "websocket server listening "+settings->value("websocketPort", websocketPort_def).toString();
    statusBarUi->showMessage(QString("Websocket server listening on port %1")
                             .arg(settings->value("websocketPort",websocketPort_def).toString()),
                             5000);
    serverLog->appendPlainText(QString("[%1] Websocket server started on port %2")
                             .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                             .arg(settings->value("websocketPort",websocketPort_def).toString()));
    connect(webSocketServer, &QWebSocketServer::newConnection, this, &MainWindow::onNewConnection);
  }

    // start timer after everything else is setup
  connect(&mainTimer, &QTimer::timeout, this, &MainWindow::timeoutMainTimer);
  mainTimer.start(timerPeriod);
  //qDebug() << "MainWindow timer thread " << mainTimer.thread();

  // = end constructor
}

void MainWindow::cronStart()
{
  cronStop();
  QSqlQuery query(db);
  query.exec("SELECT * from cron where enabled = 1");
  std::time_t now = std::time(0);
  cronLog->appendPlainText(QString("[%1] Cron started")
                            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
  while (query.next()) {
    int id = query.value("id").toInt();
    std::time_t next;
    const std::string expr = QString(query.value("expression").toString()).toStdString();
    try
    {
      auto cron = cron::make_cron(expr);
      next = cron::cron_next(cron, now);
    }
    catch (cron::bad_cronexpr const & ex)
    {
      qDebug() << ex.what();
      cronLog->appendPlainText(QString("[%1] Cron(%2) bad expression")
                                .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                .arg(id));
      continue;
    }

    //qDebug() << "cron " << id << " interval(s) " << next - now;
    QTimer *timer = new QTimer(this);
    cronTimers << timer; // cronTimer(timer, id);
    timer->setInterval((next - now)*1000);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [=]() { cronExecute(id); } );
    timer->start();
    //QString nextStr = QDateTime::fromSecsSinceEpoch(next, Qt::UTC).toLocalTime().time().toString();
    QString nextStr = QDateTime::fromSecsSinceEpoch(next, Qt::UTC).toLocalTime().toString(Qt::ISODate);
    QSqlQuery queryUpdate(db);
    queryUpdate.exec(QString("update cron set next = '%1' where id = %2")
                              .arg(nextStr)
                              .arg(id));
    cronTableModel->select();
    while (cronTableModel->canFetchMore()) {
      cronTableModel->fetchMore();
    }
    cronLog->appendPlainText(QString("[%1] Cronjob(%2) added")
                                .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                .arg(id));
  }
  //qDebug() << "Cron started";
  statusBarUi->showMessage("Cron started", tmpStatusMsgDelay);
  lbCronStatus->setText("Cron running");
  lbCronStatus_2->setText("Cron running");
}

void MainWindow::cronStop()
{
  for (const auto &crontimer : qAsConst(cronTimers)) {
    crontimer->deleteLater();
  }
  cronTimers.clear();
  //qDebug() << "Cron stopped";
  statusBarUi->showMessage("Cron stopped", tmpStatusMsgDelay);
  lbCronStatus->setText("Cron stopped");
  lbCronStatus_2->setText("Cron stopped");
  QSqlQuery queryCron(db);
  queryCron.exec("update cron set next=''");
  cronTableModel->select();
  while (cronTableModel->canFetchMore()) {
    cronTableModel->fetchMore();
  }
  cronLog->appendPlainText(QString("[%1] Cron stopped")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void MainWindow::cronExecute(int cronId)
{
  QTimer *timer = qobject_cast<QTimer *>(sender());

  //qDebug() << "Execute cron ID " << cronId;
  QSqlQuery queryCron(db);
  queryCron.exec(QString("SELECT * from cron where id = %1")
                          .arg(cronId));
  if (queryCron.first()) {
    int antenna = queryCron.value("antenna_id").toInt();
    int radio = queryCron.value("radio_id").toInt();
    //qDebug() << "Radio " << radio << " Antenna " << antenna;
    int coChannelPort = getSwitchPort(currentAntenna[coChannel[radio]]);

    QSqlQuery queryAntenna(db);
    QString sqlAntenna;
    sqlAntenna.append(QString("SELECT * from antennas where id = %1"
                              " and enabled = 1 and switch_port <> %2")
                              .arg(antenna)
                              .arg(coChannelPort) );
    //sqlAntenna.append(QString(" and enabled = 1 and switch_port <> %1").arg(coChannelPort));
    if (radio < 4) {
      sqlAntenna.append(" and radios1_4 = 1");
    } else {
      sqlAntenna.append(" and radios5_8 = 1");
    }
    queryAntenna.exec(sqlAntenna);
    if (queryAntenna.first()) { // antenna found and valid
      //qDebug() << "antenna found";
      QString sqlGroup,match,sort,extra;
      if (radio < 4) {
        match.append(" and groups.radios1_4 = 1");
      } else {
        match.append(" and groups.radios5_8 = 1");
      }
      match.append(QString(" and antennas.id = %1")
                        .arg(antenna));
      sort.append(" order by groups.priority desc, groups.id desc");
      extra.append(" inner join group_antenna_map on group_antenna_map.group_id = groups.id"
                   " inner join antennas on antennas.id = group_antenna_map.antenna_id");
      sqlGroup.append(kSelectGroupSQL.arg(currentBand[radio])
                                     .arg(match)
                                     .arg(sort)
                                     .arg(extra) );

      //qDebug() << sqlGroup;
      QSqlQuery queryGroup(db);
      queryGroup.exec(sqlGroup);
      if (queryGroup.first()) { // found a group, highest priority
        //qDebug() << "group found " << queryGroup.value("id").toInt();
        setAntennaLock(radio, false);
        setAntennaScanning(radio, false);
        setAntennaTracking(radio, false);
        int display_mode = queryGroup.value("display_mode").toInt();
        int bearing = calcCenterBearing(queryAntenna.value("start_deg").toInt(),
                                        queryAntenna.value("stop_deg").toInt());
        if (currentBearing[radio] != bearing) {
          currentBearing[radio] = bearing;
          bearingChanged(radio);
        }
        if (currentGroup[radio] != queryGroup.value("id").toInt()) {
          currentGroup[radio] = queryGroup.value("id").toInt();
          radioGroupLabel[radio]->setText(queryGroup.value("label").toString());
          cbGroupSetText(radio, queryGroup.value("label").toString());
          //groupChanged(radio);
          if (display_mode == kDispList) {
            createAntennaButtons(radio);
            setLayoutIndex(radio, kDispList);
          } else if (display_mode == kDispCompass) {
            updateGraphicsLines(radio);
            setLayoutIndex(radio, kDispCompass);
          } else {
            setLayoutIndex(radio, kDispNone);
          }
        }
        if (currentAntenna[radio] != antenna) {
          currentAntenna[radio] = antenna;
          antennaChanged(radio);
          /* // handled in antennaChanged
          if (display_mode == kDispList) {
            updateAntennaButtonsSelection(radio);
            pbScanEnabledStatus(radio);
          } else if (display_mode == kDispCompass) {
            updateGraphicsEllipse(radio);
            updateGraphicsLabels(radio);
            pbScanEnabledStatus(radio);
          }
          */
        }

        //statusBarUi->showMessage("Executing cron("+QString::number(cronId)
        //                         +"): "+settings->value(s_radioName[radio], s_radioName_def).toString()
        //                         +" -> "+queryAntenna.value("name").toString(), 5000);
        statusBarUi->showMessage(
          QString("Executing cronjob(%1): %2 -> %3")
              .arg(cronId)
              .arg(settings->value(s_radioName[radio],s_radioName_def).toString())
              .arg(queryAntenna.value("name").toString()),
          5000);

        cronLog->appendPlainText(QString("[%1] Cronjob(%2) executed OK")
                                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                    .arg(cronId));

      } else { // group not found
        cronLog->appendPlainText(QString("[%1] Cronjob(%2) group not found")
                                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                    .arg(cronId));
      }

    } else { // antenna not found or invalid
      cronLog->appendPlainText(QString("[%1] Cronjob(%2) antenna(%3) not found")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(cronId)
                                  .arg(antenna));
    }

    // setup next timer interval for this cronjob
    const std::string expr = QString(queryCron.value("expression").toString()).toStdString();
    std::time_t now = std::time(0);
    std::time_t next;
    try
    {
      auto cron = cron::make_cron(expr);
      next = cron::cron_next(cron, now);
    }
    catch (cron::bad_cronexpr const & ex)
    {
      qDebug() << ex.what();
      for (int i=0; i < cronTimers.count(); ++i) {
        if (cronTimers.at(i) == timer) {
          cronTimers.removeAt(i);
          break;
        }
      }
      QSqlQuery queryUpdate(db);
      queryUpdate.exec(QString("update cron set next = '' where id = %1")
                            .arg(cronId));
      cronTableModel->select();
      while (cronTableModel->canFetchMore()) {
        cronTableModel->fetchMore();
      }
      timer->deleteLater();
      cronLog->appendPlainText(QString("[%1] Cronjob(%2) bad expression")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(cronId));
      return;
    }
    timer->setInterval((next - now)*1000);
    timer->setSingleShot(true);
    timer->start();
    //QString nextStr = QDateTime::fromSecsSinceEpoch(next, Qt::UTC).toLocalTime().time().toString();
    QString nextStr = QDateTime::fromSecsSinceEpoch(next, Qt::UTC).toLocalTime().toString(Qt::ISODate);
    QSqlQuery queryUpdate(db);
    queryUpdate.exec(QString("update cron set next = '%1' where id = %2")
                              .arg(nextStr)
                              .arg(cronId));
    cronTableModel->select();
    while (cronTableModel->canFetchMore()) {
      cronTableModel->fetchMore();
    }
    cronLog->appendPlainText(QString("[%1] Cronjob(%2) re-queued")
                                .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                .arg(cronId));

  } else { // not in database, remove timer from list and delete timer
    for (int i=0; i < cronTimers.count(); ++i) {
      if (cronTimers.at(i) == timer) {
        cronTimers.removeAt(i);
        break;
      }
    }
    timer->deleteLater();
    cronLog->appendPlainText(QString("[%1] Cronjob(%2) not found")
                                .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                .arg(cronId));
  }

}

void MainWindow::restartWebSocketServer()
{
  // existing clients stay connected on original port
  webSocketServer->close();
  if ( webSocketServer->listen(QHostAddress::Any, settings->value("websocketPort", websocketPort_def).toInt())) {
    //qDebug() << "websocket server listening "+settings->value("websocketPort", websocketPort_def).toString();
    statusBarUi->showMessage(QString("Websocket server listening on port %1")
                              .arg(settings->value("websocketPort",websocketPort_def).toString()),
                             5000);
    serverLog->appendPlainText(QString("[%1] Websocket server restarted on port %2")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(settings->value("websocketPort",websocketPort_def).toString()));
  }
}

void MainWindow::processMessage(const QByteArray &message)
{
  QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());

  QJsonDocument doc = QJsonDocument::fromJson(message);
  QJsonObject object = doc.object();

  // client registration
  if (object.contains("radio")) {
    int nrig = object.value("radio").toInt() - 1;
    if (nrig >= 0 && nrig < 8) {
      bool registered = false;
      for (const auto &client : qAsConst(m_clients)) {
        if (client.websocket == pSender) {
          registered = true;
        }
      }
      if (registered) {
        //qDebug() << "client already registered, re-initializing";
        serverLog->appendPlainText(QString("[%1] %2:%3 re-initializing as radio %4")
                                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                      .arg(pSender->peerAddress().toString())
                                      .arg(pSender->peerPort())
                                      .arg(nrig + 1));
      } else {
        m_clients << clientinfo(pSender, nrig);
        //qDebug() << "new client registered as radio " << nrig + 1;
        statusBarUi->showMessage(QString("New client registered as radio %1")
                                      .arg(nrig + 1),
                                      tmpStatusMsgDelay);
        serverLog->appendPlainText(QString("[%1] %2:%3 registered as radio %4")
                                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                      .arg(pSender->peerAddress().toString())
                                      .arg(pSender->peerPort())
                                      .arg(nrig + 1));
      }
      // initialize client window
      radioNameSetText(nrig, pSender);
      radioConnectionStatus(nrig, radioConnected[nrig], pSender); // cat[nrig]->radioOpen());
      cbBandAddItems(nrig, pSender);
      //updateBandComboSelection(nrig);
      cbBandSetText(nrig, radioBandLabel[nrig]->text(), pSender);
      cbBandSetEnabled(nrig, pSender); // not effected by lock state
      bearingLabelText(nrig, pSender);

      cbLinkedAddItems(nrig, pSender);
      cbLinkedSetIndex(nrig, pSender);
      cbScanDelayAddItems(nrig, pSender);
      cbScanDelaySetIndex(nrig, pSender);
      pbScanEnabledStatus(nrig, pSender);
      cbGroupAddItems(nrig, pSender);
      lbPttState(nrig, pSender);
      lbHpfSetText(nrig, pSender);
      lbBpfSetText(nrig, pSender);
      lbGainSetText(nrig, pSender);
      lbAuxSetText(nrig, pSender);

      int display_mode = getDisplayMode(currentGroup[nrig]);

      if (currentGroup[nrig] > 0) {
        if (display_mode == kDispList) {
          createAntennaButtons(nrig, pSender);
          updateAntennaButtonsSelection(nrig, pSender);
          setLayoutIndex(nrig, kDispList, pSender);
        } else if (display_mode == kDispCompass) {
          updateGraphicsLines(nrig, pSender);
          updateGraphicsLabels(nrig, pSender);
          updateGraphicsEllipse(nrig, pSender);
          setLayoutIndex(nrig, kDispCompass, pSender);
        } else {
          setLayoutIndex(nrig, kDispNone, pSender);
        }
      } else {
        setLayoutIndex(nrig, kDispNone, pSender);
      }
      //if (scanState[nrig]) {
        pbScanStatus(nrig, scanState[nrig], pSender);
      //} else if (trackingState[nrig]) {
        pbTrackStatus(nrig, trackingState[nrig], pSender);
      if (trackingState[nrig]) {
        cbLinkedSetEnabled(nrig, false, pSender);
      }
      //} else if (lockState[nrig]) {
        pbLockStatus(nrig, lockState[nrig], pSender);
      if (lockState[nrig]) {
        cbGroupSetEnabled(nrig, false, pSender);
        pbScanSetEnabled(nrig, false, pSender);
        pbTrackSetEnabled(nrig, false, pSender);
        cbLinkedSetEnabled(nrig, false, pSender);
        cbScanDelaySetEnabled(nrig, false, pSender);
        frameSetEnabled(nrig, false, pSender);
      }
      // end init

      // update client count
      int cnt = 0;
      for (const auto &client : qAsConst(m_clients)) {
        if (client.radio == nrig) {
          cnt++;
        }
      }
      if (cnt) {
        clientsLabel[nrig]->setText(QString::number(cnt));
      } else {
        clientsLabel[nrig]->setText("");
      }
    }
    return;
  }

  if (!object.contains("action")) return;
  if (!object.value("action").isString()) return;

  int nrig = -1;
  for (const auto &client : qAsConst(m_clients)) {
    if (client.websocket == pSender) {
      nrig = client.radio;
      break;
    }
  }

  if (nrig >= 0) {
    //qDebug() << "processMessage radio: " << nrig+1;
    serverLog->appendPlainText(QString("[%1][%2] %3:%4 %5")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(nrig + 1)
                                  .arg(pSender->peerAddress().toString())
                                  .arg(pSender->peerPort())
                                  .arg(QString::fromUtf8(message).simplified()));

    if (object.value("action").toString() == "bearing") {
      bearingChangedMouse(nrig, object.value("degrees").toInt());
      //qDebug() << "bearing " << bearing;
    } else if (object.value("action").toString() == "buttonclicked") {
      antennaButtonClicked(nrig, object.value("antenna").toInt());
    } else if (object.value("action").toString() == "togglescan") {
      //qDebug() << "togglescan";
      toggleAntennaScanning(nrig);
    } else if (object.value("action").toString() == "togglescanenabled") {
      toggleScanEnabled(nrig);
    } else if (object.value("action").toString() == "toggletracking") {
      //qDebug() << "toggletracking";
      toggleAntennaTracking(nrig);
    } else if (object.value("action").toString() == "togglelock") {
      //qDebug() << "togglelock";
      toggleAntennaLock(nrig);
    } else if (object.value("action").toString() == "changegroup") {
      cbGroupChanged(nrig, object.value("value").toString());
      //qDebug() << "changegroup " << group;
      // search group
    } else if (object.value("action").toString() == "changeband") {
      cbBandChanged(nrig, object.value("value").toString());
      //qDebug() << "changeband " << band;
    } else if (object.value("action").toString() == "changescandelay") {
      currentScanDelay[nrig] = object.value("value").toInt() * 100 + 100;
      cbScanDelaySetIndex(nrig);
      //qDebug() << "changescandelay " << scanDelay;
      // update global scna delay
    } else if (object.value("action").toString() == "changelinked") {
      currentTrackedRadio[nrig] = object.value("value").toInt();
      cbLinkedSetIndex(nrig);
      //qDebug() << "changelinked " << linked;
    } else if (object.value("action").toString() == "getEllipseData") {
      updateGraphicsEllipse(nrig);
    } else if (object.value("action").toString() == "swapantennas") {
      swapAntennas(nrig);
    }
  }

}


void MainWindow::swapAntennas(int nrig)
{
  QString tmpGroupLabel_1 = QStringLiteral("");
  QString tmpGroupLabel_2 = QStringLiteral("");
  int displayMode_1 = kDispNone;
  int displayMode_2 = kDispNone;
  QSqlQuery query(db);
  query.exec(QString("SELECT * from groups where id = %1")
                .arg(currentGroup[nrig]));
  if (query.first()) {
    tmpGroupLabel_1 = query.value("label").toString();
    displayMode_1 = query.value("display_mode").toInt();
  }
  query.exec(QString("SELECT * from groups where id = %1")
                .arg(currentGroup[coChannel[nrig]]));
  if (query.first()) {
    tmpGroupLabel_2 = query.value("label").toString();
    displayMode_2 = query.value("display_mode").toInt();
  }

  setAntennaScanning(nrig, false);
  setAntennaTracking(nrig, false);
  setAntennaScanning(coChannel[nrig], false);
  setAntennaTracking(coChannel[nrig], false);
  int tmpBearing_1 = currentBearing[nrig];
  int tmpBearing_2 = currentBearing[coChannel[nrig]];
  int tmpAntenna_1 = currentAntenna[nrig];
  int tmpAntenna_2 = currentAntenna[coChannel[nrig]];
  int tmpGroup_1 = currentGroup[nrig];
  int tmpGroup_2 = currentGroup[coChannel[nrig]];

  currentBearing[nrig] = tmpBearing_2;
  //bearingChanged(nrig);
  currentGroup[nrig] = tmpGroup_2;
  //groupChanged(nrig);
  currentAntenna[nrig] = tmpAntenna_2;
  //antennaChanged(nrig);

  if (currentBearing[coChannel[nrig]] != tmpBearing_1) {
    currentBearing[coChannel[nrig]] = tmpBearing_1;
    bearingChanged(coChannel[nrig]);
  }
  if (currentGroup[coChannel[nrig]] != tmpGroup_1) {
    currentGroup[coChannel[nrig]] = tmpGroup_1;
    radioGroupLabel[coChannel[nrig]]->setText(tmpGroupLabel_1);
    cbGroupSetText(coChannel[nrig], tmpGroupLabel_1);
    //groupChanged(coChannel[nrig]);
    if (displayMode_1 == kDispList) {
      createAntennaButtons(coChannel[nrig]);
      setLayoutIndex(coChannel[nrig], kDispList);
    } else if (displayMode_1 == kDispCompass) {
      updateGraphicsLines(coChannel[nrig]);
      setLayoutIndex(coChannel[nrig], kDispCompass);
    }
  }
  currentAntenna[coChannel[nrig]] = tmpAntenna_1;
  antennaChanged(coChannel[nrig]);
  /* // handled by antennaChanged
  if (displayMode_1 == kDispList) {
    updateAntennaButtonsSelection(coChannel[nrig]);
    pbScanEnabledStatus(coChannel[nrig]);
  } else if (displayMode_1 == kDispCompass) {
    updateGraphicsEllipse(coChannel[nrig]);
    updateGraphicsLabels(coChannel[nrig]);
    pbScanEnabledStatus(coChannel[nrig]);
  }
  */

  //currentBearing[nrig] = tmpBearing_2;
  if (tmpBearing_1 != tmpBearing_2) {
    bearingChanged(nrig);
  }
  //currentGroup[nrig] = tmpGroup_2;
  if (tmpGroup_1 != tmpGroup_2) {
    radioGroupLabel[nrig]->setText(tmpGroupLabel_2);
    cbGroupSetText(nrig, tmpGroupLabel_2);
    if (displayMode_2 == kDispList) {
      createAntennaButtons(nrig);
      setLayoutIndex(nrig, kDispList);
    } else if (displayMode_2 == kDispCompass) {
      updateGraphicsLines(nrig);
      setLayoutIndex(nrig, kDispCompass);
    }
  }
  //groupChanged(nrig);
  //currentAntenna[nrig] = tmpAntenna_2;
  antennaChanged(nrig);
  /* // handled by antennaChanged
  if (displayMode_2 == kDispList) {
    updateAntennaButtonsSelection(nrig);
    pbScanEnabledStatus(nrig);
  } else if (displayMode_2 == kDispCompass) {
    updateGraphicsEllipse(nrig);
    updateGraphicsLabels(nrig);
    pbScanEnabledStatus(nrig);
  }
  */
}

void MainWindow::antennaChanged(int nrig)
{
  //qDebug() << "antennaChanged: Radio " << nrig+1 << " Antenna: " <<  currentAntenna[nrig] << " Bearing: " << currentBearing[nrig];

  int cat_id = 0;
  int hpf = 0;
  int bpf = 0;
  int gain = settings->value(s_radioGain[nrig], s_radioGain_def).toInt();
  int display_mode = getDisplayMode(currentGroup[nrig]);
  int display_mode_coChannel = getDisplayMode(currentGroup[coChannel[nrig]]);

  QSqlQuery query(db);
  query.exec(QString("SELECT * from bands where id = %1")
                  .arg(currentBand[nrig]));
  if (query.first()) {
    cat_id = query.value("cat_id").toInt();
    if (settings->value(s_radioBpf[nrig], s_radioBpf_def).toBool()) {
      bpf = query.value("bpf").toInt();
    }
    if (settings->value(s_radioHpf[nrig], s_radioHpf_def).toBool()) {
      hpf = query.value("hpf").toInt();
    }
    gain += query.value("gain").toInt();
  }

  QString data;
  data.append(QString("DATA 0 %1 %2 0 ")
                      .arg(nrig+1)
                      .arg(cat_id) ); // msg type, addressee, radio number,
                                      // band id, bearing (not needed)

  query.exec(QString("SELECT * from antennas where id = %1")
                .arg(currentAntenna[nrig]));
  if (query.first()) {
    radioAntennaLabel[nrig]->setText(query.value("label").toString());
    gain += query.value("gain").toInt();
    data.append(QString("%1 %2 %3 %4 %5\r")
                      .arg(query.value("switch_port").toInt())
                      .arg(query.value("vant").toInt())
                      .arg(gain)
                      .arg(hpf)
                      .arg(bpf) );

  } else {
    radioAntennaLabel[nrig]->setText("");
    data.append("0 0 0 0 0\r");
  }

  rs485SendData(data.toUtf8());

  //if (currentGain[nrig] != gain) {
    currentGain[nrig] = gain;
    lbGainSetText(nrig);
  //}

  // update visuals

  if (currentGroup[nrig]) {
    if (display_mode == kDispList) {
      updateAntennaButtonsSelection(nrig);
    } else if (display_mode == kDispCompass) {
      updateGraphicsEllipse(nrig);
      updateGraphicsLabels(nrig);
    }
    pbScanEnabledStatus(nrig);
  }
  if (currentGroup[coChannel[nrig]]) {
    if (display_mode_coChannel == kDispList) {
      updateAntennaButtonsSelection(coChannel[nrig]);
    } else if (display_mode_coChannel == kDispCompass) {
      updateGraphicsEllipse(coChannel[nrig]);
      updateGraphicsLabels(coChannel[nrig]);
    }
  }
  //bearingLabelText(nrig);
  cbGroupAddItems(coChannel[nrig]); // update co-channel group box
}

void MainWindow::lbHpfSetText(int nrig, QWebSocket *pClient)
{
  int hpf = 0;
  QSqlQuery query(db);
  query.exec(QString("SELECT * from bands where id = %1")
                .arg(currentBand[nrig]));
  if (query.first()) {
    if (settings->value(s_radioHpf[nrig], s_radioHpf_def).toBool()) {
      hpf = query.value("hpf").toInt();
    }
  }
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("hpfLabel"));
  object.insert("method", QJsonValue::fromVariant("setText"));
  if (hpf) object.insert("text", QJsonValue::fromVariant("HPF"));
  else object.insert("text", QJsonValue::fromVariant(""));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::lbBpfSetText(int nrig, QWebSocket *pClient)
{
  int bpf = 0;
  QSqlQuery query(db);
  query.exec(QString("SELECT * from bands where id = %1")
                  .arg(currentBand[nrig]));
  if (query.first()) {
    if (settings->value(s_radioBpf[nrig], s_radioBpf_def).toBool()) {
      bpf = query.value("bpf").toInt();
    }
  }
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("bpfLabel"));
  object.insert("method", QJsonValue::fromVariant("setText"));
  if (bpf) object.insert("text", QJsonValue::fromVariant("BPF"));
  else object.insert("text", QJsonValue::fromVariant(""));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::lbGainSetText(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("gainLabel"));
  object.insert("method", QJsonValue::fromVariant("setText"));
  if (currentAntenna[nrig]) {
    object.insert("text", QJsonValue::fromVariant(QString::number(currentGain[nrig])+"dB"));
  } else {
    object.insert("text", QJsonValue::fromVariant(""));
  }
  sendRadioWindowData(nrig, object, pClient);
}

int MainWindow::getAux(int nrig)
{
  int aux = 0;
  if (settings->value(s_radioAux[nrig], s_radioAux_def).toBool()) {
    QSqlQuery query(db);
    query.exec(QString("select aux from bands where id = %1")
                  .arg(currentBand[nrig]));
    if (query.first()) {
      aux = query.value("aux").toInt();
    }
  }
  return aux;
}

void MainWindow::lbAuxSetText(int nrig, QWebSocket *pClient)
{
  int aux = getAux(nrig);
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("auxLabel"));
  object.insert("method", QJsonValue::fromVariant("setText"));

  if (aux == 2) object.insert("text", QJsonValue::fromVariant("AUX2"));
  else if (aux == 1) object.insert("text", QJsonValue::fromVariant("AUX"));
  else object.insert("text", QJsonValue::fromVariant(""));

  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::toggleAntennaTracking(int nrig)
{
  bool restorePrevious = false;
  if (trackingState[nrig]) restorePrevious = true;

  setAntennaTracking(nrig, !trackingState[nrig]);

  if (restorePrevious) {

    // check for collision if tracked radio chose our last antenna

    if (currentBearing[nrig] != prevBearingTrack[nrig]) {
      currentBearing[nrig] = prevBearingTrack[nrig];
      bearingChanged(nrig);
    }
    if (currentGroup[nrig] != prevGroupTrack[nrig]) {
      currentGroup[nrig] = prevGroupTrack[nrig];
      QString label = prevGroupLabel[nrig];
      int display_mode = getDisplayMode(currentGroup[nrig]);
      QSqlQuery query(db);
      query.exec(QString("select * from groups where id = %1")
                    .arg(currentGroup[nrig]));
      if (query.first()) {
        label = query.value("label").toString(); // in case name changed
      }
      radioGroupLabel[nrig]->setText(label);
      cbGroupSetText(nrig, label);
      //groupChanged(nrig);
      if (display_mode == kDispList) {
        createAntennaButtons(nrig);
        setLayoutIndex(nrig, kDispList);
      } else if (display_mode == kDispCompass) {
        updateGraphicsLines(nrig);
        setLayoutIndex(nrig, kDispCompass);
      } else {
        setLayoutIndex(nrig, kDispNone);
      }
    }
    if (currentAntenna[nrig] != prevAntennaTrack[nrig]) {
      currentAntenna[nrig] = prevAntennaTrack[nrig];
      selectAntenna(nrig); // call this to avoid collision
      if (currentAntenna[nrig] == prevAntennaTrack[nrig]) { // selectAntenna didnt change
        antennaChanged(nrig);
      }
    }
  }
}

void MainWindow::setAntennaTracking(int nrig, bool state)
{

  if (trackingState[nrig] != state) {
    //qDebug() << "setAntennaTracking, radio " << nrig << " state " << state;
    //trackingState[nrig] = state;
    if (state) {

      // don't track a radio if it's track us
      if (trackingState[currentTrackedRadio[nrig]] &&
          currentTrackedRadio[currentTrackedRadio[nrig]] == nrig ) {
        return;
      }
      // don't track if other radio not in compass group
      int display_mode = getDisplayMode(currentGroup[currentTrackedRadio[nrig]]);

      if (display_mode != kDispCompass) return;

      // get all groups not same as tracked radio, enabled, compass mode
      // check all antennas in group for collisions, using tracked radio antenna center bearings
      // choose group or abort
      // set antenna based on tracked radio's bearing
      // add tracking code to antennaChanged so tracked radio updates tracker
      QSqlQuery query(db);
      QString sql,match,sort;
      if (nrig < 4) {
        match.append(" and groups.radios1_4 = 1");
      } else {
        match.append(" and groups.radios5_8 = 1");
      }
      match.append(QString(" and groups.display_mode = 1 and groups.id <> %1")
                          .arg(currentGroup[currentTrackedRadio[nrig]]) );
      sort.append(" order by groups.priority desc, groups.id desc");
      sql.append(kSelectGroupSQL.arg(currentBand[nrig])
                                .arg(match)
                                .arg(sort)
                                .arg("") );

      //qDebug() << sql;
      query.exec(sql);
      bool found = false;
      while (!found && query.next()) {
        if (currentGroup[nrig] == query.value("id").toInt()) {
          found = true; // keep current group if usable for tracking
        }
      }
      if (!found) {
        query.seek(QSql::BeforeFirstRow);
        if (query.next()) { // use highest priority group
          found = true;
        }
      }
      if (found) {
        // found a group, how to verify no collisions?
        setAntennaScanning(nrig, false);
        trackingState[nrig] = true;
        prevGroupTrack[nrig] = currentGroup[nrig];
        prevGroupLabel[nrig] = radioGroupLabel[nrig]->text();
        prevAntennaTrack[nrig] = currentAntenna[nrig];
        prevBearingTrack[nrig] = currentBearing[nrig];
        if (currentBearing[nrig] != currentBearing[currentTrackedRadio[nrig]]) {
          currentBearing[nrig] = currentBearing[currentTrackedRadio[nrig]];
          bearingChanged(nrig);
        }
        //qDebug() << "Prev: " << prevGroup[nrig] << " Current: " << currentGroup[nrig];
        //prevAntenna[nrig] = currentAntenna[nrig];
        //prevGroup[nrig] = currentGroup[nrig];
        if (currentGroup[nrig] != query.value("id").toInt()) {
          currentGroup[nrig] = query.value("id").toInt();
          //currentAntenna[nrig] = 0; // force antenna update
          radioGroupLabel[nrig]->setText(query.value("label").toString());
          cbGroupSetText(nrig, query.value("label").toString());
          groupChanged(nrig);
        } else { // group didn't change
          selectAntenna(nrig);
        }

        pbTrackStatus(nrig, true);
        cbLinkedSetEnabled(nrig, false);

      } else {
        trackingState[nrig] = false;
        pbTrackStatus(nrig, false);
        cbLinkedSetEnabled(nrig, true);
      }

    } else {
      //qDebug() << "Prev: " << prevGroup[nrig] << " Current: " << currentGroup[nrig];
      //currentGroup[nrig] = prevGroup[nrig];
      //currentAntenna[nrig] = prevAntenna[nrig];
      //groupChanged(nrig);
      trackingState[nrig] = false;
      pbTrackStatus(nrig, false);
      cbLinkedSetEnabled(nrig, true);
      // restore antenna prior to tracking
      /*
      currentGroup[nrig] =   prevGroupTrack[nrig];
      radioGroupLabel[nrig]->setText(prevGroupLabel[nrig]);
      cbGroupSetText(nrig, prevGroupLabel[nrig]);
      //currentAntenna[nrig] = prevAntennaTrack[nrig];
      currentBearing[nrig] = prevBearingTrack[nrig];
      bearingChanged(nrig);
      groupChanged(nrig);
      */
    }
  }
}

bool MainWindow::selectAntenna(int nrig, bool makeChanges, int newGroup)
{
  int group = currentGroup[nrig];
  if (!makeChanges) {
    group = newGroup;
  }

  int display_mode = getDisplayMode(group); // list mode
  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio
  QSqlQuery query(db);
  QString sql, match, sort;
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  if (display_mode == kDispCompass) {
    sort.append(" order by antennas.start_deg asc");
  } else {
    sort.append(" order by antennas.priority desc, antennas.id asc");
  }
  sql.append(kSelectAntennaSQL.arg(group)
                              .arg(match)
                              .arg(sort) );

  //qDebug() << sql;
  bool found = false;
  query.exec(sql);

  while (!found && (!trackingState[nrig] || !makeChanges) && query.next()) { // skip if in tracking mode
    if (query.value("switch_port").toInt() != coChannelPort) {
      if (query.value("id").toInt() == currentAntenna[nrig]) { // keep current antenna
        found = true;
      }
    }
  }

  query.seek(QSql::BeforeFirstRow);
  // search here for antenna that covers current bearing
  if (currentBearing[nrig] >= 0) { // && display_mode == kDispCompass) {
    while (!found && query.next()) {
      if (query.value("switch_port").toInt() != coChannelPort) {
        if (currentBearing[nrig] >= query.value("start_deg").toInt() &&
            currentBearing[nrig] <= query.value("stop_deg").toInt() ) {
          found = true;
        } else if (query.value("stop_deg").toInt() < query.value("start_deg").toInt()) {
          if (currentBearing[nrig] >= query.value("start_deg").toInt() &&
              currentBearing[nrig] <= query.value("stop_deg").toInt() + 360) {
            found = true;
          } else if (currentBearing[nrig] >= query.value("start_deg").toInt() - 360 &&
                     currentBearing[nrig] <= query.value("stop_deg").toInt()) {
            found = true;
          }
        }
        if (found && makeChanges) {
          //qDebug() << "found new antenna covering current bearing";
          if (currentAntenna[nrig] != query.value("id").toInt()) {
            currentAntenna[nrig] = query.value("id").toInt();
            antennaChanged(nrig);
          }
        }
      }
    }
  }

  query.seek(QSql::BeforeFirstRow);
  while (!found && (!trackingState[nrig] || !makeChanges) && query.next()) {
    if (query.value("switch_port").toInt() != coChannelPort) {
      //qDebug() << "found new antenna via priority search";
      found = true;
      if (makeChanges) {
        currentAntenna[nrig] = query.value("id").toInt();
        int bearing = calcCenterBearing(query.value("start_deg").toInt(), query.value("stop_deg").toInt());
        if (currentBearing[nrig] != bearing) {
          currentBearing[nrig] = bearing;
          bearingChanged(nrig);
        }
        antennaChanged(nrig);
      }
    }
  }

  if (!found && !trackingState[nrig] && makeChanges) { // no antennas this group
    //qDebug() << "no antennas found";
    if (currentAntenna[nrig] != 0) {
      currentAntenna[nrig] = 0;
      currentBearing[nrig] = -1;
      bearingChanged(nrig);
      antennaChanged(nrig);
    }
  }
  return found;
}

/**
 * groupChanged(int nrig) {}
 * this gets called anytime currentGroup[] is
 * updated by the caller
 */
void MainWindow::groupChanged(int nrig)
{
  //qDebug() << "groupChanged: Radio " << nrig+1 << " Group: " <<  currentGroup[nrig];

  setAntennaScanning(nrig, false);

  int display_mode = getDisplayMode(currentGroup[nrig]);
  int display_mode_coChannel = getDisplayMode(currentGroup[coChannel[nrig]]);

  // unset any radios tracking us
  for (int i=0; i<NRIG; ++i){
    //qDebug() << "i " << i;
    if (trackingState[i] && currentTrackedRadio[i] == nrig) { // a radio is tracking us
      //qDebug() << "tracking me ";
      if (display_mode == kDispList || currentGroup[i] == currentGroup[nrig] ) {
        //qDebug() << "list mode or same group";
        setAntennaTracking(i, false);
        //qDebug() << "unset tracking";
      }
    }
  }


  // do group visual setup
  // graphic selections handled in antennaChanged()

  if (currentGroup[nrig] > 0) {
    if (display_mode == kDispList) {
      createAntennaButtons(nrig);
    } else if (display_mode == kDispCompass) {
      updateGraphicsLines(nrig);
      //updateGraphicsLabels(nrig);
      //updateGraphicsLabels(coChannel[nrig]);
    }
  }

  int tmpAntenna = currentAntenna[nrig];

  selectAntenna(nrig);

  if (currentGroup[nrig]) {
    if (display_mode == kDispList) {
      if (tmpAntenna == currentAntenna[nrig]) { // not changed by selectAntenna
        updateAntennaButtonsSelection(nrig);
        updateAntennaButtonsSelection(coChannel[nrig]);
        pbScanEnabledStatus(nrig);
      }
      setLayoutIndex(nrig, kDispList);
    } else if (display_mode == kDispCompass) {
      if (tmpAntenna == currentAntenna[nrig]) {  // not changed by selectAntenna
        updateGraphicsEllipse(nrig);
        updateGraphicsEllipse(coChannel[nrig]);
        updateGraphicsLabels(nrig);
        updateGraphicsLabels(coChannel[nrig]);
        pbScanEnabledStatus(nrig);
      }
      setLayoutIndex(nrig, kDispCompass);
    } else {
      setLayoutIndex(nrig, kDispNone);
      pbScanEnabledStatus(nrig);
    }
  } else {
    setLayoutIndex(nrig, kDispNone);
    pbScanEnabledStatus(nrig);
  }
  if (tmpAntenna == currentAntenna[nrig]) { // not changed by selectAntenna
    if (currentGroup[coChannel[nrig]]) {
      if (display_mode_coChannel == kDispList) {
        updateAntennaButtonsSelection(coChannel[nrig]);
      } else if (display_mode_coChannel == kDispCompass) {
        updateGraphicsEllipse(coChannel[nrig]);
        updateGraphicsLabels(coChannel[nrig]);
      }
    }
  }

}



void MainWindow::bearingChanged(int nrig)
{
  bearingLabelText(nrig);
  //tracking
  for (int i=0; i<NRIG; ++i){
    if (trackingState[i] && currentTrackedRadio[i] == nrig) { // a radio is tracking us
      if (currentBearing[i] != currentBearing[nrig]) {
        currentBearing[i] = currentBearing[nrig];
        bearingChanged(i); // in case someone tracking him too
        //currentAntenna[i] = 0; // bombards rs485 with data
        //groupChanged(i); // will select new antenna per new bearing
        selectAntenna(i);
      }
    }
  }
}

void MainWindow::bearingChangedMouse(int nrig, int bearing)
{
  setAntennaScanning(nrig, false);

  //if (trackingState[nrig]) {
    setAntennaTracking(nrig, false);
  //}

  if (currentBearing[nrig] != bearing) {
    currentBearing[nrig] = bearing;
    bearingChanged(nrig);
  }

  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio

  QSqlQuery query(db);
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  sort.append(" order by antennas.start_deg asc");
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  //qDebug() << sql;
  bool found = false;
  query.exec(sql);
  while (!found && query.next()) {
    if (query.value("switch_port").toInt() != coChannelPort) {
      if (bearing >= query.value("start_deg").toInt() &&
          bearing <= query.value("stop_deg").toInt() ) {
        found = true;
      } else if (query.value("stop_deg").toInt() < query.value("start_deg").toInt()) {
        if (bearing >= query.value("start_deg").toInt() &&
            bearing <= query.value("stop_deg").toInt() + 360) {
          found = true;
        } else if (bearing >= query.value("start_deg").toInt() - 360 &&
                   bearing <= query.value("stop_deg").toInt()) {
          found = true;
        }
      }
    }
  }
  if (found) {
    if (currentAntenna[nrig] != query.value("id").toInt()) {
      currentAntenna[nrig] = query.value("id").toInt();
      antennaChanged(nrig);
    }
  } // no bearing match, keep current antenna

}


void MainWindow::updateGraphicsLabels(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("GraphicsLabels"));
  object.insert("method", QJsonValue::fromVariant("update"));
  QJsonArray labels;
  //qDebug() << "updateGraphicsLabels start";
  QSqlQuery query(db);
  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  sort.append(" order by antennas.start_deg asc");
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  //qDebug() << sql;
  query.exec(sql);
  int angle;
  while (query.next()) {
    QJsonObject attributes;
    angle = calcCenterBearing(query.value("start_deg").toInt(), query.value("stop_deg").toInt());
    attributes.insert("angle", QJsonValue::fromVariant(angle));
    attributes.insert("text", QJsonValue::fromVariant(query.value("label").toString()));
    if (currentAntenna[nrig] == query.value("id").toInt()) {
      attributes.insert("state", QJsonValue::fromVariant(kSelected));
    } else if (query.value("switch_port").toInt() == coChannelPort) {
      attributes.insert("state", QJsonValue::fromVariant(kUnavailable));
    } else {
      attributes.insert("state", QJsonValue::fromVariant(kAvailable));
    }
    labels.push_back(attributes);
  }
  //qDebug() << "updateGraphicsLabels end";
  object.insert("labels", QJsonValue(labels));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::updateGraphicsLines(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("GraphicsLines"));
  object.insert("method", QJsonValue::fromVariant("update"));
  QJsonArray angles;
  //qDebug() << "updateGraphicsLines start";
  QSqlQuery query(db);
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  sort.append(" order by antennas.start_deg asc");
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  //qDebug() << sql;
  query.exec(sql);
  while (query.next()) {
    angles.push_back(QJsonValue::fromVariant(query.value("start_deg").toInt()));
    angles.push_back(QJsonValue::fromVariant(query.value("stop_deg").toInt()));
  }
  //qDebug() << "updateGraphicsLines end";
  object.insert("angles", QJsonValue(angles));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::updateGraphicsEllipse(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("GraphicsEllipse"));
  object.insert("method", QJsonValue::fromVariant("update"));
  QJsonArray ellipses;

  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio

  QSqlQuery query(db);
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  sort.append(" order by antennas.start_deg asc");
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  query.exec(sql);
  while (query.next()) {
    QJsonObject attributes;
    if (currentAntenna[nrig] == query.value("id").toInt()) {
      attributes.insert("start_deg", QJsonValue::fromVariant(query.value("start_deg").toInt()));
      attributes.insert("stop_deg", QJsonValue::fromVariant(query.value("stop_deg").toInt()));
      attributes.insert("state", QJsonValue::fromVariant(kSelected));
      ellipses.push_back(attributes);
    } else if (query.value("switch_port").toInt() == coChannelPort) {
      attributes.insert("start_deg", QJsonValue::fromVariant(query.value("start_deg").toInt()));
      attributes.insert("stop_deg", QJsonValue::fromVariant(query.value("stop_deg").toInt()));
      attributes.insert("state", QJsonValue::fromVariant(kUnavailable));
      ellipses.push_back(attributes);
    }

  }
  //qDebug() << "updateGraphicsLabels end";
  object.insert("ellipses", QJsonValue(ellipses));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::setLayoutIndex(int nrig, int idx, QWebSocket *pClient)
{
  /**
   * kDispList=0
   * kDispCompass=1
   * kDispNone=2
   */
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("StackedLayout"));
  object.insert("method", QJsonValue::fromVariant("setCurrentIndex"));
  object.insert("index", QJsonValue::fromVariant(idx));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::updateAntennaButtonsSelection(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("AntennaButtons"));
  object.insert("method", QJsonValue::fromVariant("update"));
  QJsonArray buttons;

  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio

  QSqlQuery query(db);
  QString sql,match,sort;
  // radio range
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  sort.append(" order by antennas.start_deg asc");
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  query.exec(sql);
  while (query.next()) {
    QJsonObject attributes;
    attributes.insert("antenna", QJsonValue::fromVariant(query.value("id").toInt()));
    if (currentAntenna[nrig] == query.value("id").toInt()) {
      attributes.insert("state", QJsonValue::fromVariant(kSelected));
    } else if (query.value("switch_port").toInt() == coChannelPort) {
      attributes.insert("state", QJsonValue::fromVariant(kUnavailable));
    } else {
      attributes.insert("state", QJsonValue::fromVariant(kAvailable));
    }
    buttons.push_back(attributes);
  }
  //qDebug() << "updateGraphicsLabels end";
  object.insert("buttons", QJsonValue(buttons));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::createAntennaButtons(int nrig, QWebSocket *pClient)
{
  //qDebug() << "buttons";

  int display_mode = getDisplayMode(currentGroup[nrig]);

  //qDebug() << "display mode " << display_mode;
  if (display_mode == kDispList) {
    QJsonObject object;
    object.insert("object", QJsonValue::fromVariant("AntennaButtons"));
    object.insert("method", QJsonValue::fromVariant("create"));
    QJsonArray buttons;
    QSqlQuery query(db);
    QString sql,match,sort;
    if (nrig < 4) {
      match.append(" and antennas.radios1_4 = 1");
    } else {
      match.append(" and antennas.radios5_8 = 1");
    }
    if (display_mode == kDispCompass) {
      sort.append(" order by antennas.start_deg asc");
    } else {
      sort.append(" order by antennas.priority desc, antennas.id asc");
    }
    sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                                .arg(match)
                                .arg(sort) );

    //qDebug() << sql;
    query.exec(sql);
    while (query.next()) {
      QJsonObject attributes;
      attributes.insert("label", QJsonValue::fromVariant(query.value("label").toString()));
      attributes.insert("antenna", QJsonValue::fromVariant(query.value("id").toInt()));
      buttons.push_back(attributes);
    }

    object.insert("buttons", QJsonValue::fromVariant(buttons));
    sendRadioWindowData(nrig, object, pClient);
  }
}

void MainWindow::antennaButtonClicked(int nrig, int antenna)
{
  //qDebug() << "Radio " << nrig+1 << " antenna " << antenna;

  setAntennaScanning(nrig, false);

  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio
  int switch_port = getSwitchPort(antenna);

  if (switch_port != coChannelPort) {
    if (currentAntenna[nrig] != antenna) {
      currentAntenna[nrig] = antenna;
      int bearing = 0;
      QSqlQuery query(db);
      query.exec(QString("SELECT start_deg,stop_deg from antennas where id = %1")
                      .arg(antenna) );
      if (query.first()) {
        bearing = calcCenterBearing(query.value("start_deg").toInt(), query.value("stop_deg").toInt());
      }
      if (currentBearing[nrig] != bearing) {
        currentBearing[nrig] = bearing;
        bearingChanged(nrig);
      }
      antennaChanged(nrig);
    }
  }
}




void MainWindow::timeoutMainTimer()
{
  for (int i=0;i<NRIG;++i) {

    if (settings->value(s_radioEnable[i], s_radioEnable_def).toBool()) {
      int freq = 0;
      int ptt = 0;
      int mainRx = 0;
      bool catOpen = false;

      if (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt() == kCat) {
        freq = cat[i]->getRigFreq();
        ptt = cat[i]->getRigPtt();
        catOpen = cat[i]->radioOpen();
        if (radioConnected[i] != catOpen) { // CAT state changed
          //qDebug() << "timeoutMainTimer: cat connection changed " << i;
          radioConnected[i] = catOpen;
          radioConnectionStatus(i, radioConnected[i]);
        }
      }
      if (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt() == kManual) {
        if (!radioConnected[i]) {
          radioConnected[i] = true;
          radioConnectionStatus(i, radioConnected[i]);
        }
      }

      bool ptt_tmp;
      switch (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt()) {
        case kSubRx:
          mainRx = settings->value(s_radioSubRxNr[i], s_radioSubRxNr_def).toInt();
          ptt_tmp = currentPtt[mainRx];
          break;
        case kCat:
          ptt_tmp = ptt;
          break;
        case kManual:
        default:
          ptt_tmp = 0;
          break;
      }
      if (currentPtt[i] != ptt_tmp) { // ptt state changed
        //qDebug() << "timeoutMainTimer: ptt state changed " << i;
        currentPtt[i] = ptt_tmp;
        lbPttState(i);
        if (currentPtt[i]) {
          radioPttLabel[i]->setText("TX");
          radioPttLabel[i]->setStyleSheet("QLabel { color : red; font-weight:600; }");
        } else {
          radioPttLabel[i]->setText("RX");
          radioPttLabel[i]->setStyleSheet("QLabel { color : green; font-weight:600; }");
        }
      }


      if (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt() == kCat) {
        if (currentFreq[i] != freq) { // freq changed
          int iFreqKhz = freq / 1000;
          float fFreqKhz = freq / 1000.0;
          if (freq) {
            radioFreqLabel[i]->setText(QString::number(fFreqKhz, 'f', 2));
          } else {
            radioFreqLabel[i]->setText("");
          }
          QSqlQuery query(db);
          query.exec("SELECT * from bands ORDER BY start_freq");
          bool found = false;
          while (!found && query.next()) {
            if ( iFreqKhz >= query.value("start_freq").toInt() && iFreqKhz <= query.value("stop_freq").toInt()) {
              found = true;
              if (currentBand[i] != query.value("id").toInt()) { // band changed
                radioBandLabel[i]->setText(query.value("name").toString());
                cbBandSetText(i, query.value("name").toString());
                currentBand[i] = query.value("id").toInt();
                //qDebug() << "timeoutMainTimer: cat band change found";
                bandChanged(i);
                //break;
              }
            }
          }
          if (!found) { // no matching band definition
            if (currentBand[i] != 0) {
              currentBand[i] = 0;
              radioBandLabel[i]->setText("");
              cbBandSetText(i, QStringLiteral(""));
              //qDebug() << "timeoutMainTimer: cat band change not found";
              bandChanged(i);
            }
          }
          currentFreq[i] = freq;
        }
      }

      if (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt() == kSubRx) {
        catOpen = radioConnected[settings->value(s_radioSubRxNr[i], s_radioSubRxNr_def).toInt()];
        if (radioConnected[i] != catOpen) {
          radioConnected[i] = catOpen;
          radioConnectionStatus(i, radioConnected[i]);
        }
        if (currentBand[i] != currentBand[mainRx]) {
          currentBand[i] = currentBand[mainRx];
          radioBandLabel[i]->setText(radioBandLabel[mainRx]->text());
          cbBandSetText(i, radioBandLabel[mainRx]->text());
          //qDebug() << "timeoutMainTimer: subrx band change";
          bandChanged(i);
        }
      }
    }

    // scanning
    if (scanState[i]) {
      if (scanCount[i] >= currentScanDelay[i] / timerPeriod) {
        if (!settings->value(s_radioPauseScan[i], s_radioPauseScan_def).toBool() || !currentPtt[i]) {
          antennaStep(i, kNext, true);
          scanCount[i] = 0;
        }
      } else {
        scanCount[i]++;
      }
    }

  } // end nrig loop


}





void MainWindow::rs485RcvdData ()
{
  if (rs485Serial->isOpen()) {
    while(rs485Serial->canReadLine()) {
      QString text = QString::fromUtf8(rs485Serial->readLine());
      rs485RcvdDataLabel->setText(": " + text);
    }
  }
}

void MainWindow::rs485SendData (QByteArray data)
{

  //qDebug() << QString::fromUtf8(data);
  //rs485RcvdDataLabel->setText(QString::fromUtf8(data));
  //return;

  if (rs485Serial->isOpen()) {
    if(rs485Serial->write(data)) {
      rs485RcvdDataLabel->setText(QString::fromUtf8(data));
      serialLog->appendPlainText("[" + QDateTime::currentDateTime().toString("hh:mm:ss")
                                 + "] " + QString::fromUtf8(data).simplified());
    } else {
      statusBarUi->showMessage("RS485 send error", tmpStatusMsgDelay);
    }
  }
}







void MainWindow::writeSettings()
{
  settings->setValue("rs485Port", rs485PortComboBox->currentText());
  settings->setValue("rs485auto", rs485AutoConnect->isChecked());
  settings->setValue("websocketPort", websocketPortLineEdit->text());

  for (int i=0;i<NRIG;++i) {
    settings->setValue(s_radioSerialPort[i], radioSerialPortComboBox[i]->currentText());
    settings->setValue(s_radioBaudRate[i], radioBaudRateComboBox[i]->currentText());
    settings->setValue(s_radioModel[i], cat[0]->hamlibModelIndex(radioManufComboBox[i]->currentIndex(), radioModelComboBox[i]->currentIndex()));
    settings->setValue(s_radioEnable[i], radioEnableCheckBox[i]->isChecked());
    settings->setValue(s_radioPauseScan[i], radioPauseScanCheckBox[i]->isChecked());
    settings->setValue(s_radioHpf[i], radioHpfCheckBox[i]->isChecked());
    settings->setValue(s_radioBpf[i], radioBpfCheckBox[i]->isChecked());
    settings->setValue(s_radioAux[i], radioAuxCheckBox[i]->isChecked());
    settings->setValue(s_radioBandDecoder[i], radioBandDecoderComboBox[i]->currentIndex());
    settings->setValue(s_rigctld[i], rigctldCheckbox[i]->isChecked());
    settings->setValue(s_radioName[i], radioNameLineEdit[i]->text());
    settings->setValue(s_radioScanDelay[i], currentScanDelay[i]);
    settings->setValue(s_radioGain[i], radioGainSpinBox[i]->value());
    settings->setValue(s_radioSubRxNr[i], radioSubRxSpinBox[i]->value() - 1);
    settings->setValue(s_radioPollTime[i], radioPollTimeLineEdit[i]->text());
    settings->setValue(s_rigctldIp[i], rigctldIpLineEdit[i]->text());
    settings->setValue(s_rigctldPort[i], rigctldPortLineEdit[i]->text());
    settings->setValue(s_radioTrackNr[i], currentTrackedRadio[i]);

  }

  settings->setValue("geometry", saveGeometry());
  settings->setValue("windowState", saveState());

  //settings->setValue();
  settings->sync();
  statusBarUi->showMessage("Settings saved to disk", tmpStatusMsgDelay);
}





void MainWindow::saveSettingsButtonClicked() {

  bool radioEnableChanged[NRIG]; // catch changes for processing later
  bool bandDecoderChanged[NRIG];
  bool hpfChanged[NRIG];
  bool bpfChanged[NRIG];
  bool auxChanged[NRIG];
  bool gainChanged[NRIG];
  for (int i=0; i<NRIG; ++i){
    radioEnableChanged[i] = false;
    bandDecoderChanged[i] = false;
    hpfChanged[i] = false;
    bpfChanged[i] = false;
    auxChanged[i] = false;
    gainChanged[i] = false;
    if (settings->value(s_radioEnable[i], s_radioEnable_def).toBool() != radioEnableCheckBox[i]->isChecked()) {
      radioEnableChanged[i] = true;
    }
    if (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt() != radioBandDecoderComboBox[i]->currentIndex()) {
      bandDecoderChanged[i] = true;
    }
    if (settings->value(s_radioBpf[i], s_radioBpf_def).toBool() != radioBpfCheckBox[i]->isChecked()) {
      bpfChanged[i] = true;
    }
    if (settings->value(s_radioHpf[i], s_radioHpf_def).toBool() != radioHpfCheckBox[i]->isChecked()) {
      hpfChanged[i] = true;
    }
    if (settings->value(s_radioAux[i], s_radioAux_def).toBool() != radioAuxCheckBox[i]->isChecked()) {
      auxChanged[i] = true;
    }
    if (settings->value(s_radioGain[i], s_radioGain_def).toInt() != radioGainSpinBox[i]->value()) {
      gainChanged[i] = true;
    }
  }

  writeSettings();
  statusPageInit();

  for (int i=0; i<NRIG; ++i){
    if (radioEnableChanged[i]) {
      if (!settings->value(s_radioEnable[i], s_radioEnable_def).toBool()) {

        int tmpBand = currentBand[i];
        currentFreq[i] = 0;
        currentBand[i] = 0;
        currentGroup[i] = 0;
        currentAntenna[i] = 0;
        currentBearing[i] = -1;
        bearingChanged(i);
        currentPtt[i] = false;
        currentGain[i] = 0;
        scanState[i] = false;
        trackingState[i] = false;
        setAntennaLock(i, false);
        radioConnected[i] = false;
        if (tmpBand != 0) { // band was changed
          radioBandLabel[i]->setText("");
          cbBandSetText(i, QStringLiteral(""));
          bandChanged(i);
        }
      }
    }
    if (bandDecoderChanged[i]) {
      scanState[i] = false;
      trackingState[i] = false;
      setAntennaLock(i, false);
      radioConnected[i] = false;
      switch (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt()) {
        case kCat:
          break;
        case kSubRx:
        case kManual:
        default:
          currentFreq[i] = 0;
          break;
      }
    }
    radioNameSetText(i);
    cbBandSetEnabled(i);
    if (bpfChanged[i] || hpfChanged[i] || gainChanged[i]) {
      antennaChanged(i); // force update of external equip. via rs485 msg
      lbHpfSetText(i);
      lbBpfSetText(i);
    }
    if (auxChanged[i]) {
      // send AUX update
      QString data;
      data.append(kAuxData.arg(i+1)
                          .arg(getAux(i)));
      rs485SendData(data.toUtf8());
      lbAuxSetText(i);
    }
    cbLinkedAddItems(i);
    cbLinkedSetIndex(i);
  }

  cronTableView->viewport()->repaint(); // updates radio names if changed
}




// DATABASE
void MainWindow::resetDatabase()
{
  QSqlQuery query(db);
  query.exec("DROP TABLE IF EXISTS groups");
  query.exec("DROP TABLE IF EXISTS antennas");
  query.exec("DROP TABLE IF EXISTS bands");
  query.exec("DROP TABLE IF EXISTS band_group_map");
  query.exec("DROP TABLE IF EXISTS group_antenna_map");

  initDatabase();
  statusBarUi->showMessage("Database tables reset", tmpStatusMsgDelay);
  bandsTableModel->select();
  while (bandsTableModel->canFetchMore()) {
    bandsTableModel->fetchMore();
  }
  groupsTableModel->select();
  while (groupsTableModel->canFetchMore()) {
    groupsTableModel->fetchMore();
  }
  antennasTableModel->select();
  while (antennasTableModel->canFetchMore()) {
    antennasTableModel->fetchMore();
  }
  band_groupTableModel->select();
  while (band_groupTableModel->canFetchMore()) {
    band_groupTableModel->fetchMore();
  }
  group_antennaTableModel->select();
  while (group_antennaTableModel->canFetchMore()) {
    group_antennaTableModel->fetchMore();
  }
  group_antennaTableModel->relationModel(1)->select();
  while (group_antennaTableModel->relationModel(1)->canFetchMore()) {
    group_antennaTableModel->relationModel(1)->fetchMore();
  }
  group_antennaTableModel->relationModel(2)->select();
  while (group_antennaTableModel->relationModel(2)->canFetchMore()) {
    group_antennaTableModel->relationModel(2)->fetchMore();
  }
  band_groupTableModel->relationModel(1)->select();
  while (band_groupTableModel->relationModel(1)->canFetchMore()) {
    band_groupTableModel->relationModel(1)->fetchMore();
  }
  band_groupTableModel->relationModel(2)->select();
  while (band_groupTableModel->relationModel(2)->canFetchMore()) {
    band_groupTableModel->relationModel(2)->fetchMore();
  }

  for (int i=0; i<NRIG; ++i){
    updateBandComboSelection(i);
  }

}

void MainWindow::addBand()
{
  bandsTableModel->insertRow(bandsTableModel->rowCount(QModelIndex()));
}
void MainWindow::addGroup()
{
  groupsTableModel->insertRow(groupsTableModel->rowCount(QModelIndex()));
}
void MainWindow::addAntenna()
{
  antennasTableModel->insertRow(antennasTableModel->rowCount(QModelIndex()));
}
void MainWindow::addBandGroup()
{
  band_groupTableModel->insertRow(band_groupTableModel->rowCount(QModelIndex()));
}
void MainWindow::addGroupAntenna()
{
  group_antennaTableModel->insertRow(group_antennaTableModel->rowCount(QModelIndex()));
}
void MainWindow::addCron()
{
  cronTableModel->insertRow(cronTableModel->rowCount(QModelIndex()));
}
void MainWindow::saveBand()
{
  if(bandsTableModel->submitAll()) {
    band_groupTableModel->relationModel(1)->select();
    while (band_groupTableModel->relationModel(1)->canFetchMore()) {
      band_groupTableModel->relationModel(1)->fetchMore();
    }
    band_groupTableModel->relationModel(2)->select();
    while (band_groupTableModel->relationModel(2)->canFetchMore()) {
      band_groupTableModel->relationModel(2)->fetchMore();
    }
    for (int i=0; i<NRIG; ++i) {
      cbBandAddItems(i);
      updateBandComboSelection(i);

      // misc forced updates
      antennaChanged(i); // bpf, hpf, gain
      lbHpfSetText(i);
      lbBpfSetText(i);
      // aux
      QString data;
      data.append(kAuxData.arg(i+1)
                          .arg(getAux(i)));
      rs485SendData(data.toUtf8());
      lbAuxSetText(i);
    }
  } else {
    statusBarUi->showMessage(bandsTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::saveGroup()
{
  if(groupsTableModel->submitAll()) {
    group_antennaTableModel->relationModel(1)->select();
    while (group_antennaTableModel->relationModel(1)->canFetchMore()) {
      group_antennaTableModel->relationModel(1)->fetchMore();
    }
    group_antennaTableModel->relationModel(2)->select();
    while (group_antennaTableModel->relationModel(2)->canFetchMore()) {
      group_antennaTableModel->relationModel(2)->fetchMore();
    }
    band_groupTableModel->relationModel(1)->select();
    while (band_groupTableModel->relationModel(1)->canFetchMore()) {
      band_groupTableModel->relationModel(1)->fetchMore();
    }
    band_groupTableModel->relationModel(2)->select();
    while (band_groupTableModel->relationModel(2)->canFetchMore()) {
      band_groupTableModel->relationModel(2)->fetchMore();
    }
    for (int i=0; i<NRIG; ++i) {
      cbGroupAddItems(i);
      groupChanged(i);
      // forced update
      antennaChanged(i); // gain
    }
  } else {
    statusBarUi->showMessage(groupsTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::saveAntenna()
{
  if (antennasTableModel->submitAll()) {
    group_antennaTableModel->relationModel(1)->select();
    while (group_antennaTableModel->relationModel(1)->canFetchMore()) {
      group_antennaTableModel->relationModel(1)->fetchMore();
    }
    group_antennaTableModel->relationModel(2)->select();
    while (group_antennaTableModel->relationModel(2)->canFetchMore()) {
      group_antennaTableModel->relationModel(2)->fetchMore();
    }
    cronTableView->viewport()->repaint(); // updates antenna names if changed
    for (int i=0; i<NRIG; ++i) {
      groupChanged(i); // fake change to propagate DB changes to visual elements
      // forced update
      antennaChanged(i); // gain
    }
  } else {
    statusBarUi->showMessage(antennasTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::saveBandGroup()
{
  if(band_groupTableModel->submitAll()) {
    for (int i=0; i<NRIG; ++i) {
      cbGroupAddItems(i);
      groupChanged(i);
    }
  } else {
    statusBarUi->showMessage(band_groupTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::saveGroupAntenna()
{
  if (group_antennaTableModel->submitAll()) {
    for (int i=0; i<NRIG; ++i) {
      groupChanged(i); // fake change to propagate DB changes to visual elements
    }
  } else {
    statusBarUi->showMessage(group_antennaTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::saveCron()
{
  if (cronTableModel->submitAll()) {
    //
  } else {
    statusBarUi->showMessage(cronTableModel->lastError().text(), tmpStatusMsgDelay);
  }
}
void MainWindow::removeBand()
{
  QModelIndexList indexes = bandsTableView->selectionModel()->selectedRows();
  for (int i = indexes.count(); i > 0; --i) {

    int bandId = bandsTableModel->record(indexes.at(i-1).row()).value("id").toInt();
    //qDebug() << "removeBand: band_id " << bandId;
    //delete records on relevant map table(s)
    QSqlQuery query(db);
    query.exec(QString("DELETE FROM band_group_map WHERE band_id = %1")
                  .arg(bandId));
    band_groupTableModel->select();
    while (band_groupTableModel->canFetchMore()) {
      band_groupTableModel->fetchMore();
    }

    bandsTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveBand(); // this updates band_groupTableModel foreign keys also
}
void MainWindow::removeGroup()
{
  QModelIndexList indexes = groupsTableView->selectionModel()->selectedRows();
  for (int i = indexes.count(); i > 0; --i) {

    int groupId = groupsTableModel->record(indexes.at(i-1).row()).value("id").toInt();
    //delete records on relevant map table(s) with same id
    QSqlQuery query(db);
    query.exec(QString("DELETE FROM band_group_map WHERE group_id = %1")
                  .arg(groupId));
    query.exec(QString("DELETE FROM group_antenna_map WHERE group_id = %1")
                  .arg(groupId));

    band_groupTableModel->select();
    while (band_groupTableModel->canFetchMore()) {
      band_groupTableModel->fetchMore();
    }
    group_antennaTableModel->select();
    while (group_antennaTableModel->canFetchMore()) {
      group_antennaTableModel->fetchMore();
    }

    groupsTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveGroup(); // this updates band_groupTableModel/group_antennaTableModel foreign keys also
}
void MainWindow::removeAntenna()
{
  QModelIndexList indexes = antennasTableView->selectionModel()->selectedRows();
  //foreach(QModelIndex rowIndex, rowList) {
  //  model->removeRow(roxIndex.row(), rowIndex.parent());
  //}
  for (int i = indexes.count(); i > 0; --i) {

    int antennaId = antennasTableModel->record(indexes.at(i-1).row()).value("id").toInt();
    //delete records on relevant map table(s) with same id
    QSqlQuery query(db);
    query.exec(QString("DELETE FROM group_antenna_map WHERE antenna_id = %1")
                    .arg(antennaId));

    group_antennaTableModel->select();
    while (group_antennaTableModel->canFetchMore()) {
        group_antennaTableModel->fetchMore();
    }

    antennasTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveAntenna(); // this updates group_antennaTableModel foreign keys also
}
void MainWindow::removeBandGroup()
{
  QModelIndexList indexes = band_groupTableView->selectionModel()->selectedRows();
  for (int i = indexes.count(); i > 0; --i) {
    band_groupTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveBandGroup();
}
void MainWindow::removeGroupAntenna()
{
  QModelIndexList indexes = group_antennaTableView->selectionModel()->selectedRows();
  for (int i = indexes.count(); i > 0; --i) {
    group_antennaTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveGroupAntenna();
}
void MainWindow::removeCron()
{
  QModelIndexList indexes = cronTableView->selectionModel()->selectedRows();
  for (int i = indexes.count(); i > 0; --i) {
    cronTableModel->removeRow( indexes.at(i-1).row(), QModelIndex());
  }
  saveCron();
}
// !DATABASE










void MainWindow::statusPageInit() {
  for (int i=0;i<NRIG;++i) {

    if (settings->value(s_radioEnable[i], s_radioEnable_def).toBool()) {
      radioNameLabel[i]->setText(settings->value(s_radioName[i], s_radioName_def).toString());
      //radioNameLabel[i]->setStyleSheet("QLabel { color : blue; font-weight:600; }");

      if (currentPtt[i]) {
        radioPttLabel[i]->setText("TX");
        radioPttLabel[i]->setStyleSheet("QLabel { color : red; font-weight:600; }");
      } else {
        radioPttLabel[i]->setText("RX");
        radioPttLabel[i]->setStyleSheet("QLabel { color : green; font-weight:600; }");
      }

      switch (settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt()) {
        case kCat:
          radioCatButton[i]->setEnabled(true);
          break;
        case kSubRx:
        case kManual:
        default:
          radioCatButton[i]->setEnabled(false);
          //radioFreqLabel[i]->setText("");
          break;
      }

    } else {
      radioNameLabel[i]->setText("");
      radioFreqLabel[i]->setText("");
      radioBandLabel[i]->setText("");
      radioPttLabel[i]->setText("");
      radioGroupLabel[i]->setText("");
      radioAntennaLabel[i]->setText("");
      radioCatButton[i]->setEnabled(false);
    }
  }
  rs485Label->setText("RS485 Serial (" + settings->value("rs485Port", "").toString() + ")");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  writeSettings();
  delete settings;
  delete errorBox;
  for (const auto &client : qAsConst(m_clients)) {
    //client.websocket->abort();
    client.websocket->deleteLater();
  }
  m_clients.clear();
  for (const auto &crontimer : qAsConst(cronTimers)) {
    crontimer->deleteLater();
  }
  cronTimers.clear();
  delete webSocketServer;
  db.close();
  QSqlDatabase::removeDatabase("QSQLITE");
  for (int i=0;i<NRIG;++i) {
    if (catThread[i]->isRunning()) {
      catThread[i]->quit();
      catThread[i]->wait();
    }
    cat[i]->deleteLater();
  }

  event->accept();
  exit ( 0 );
}

/**
 * bandChanged(int nrig) {}
 * this gets called anytime currentBand[] is
 * updated by the caller
 */
void MainWindow::bandChanged(int nrig)
{
  //qDebug() << "bandChanged: Radio " << nrig+1 << " Band: " <<  currentBand[nrig];
  // is currently selected group (antenna) valid on new band? if so leave alone
  // if current group is not valid, go to highest priority group for this band
  // also get called if radio disabled and need to clear switches

  // unlock on band change
  setAntennaLock(nrig, false);
  setAntennaScanning(nrig, false);
  setAntennaTracking(nrig, false);

  if (settings->value(s_radioEnable[nrig], s_radioEnable_def).toBool()) {

    int tmpAntenna = currentAntenna[nrig];
    cbGroupAddItems(nrig);
    lbHpfSetText(nrig);
    lbBpfSetText(nrig);
    if (tmpAntenna == currentAntenna[nrig]) { // antenna didn't change
      antennaChanged(nrig); // force hpf,bpf,gain updates via rs485
    }

  } else { // radio disabled
    // group should be set to 0, now clear group and antenna selections
    if (currentGroup[nrig] != 0) {
      cbGroupSetText(nrig, QStringLiteral(""));
      radioGroupLabel[nrig]->setText("");
      currentGroup[nrig] = 0;
      groupChanged(nrig);
      lbHpfSetText(nrig);
      lbBpfSetText(nrig);
    }
  }
  QString data;
  data.append(kAuxData.arg(nrig+1)
                      .arg(getAux(nrig)));
  rs485SendData(data.toUtf8());
  lbAuxSetText(nrig);
}


/**
 *  this gets called when database bands table changes
 *  ie. add,edit,delete and reset
 */
void MainWindow::updateBandComboSelection(int nrig)
{
  // unsetting band will have chain effect on group and antenna
  switch (settings->value(s_radioBandDecoder[nrig], s_radioBandDecoder_def).toInt()) {
    case kSubRx: // subRXs will follow linked RXs
      break;
    case kCat:
      {
        QSqlQuery query(db);
        query.exec("SELECT * from bands ORDER BY start_freq");
        bool found = false;
        int iFreqKhz = currentFreq[nrig] / 1000;
        while (!found && query.next()) {
          if ( iFreqKhz >= query.value("start_freq").toInt() && iFreqKhz <= query.value("stop_freq").toInt()) {
            found = true;
            radioBandLabel[nrig]->setText(query.value("name").toString());
            cbBandSetText(nrig, query.value("name").toString());
            if (currentBand[nrig] != query.value("id").toInt()) {
              currentBand[nrig] = query.value("id").toInt();
              bandChanged(nrig);
            }
            //break;
          }
        }
        if (!found) { // no matching band definition
          if (currentBand[nrig] != 0) {
            currentBand[nrig] = 0;
            radioBandLabel[nrig]->setText("");
            cbBandSetText(nrig, QStringLiteral(""));
            bandChanged(nrig);
          }
        }
        break;
      }
    case kManual: // keep same band index if exists, otherwise clear
    default:
      {
        QSqlQuery query(db);
        query.exec("SELECT * from bands");
        bool found = false;
        while (!found && query.next()) {
          if ( currentBand[nrig] == query.value("id").toInt()) {
            found = true;
            // only need to update label and combobox selection
            radioBandLabel[nrig]->setText(query.value("name").toString());
            cbBandSetText(nrig, query.value("name").toString());
            //break;
          }
        }
        if (!found) { // no matching band definition
          if (currentBand[nrig] != 0) {
            currentBand[nrig] = 0;
            radioBandLabel[nrig]->setText("");
            cbBandSetText(nrig, QStringLiteral(""));
            bandChanged(nrig);
          }
        }
        break;
      }
  }
}





// ================================================

void MainWindow::toggleScanEnabled(int nrig)
{
  bool enabled = false;
  QSqlQuery query(db);
  query.exec(QString("SELECT * from antennas where id = %1")
                .arg(currentAntenna[nrig]));
  if (query.first()) {
    enabled = query.value("scan").toBool();
  }
  // sql insert
  query.exec(QString("UPDATE antennas set 'scan' = %1 where id = %2")
                      .arg(!enabled)
                      .arg(currentAntenna[nrig]) );

  // reload antenna table view
  antennasTableModel->select();
  while (antennasTableModel->canFetchMore()) {
    antennasTableModel->fetchMore();
  }

  pbScanEnabledStatus(nrig);

}

void MainWindow::toggleAntennaScanning(int nrig)
{
  //bool goBack = false;
  //if (scanState[nrig]) goBack = true;
  setAntennaScanning(nrig, !scanState[nrig]);
  //if (goBack) {
  //  antennaPrev(nrig); // go back one antenna when stopping scan
  //}
}

void MainWindow::setAntennaScanning(int nrig, bool state)
{
  if (scanState[nrig] != state) {
    //qDebug() << "setAntennaScanning, radio " << nrig << " state " << state;

    if (state) {
      if (trackingState[nrig]) {
        setAntennaTracking(nrig, false);
        //currentAntenna[nrig] = 0;
        if (currentBearing[nrig] != prevBearingTrack[nrig]) {
          currentBearing[nrig] = prevBearingTrack[nrig];
          bearingChanged(nrig);
        }
        if (currentGroup[nrig] != prevGroupTrack[nrig]) {
          currentGroup[nrig] = prevGroupTrack[nrig];
          QString label = prevGroupLabel[nrig];
          int display_mode = getDisplayMode(currentGroup[nrig]);
          QSqlQuery query(db);
          query.exec(QString("select * from groups where id = %1")
                        .arg(currentGroup[nrig]));
          if (query.first()) {
            label = query.value("label").toString(); // in case name changed
          }
          radioGroupLabel[nrig]->setText(label);
          cbGroupSetText(nrig, label);
          //groupChanged(nrig);
          if (display_mode == kDispList) {
            createAntennaButtons(nrig);
            setLayoutIndex(nrig, kDispList);
          } else if (display_mode == kDispCompass) {
            updateGraphicsLines(nrig);
            setLayoutIndex(nrig, kDispCompass);
          } else {
            setLayoutIndex(nrig, kDispNone);
          }
        }
        if (currentAntenna[nrig] != prevAntennaTrack[nrig]) {
          currentAntenna[nrig] = prevAntennaTrack[nrig];
          selectAntenna(nrig); // call this to avoid collision
          if (currentAntenna[nrig] == prevAntennaTrack[nrig]) { // selectAntenna didnt change
            antennaChanged(nrig);
          }
        }
      }
    }
    scanState[nrig] = state;
    scanCount[nrig] = 0;
    pbScanStatus(nrig, state);
  }
}

void MainWindow::toggleAntennaLock(int nrig)
{
  setAntennaLock(nrig, !lockState[nrig]);
}

void MainWindow::setAntennaLock(int nrig, bool state)
{
  if (lockState[nrig] != state) {
    //qDebug() << "setAntennaLock, radio " << nrig << " state " << state;
    lockState[nrig] = state;
    if (state) {
      setAntennaTracking(nrig, false);
      setAntennaScanning(nrig, false);
    }
    pbLockStatus(nrig, state);
    cbGroupSetEnabled(nrig, !state);
    pbScanSetEnabled(nrig, !state);
    pbTrackSetEnabled(nrig, !state);
    cbLinkedSetEnabled(nrig, !state);
    cbScanDelaySetEnabled(nrig, !state);
    frameSetEnabled(nrig, !state);
  }
}

int MainWindow::calcCenterBearing (int start, int end)
{
  int bearing = start + ((end-start)/2);
  if (bearing < start && bearing > end) {
    bearing += 180;
  }
  if (bearing >= 360) {
    bearing -= 360;
  }
  return bearing;
}

void MainWindow::antennaNext(int nrig)
{
  setAntennaScanning(nrig, false);
  antennaStep(nrig, kNext);
}

void MainWindow::antennaPrev(int nrig)
{
  setAntennaScanning(nrig, false);
  antennaStep(nrig, kPrevious);
}

void MainWindow::groupStep(int nrig, bool direction)
{
  QSqlQuery query(db);
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and groups.radios1_4 = 1");
  } else {
    match.append(" and groups.radios5_8 = 1");
  }
  if (direction == kNext) {
    sort.append(" order by groups.priority desc, groups.id desc");
  } else {
    sort.append(" order by groups.priority asc, groups.id asc");
  }
  sql.append(kSelectGroupSQL.arg(currentBand[nrig])
                            .arg(match)
                            .arg(sort)
                            .arg("") );

  //qDebug() << sql;
  bool found = false;
  bool foundCurrent = false;
  query.exec(sql);
  while (!found && query.next()) {
    if (foundCurrent) {
      // other conditions?
      currentGroup[nrig] = query.value("id").toInt();
      radioGroupLabel[nrig]->setText(query.value("label").toString());
      cbGroupSetText(nrig, query.value("label").toString());
      groupChanged(nrig);
      found = true;
    }
    if (query.value("id").toInt() == currentGroup[nrig]) { // found current group
      foundCurrent = true;
    }
  }
  if (!found && query.first()) {
    if (currentGroup[nrig] != query.value("id").toInt()) {
      currentGroup[nrig] = query.value("id").toInt();
      radioGroupLabel[nrig]->setText(query.value("label").toString());
      cbGroupSetText(nrig, query.value("label").toString());
      groupChanged(nrig);
      found = true;
    }
  }
  if (trackingState[nrig]) {
    if (currentGroup[nrig] == currentGroup[currentTrackedRadio[nrig]]) {
      setAntennaTracking(nrig, false);
    }
    if (getDisplayMode(currentGroup[nrig]) != kDispCompass) {
      setAntennaTracking(nrig, false);
    }
  }
}

void MainWindow::groupNext(int nrig)
{
  groupStep(nrig, kNext);
}

void MainWindow::groupPrev(int nrig)
{
  groupStep(nrig, kPrevious);
}

MainWindow::~MainWindow()
{

}




void MainWindow::rs485Connection()
{

  if (rs485Serial->isOpen()) {
    disconnect(rs485Serial, 0, 0, 0);
    rs485Serial->close();
    rs485RcvdDataLabel->setText("");
    statusBarUi->showMessage("RS485 port closed", tmpStatusMsgDelay);
    rs485Button->setText("Connect");
    rs485Label->setStyleSheet("");
    rs485PortComboBox->setEnabled(true);
    return;
  }

  if (settings->value("rs485Port", "").toString().isNull() || settings->value("rs485Port", "").toString().isEmpty()) {
    statusBarUi->showMessage("No RS485 port selected", tmpStatusMsgDelay);
    return;
  }

  rs485Serial->setPortName(settings->value("rs485Port", "").toString());
  rs485Serial->setBaudRate(QSerialPort::Baud38400);
  rs485Serial->setFlowControl(QSerialPort::NoFlowControl);
  rs485Serial->setParity(QSerialPort::NoParity);
  rs485Serial->setDataBits(QSerialPort::Data8);
  rs485Serial->setStopBits(QSerialPort::OneStop);
  rs485Serial->open(QIODevice::ReadWrite);
  if (!rs485Serial->isOpen()) {
    statusBarUi->showMessage(QString("Can't open serial device %1")
                              .arg(rs485Serial->portName()),
                              tmpStatusMsgDelay);
    return;
  }
  rs485Serial->setRequestToSend(false); // RTS
  rs485Serial->setDataTerminalReady(false); // DTS

  connect(rs485Serial, &QSerialPort::readyRead, this, &MainWindow::rs485RcvdData);
  statusBarUi->showMessage("RS485 Port connected", tmpStatusMsgDelay);
  rs485Button->setText("Disconnect");
  rs485Label->setStyleSheet("QLabel { color : blue; }");
  rs485PortComboBox->setEnabled(false);
}

void MainWindow::populateSerialPortComboBox(QComboBox* combobox) //, QString savedPort)
{
  combobox->clear();
  auto ports = QSerialPortInfo::availablePorts();
  for (const auto &info : qAsConst(ports)) {
    combobox->addItem(info.systemLocation());
  }
  //combobox->setCurrentText(savedPort);
}

void MainWindow::populateBaudRateComboBox(QComboBox* combobox) //, QString savedBaud)
{
  combobox->clear();
  combobox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
  combobox->addItem(QStringLiteral("19200"), QSerialPort::Baud19200);
  combobox->addItem(QStringLiteral("38400"), QSerialPort::Baud38400);
  combobox->addItem(QStringLiteral("115200"), QSerialPort::Baud115200);
  //combobox->setCurrentText(savedBaud);
}

void MainWindow::setRadioFormFromSettings()
{
  for (int i=0;i<NRIG;++i) {
    radioEnableCheckBox[i]->setChecked(settings->value(s_radioEnable[i], s_radioEnable_def).toBool());
    radioPauseScanCheckBox[i]->setChecked(settings->value(s_radioPauseScan[i], s_radioPauseScan_def).toBool());
    radioHpfCheckBox[i]->setChecked(settings->value(s_radioHpf[i], s_radioHpf_def).toBool());
    radioBpfCheckBox[i]->setChecked(settings->value(s_radioBpf[i], s_radioBpf_def).toBool());
    radioAuxCheckBox[i]->setChecked(settings->value(s_radioAux[i], s_radioAux_def).toBool());
    rigctldCheckbox[i]->setChecked(settings->value(s_rigctld[i], s_rigctld_def).toBool());
    radioNameLineEdit[i]->setText(settings->value(s_radioName[i], s_radioName_def).toString());
    radioGainSpinBox[i]->setValue(settings->value(s_radioGain[i], s_radioGain_def).toInt());
    radioSubRxSpinBox[i]->setValue(settings->value(s_radioSubRxNr[i], s_radioSubRxNr_def).toInt() + 1);
    radioPollTimeLineEdit[i]->setText(settings->value(s_radioPollTime[i], s_radioPollTime_def).toString());
    rigctldIpLineEdit[i]->setText(settings->value(s_rigctldIp[i], s_rigctldIp_def).toString());
    rigctldPortLineEdit[i]->setText(settings->value(s_rigctldPort[i], s_rigctldPort_def).toString());
    radioBandDecoderComboBox[i]->setCurrentIndex(settings->value(s_radioBandDecoder[i], s_radioBandDecoder_def).toInt());

    radioBaudRateComboBox[i]->setCurrentText(settings->value(s_radioBaudRate[i], s_radioBaudRate_def).toString());
    radioSerialPortComboBox[i]->setCurrentText(settings->value(s_radioSerialPort[i], s_radioSerialPort_def).toString());

    radioManufComboBox[i]->setCurrentIndex(0);
    populateModelCombo(i, 0);

    if (cat[0] != nullptr) {
      int idx1;
      int idx2;
      cat[0]->hamlibModelLookup(settings->value(s_radioModel[i], s_radioModel_def).toInt(), idx1, idx2);
      radioManufComboBox[i]->setCurrentIndex(idx1);
      radioModelComboBox[i]->setCurrentIndex(idx2);
    }

  }
  rs485PortComboBox->setCurrentText(settings->value("rs485Port", "").toString());
  rs485AutoConnect->setChecked(settings->value("rs485auto", false).toBool());
  websocketPortLineEdit->setText(settings->value("websocketPort", "7300").toString());
}

void MainWindow::rejectSettings()
{
  setRadioFormFromSettings();
  for (int i=0; i<NRIG; ++i){
    radioEnableCheckBox_stateChanged(i);
    rigctldCheckbox_stateChanged(i);
    radioBandDecoderComboBoxChanged(i);
  }
}

void MainWindow::radioConnection(int nrig)
{
  // toggle
  if (catThread[nrig]->isRunning()) {
    catThread[nrig]->quit();
    catThread[nrig]->wait();
    radioCatButton[nrig]->setText("Start");
    //cat[nrig]->stopSerial();
    cat[nrig]->closeRig();
    cat[nrig]->closeSocket();
    //radioConnected[nrig] = false;
    //radioConnectionStatus(nrig, true);
    //for (int i=0; i<NRIG; ++i) {
    //  if (settings->value(s_radioSubRxNr[i],s_radioSubRxNr_def).toInt() == nrig) {
    //    radioConnected[i] = false;
    //    radioConnectionStatus(i, true);
    //  }
    //}

  } else {
    catThread[nrig]->start();
    radioCatButton[nrig]->setText("Stop");
  }
}



void MainWindow::populateModelCombo(int nrig, int mfg_idx)
{
    radioModelComboBox[nrig]->clear();
    for (int i = 0; i < cat[0]->hamlibNModels(mfg_idx); ++i) {
        radioModelComboBox[nrig]->insertItem(i, cat[0]->hamlibModelName(mfg_idx, i));
    }
}

/**
 * only effects radio form visual elements
 * actual changes applied during save
 */
void MainWindow::radioEnableCheckBox_stateChanged(int nrig)
{
  bool state = radioEnableCheckBox[nrig]->isChecked();
  radioCatFrame[nrig]->setEnabled(state);
  radioProcFrame[nrig]->setEnabled(state);
  radioGeneralFrame[nrig]->setEnabled(state);
}

/**
 * only effects radio form visual elements
 * actual changes applied during save
 */
void MainWindow::radioBandDecoderComboBoxChanged(int nrig)
{
  int idx0 = radioBandDecoderComboBox[nrig]->currentIndex();
  int idx1;
  int idx2;
  switch (idx0) {
    case kCat: // cat
      radioCatFrame[nrig]->setEnabled(true);
      radioSubRxSpinBox[nrig]->setEnabled(false);
      break;
    case kSubRx: // subrx
      cat[0]->hamlibModelLookup(RIG_MODEL_DUMMY, idx1, idx2);
      radioManufComboBox[nrig]->setCurrentIndex(idx1);
      radioModelComboBox[nrig]->setCurrentIndex(idx2);
      radioCatFrame[nrig]->setEnabled(false);
      radioSubRxSpinBox[nrig]->setEnabled(true);
      break;
    case kManual: // manual
    default:
      cat[0]->hamlibModelLookup(RIG_MODEL_DUMMY, idx1, idx2);
      radioManufComboBox[nrig]->setCurrentIndex(idx1);
      radioModelComboBox[nrig]->setCurrentIndex(idx2);
      radioCatFrame[nrig]->setEnabled(false);
      radioSubRxSpinBox[nrig]->setEnabled(false);
      break;
  }
}

/**
 * only effects radio form visual elements
 * actual changes applied during save
 */
void MainWindow::rigctldCheckbox_stateChanged(int nrig)
{
  bool state = rigctldCheckbox[nrig]->isChecked();
  rigctldIpLineEdit[nrig]->setEnabled(state);
  rigctldPortLineEdit[nrig]->setEnabled(state);
  radioManufComboBox[nrig]->setEnabled(!state);
  radioModelComboBox[nrig]->setEnabled(!state);
  radioSerialPortComboBox[nrig]->setEnabled(!state);
  radioBaudRateComboBox[nrig]->setEnabled(!state);
}

void MainWindow::setupDatabaseModelsViews()
{
  QPalette p = palette();
  p.setColor(QPalette::Highlight, hlClr);
  p.setColor(QPalette::HighlightedText, txtClr);
  //QPalette palHeader = palette();
  //palHeader.setColor(QPalette::Text, Qt::blue);
  //palheader.setColor(QPalette::WindowText, Qt::blue);

  //bandsTableModel = new QSqlTableModel(this, db);
  bandsTableModel->setTable("bands");
  bandsTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  bandsTableModel->setSort(2,Qt::AscendingOrder); // sort by start freq ascending
  bandsTableModel->select();
  while (bandsTableModel->canFetchMore()) {
    bandsTableModel->fetchMore();
  }
  bandsTableModel->setHeaderData(1, Qt::Horizontal, tr("Band Name"));
  bandsTableModel->setHeaderData(2, Qt::Horizontal, tr("Start (kHz)"));
  bandsTableModel->setHeaderData(3, Qt::Horizontal, tr("Stop (kHz)"));
  bandsTableModel->setHeaderData(4, Qt::Horizontal, tr("CAT Id"));
  bandsTableModel->setHeaderData(5, Qt::Horizontal, tr("Gain(dB)"));
  bandsTableModel->setHeaderData(6, Qt::Horizontal, tr("BPF"));
  bandsTableModel->setHeaderData(7, Qt::Horizontal, tr("HPF"));
  bandsTableModel->setHeaderData(8, Qt::Horizontal, tr("AUX"));
  //QSortFilterProxyModel proxyModel;
  //proxyModel.setSourceModel(bandsTableModel);
  //bandsTableView->setModel(&proxyModel);
  bandsTableView->setModel(bandsTableModel);
  bandsTableView->setItemDelegateForColumn(2, &frequencyDelegate);
  bandsTableView->setItemDelegateForColumn(3, &frequencyDelegate);
  bandsTableView->setItemDelegateForColumn(4, &catIdDelegate);
  bandsTableView->setItemDelegateForColumn(5, &gainDelegate);
  bandsTableView->setItemDelegateForColumn(6, &yesNoDelegate);
  bandsTableView->setItemDelegateForColumn(7, &yesNoDelegate);
  bandsTableView->setItemDelegateForColumn(8, &auxDelegate);
  //bandsTableView->resize(400, 250);
  bandsTableView->hideColumn(0); // don't show the ID
  //bandsTableView->verticalHeader()->hide(); // hide row numbers
  //bandsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  bandsTableView->horizontalHeader()->setHighlightSections(false);
  bandsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  bandsTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  bandsTableView->horizontalHeader()->resizeSection(2, 80);
  bandsTableView->horizontalHeader()->resizeSection(3, 80);
  bandsTableView->horizontalHeader()->resizeSection(4, 55);
  bandsTableView->horizontalHeader()->resizeSection(5, 65);
  bandsTableView->horizontalHeader()->resizeSection(6, 55);
  bandsTableView->horizontalHeader()->resizeSection(7, 55);
  bandsTableView->horizontalHeader()->resizeSection(8, 55);
  //bandsTableView->horizontalHeader()->setFont(QFont(QGuiApplication::font().family(), -1, QFont::DemiBold, QFont::StyleNormal));
  bandsTableView->setAlternatingRowColors(true);
  bandsTableView->verticalHeader()->setDefaultSectionSize(23);
  bandsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  bandsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  bandsTableView->setPalette(p);
  //bandsTableView->setSortingEnabled(true);
  //bandsTableView->horizontalHeader()->setPalette(palHeader);
  bandsTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  bandsTableView->show();

  //groupsTableModel = new QSqlTableModel(this, db);
  groupsTableModel->setTable("groups");
  groupsTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  groupsTableModel->setSort(1,Qt::AscendingOrder);  // sort by name
  groupsTableModel->select();
  while (groupsTableModel->canFetchMore()) {
    groupsTableModel->fetchMore();
  }
  groupsTableModel->setHeaderData(1, Qt::Horizontal, tr("Group Name"));
  groupsTableModel->setHeaderData(2, Qt::Horizontal, tr("Label"));
  groupsTableModel->setHeaderData(3, Qt::Horizontal, tr("Display Mode"));
  groupsTableModel->setHeaderData(4, Qt::Horizontal, tr("Gain(dB)"));
  groupsTableModel->setHeaderData(5, Qt::Horizontal, tr("Priority"));
  groupsTableModel->setHeaderData(6, Qt::Horizontal, tr("Enabled"));
  groupsTableModel->setHeaderData(7, Qt::Horizontal, tr("Radios\n[1-4]"));
  groupsTableModel->setHeaderData(8, Qt::Horizontal, tr("Radios\n[5-8]"));
  groupsTableView->setModel(groupsTableModel);
  groupsTableView->setItemDelegateForColumn(3, &compassDelegate);
  groupsTableView->setItemDelegateForColumn(4, &gainDelegate);
  groupsTableView->setItemDelegateForColumn(5, &priorityDelegate);
  groupsTableView->setItemDelegateForColumn(6, &yesNoDelegate);
  groupsTableView->setItemDelegateForColumn(7, &yesNoDelegate);
  groupsTableView->setItemDelegateForColumn(8, &yesNoDelegate);
  groupsTableView->hideColumn(0);
  //groupsTableView->verticalHeader()->hide();
  groupsTableView->horizontalHeader()->setHighlightSections(false);
  groupsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  groupsTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  groupsTableView->horizontalHeader()->resizeSection(2, 80);
  groupsTableView->horizontalHeader()->resizeSection(3, 100);
  groupsTableView->horizontalHeader()->resizeSection(4, 65);
  groupsTableView->horizontalHeader()->resizeSection(5, 55);
  groupsTableView->horizontalHeader()->resizeSection(6, 55);
  groupsTableView->horizontalHeader()->resizeSection(7, 55);
  groupsTableView->horizontalHeader()->resizeSection(8, 55);
  //groupsTableView->horizontalHeader()->setFont(QFont(QGuiApplication::font().family(), -1, QFont::DemiBold, QFont::StyleNormal));
  groupsTableView->setAlternatingRowColors(true);
  groupsTableView->verticalHeader()->setDefaultSectionSize(23);
  groupsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  groupsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  groupsTableView->setPalette(p);
  groupsTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  groupsTableView->show();

  //antennasTableModel = new QSqlTableModel(this, db);
  antennasTableModel->setTable("antennas");
  antennasTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  antennasTableModel->setSort(3,Qt::AscendingOrder); // sort by switch port
  antennasTableModel->select();
  while (antennasTableModel->canFetchMore()) {
    antennasTableModel->fetchMore();
  }
  antennasTableModel->setHeaderData(1, Qt::Horizontal, tr("Antenna Name"));
  antennasTableModel->setHeaderData(2, Qt::Horizontal, tr("Label"));
  antennasTableModel->setHeaderData(3, Qt::Horizontal, tr("Port\nNumber"));
  antennasTableModel->setHeaderData(4, Qt::Horizontal, tr("Virtual\nNumber"));
  antennasTableModel->setHeaderData(5, Qt::Horizontal, tr("Start(°)"));
  antennasTableModel->setHeaderData(6, Qt::Horizontal, tr("Stop(°)"));
  antennasTableModel->setHeaderData(7, Qt::Horizontal, tr("Gain(dB)"));
  antennasTableModel->setHeaderData(8, Qt::Horizontal, tr("Priority"));
  antennasTableModel->setHeaderData(9, Qt::Horizontal, tr("Radios\n[1-4]"));
  antennasTableModel->setHeaderData(10, Qt::Horizontal, tr("Radios\n[5-8]"));
  antennasTableModel->setHeaderData(11, Qt::Horizontal, tr("Scan\nEnable"));
  antennasTableModel->setHeaderData(12, Qt::Horizontal, tr("Enabled"));
  antennasTableView->setModel(antennasTableModel);
  antennasTableView->hideColumn(0);
  //antennasTableView->verticalHeader()->hide();
  antennasTableView->horizontalHeader()->setHighlightSections(false);
  antennasTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  antennasTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  antennasTableView->horizontalHeader()->resizeSection(2, 80);
  antennasTableView->horizontalHeader()->resizeSection(3, 55);
  antennasTableView->horizontalHeader()->resizeSection(4, 55);
  antennasTableView->horizontalHeader()->resizeSection(5, 50);
  antennasTableView->horizontalHeader()->resizeSection(6, 50);
  antennasTableView->horizontalHeader()->resizeSection(7, 65);
  antennasTableView->horizontalHeader()->resizeSection(8, 50);
  antennasTableView->horizontalHeader()->resizeSection(9, 50);
  antennasTableView->horizontalHeader()->resizeSection(10, 50);
  antennasTableView->horizontalHeader()->resizeSection(11, 50);
  antennasTableView->horizontalHeader()->resizeSection(12, 50);
  antennasTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter|Qt::AlignBottom);
  //antennasTableView->horizontalHeader()->setFont(QFont(QGuiApplication::font().family(), -1, QFont::DemiBold, QFont::StyleNormal));
  antennasTableView->setAlternatingRowColors(true);
  antennasTableView->verticalHeader()->setDefaultSectionSize(23);
  antennasTableView->setItemDelegateForColumn(3, &portDelegate);
  antennasTableView->setItemDelegateForColumn(4, &vantDelegate);
  antennasTableView->setItemDelegateForColumn(5, &degreesDelegate);
  antennasTableView->setItemDelegateForColumn(6, &degreesDelegate);
  antennasTableView->setItemDelegateForColumn(7, &gainDelegate);
  antennasTableView->setItemDelegateForColumn(8, &priorityDelegate);
  antennasTableView->setItemDelegateForColumn(9, &yesNoDelegate);
  antennasTableView->setItemDelegateForColumn(10, &yesNoDelegate);
  antennasTableView->setItemDelegateForColumn(11, &yesNoDelegate);
  antennasTableView->setItemDelegateForColumn(12, &yesNoDelegate);
  antennasTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  antennasTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  antennasTableView->setPalette(p);
  antennasTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  antennasTableView->show();

  //band_groupTableModel = new QSqlRelationalTableModel(this, db);
  band_groupTableModel->setTable("band_group_map");
  band_groupTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  band_groupTableModel->setRelation(1, QSqlRelation("bands", "id", "name"));
  band_groupTableModel->setRelation(2, QSqlRelation("groups", "id", "name"));
  band_groupTableModel->setSort(1,Qt::AscendingOrder); // sort by band
  band_groupTableModel->select();
  while (band_groupTableModel->canFetchMore()) {
    band_groupTableModel->fetchMore();
  }
  band_groupTableModel->setHeaderData(1, Qt::Horizontal, tr("Band"));
  band_groupTableModel->setHeaderData(2, Qt::Horizontal, tr("Group"));
  band_groupTableView->setModel(band_groupTableModel);
  band_groupTableView->hideColumn(0);
  band_groupTableView->setItemDelegate(new QSqlRelationalDelegate(band_groupTableView));
  //band_groupTableView->verticalHeader()->hide();
  band_groupTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  band_groupTableView->horizontalHeader()->setHighlightSections(false);
  //band_groupTableView->horizontalHeader()->setFont(QFont(QGuiApplication::font().family(), -1, QFont::DemiBold, QFont::StyleNormal));
  band_groupTableView->setAlternatingRowColors(true);
  band_groupTableView->verticalHeader()->setDefaultSectionSize(23);
  band_groupTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  band_groupTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  band_groupTableView->setPalette(p);
  band_groupTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  band_groupTableView->show();

  //group_antennaTableModel = new QSqlRelationalTableModel(this, db);
  group_antennaTableModel->setTable("group_antenna_map");
  group_antennaTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  group_antennaTableModel->setRelation(1, QSqlRelation("groups", "id", "name"));
  group_antennaTableModel->setRelation(2, QSqlRelation("antennas", "id", "name"));
  group_antennaTableModel->setSort(1,Qt::AscendingOrder); // sort by group
  group_antennaTableModel->select();
  while (group_antennaTableModel->canFetchMore()) {
    group_antennaTableModel->fetchMore();
  }
  group_antennaTableModel->setHeaderData(1, Qt::Horizontal, tr("Group"));
  group_antennaTableModel->setHeaderData(2, Qt::Horizontal, tr("Antenna"));
  group_antennaTableView->setModel(group_antennaTableModel);
  group_antennaTableView->hideColumn(0);
  group_antennaTableView->setItemDelegate(new QSqlRelationalDelegate(group_antennaTableView));
  //group_antennaTableView->verticalHeader()->hide();
  group_antennaTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  group_antennaTableView->horizontalHeader()->setHighlightSections(false);
  //group_antennaTableView->horizontalHeader()->setFont(QFont(QGuiApplication::font().family(), -1, QFont::DemiBold, QFont::StyleNormal));
  group_antennaTableView->setAlternatingRowColors(true);
  group_antennaTableView->verticalHeader()->setDefaultSectionSize(23);
  group_antennaTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  group_antennaTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  group_antennaTableView->setPalette(p);
  group_antennaTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  group_antennaTableView->show();

  //cron
  cronTableModel->setTable("cron");
  cronTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
  cronTableModel->setRelation(2, QSqlRelation("antennas", "id", "name"));
  cronTableModel->setSort(1,Qt::AscendingOrder); // sort by radio
  cronTableModel->select();
  while (cronTableModel->canFetchMore()) {
    cronTableModel->fetchMore();
  }
  cronTableModel->setHeaderData(1, Qt::Horizontal, tr("Radio"));
  cronTableModel->setHeaderData(2, Qt::Horizontal, tr("Antenna"));
  cronTableModel->setHeaderData(3, Qt::Horizontal, tr("Expression\ns m h d M dW [Y]"));
  cronTableModel->setHeaderData(4, Qt::Horizontal, tr("Enabled"));
  cronTableModel->setHeaderData(5, Qt::Horizontal, tr("Next Run"));
  cronTableView->setModel(cronTableModel);
  cronTableView->hideColumn(0);
  cronTableView->setItemDelegateForColumn(2, new QSqlRelationalDelegate(cronTableView));
  radioDelegate = new RadioComboBoxItemDelegate(*settings, this);
  cronTableView->setItemDelegateForColumn(1, radioDelegate);
  cronTableView->setItemDelegateForColumn(4, &yesNoDelegate);
  //cronTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  cronTableView->horizontalHeader()->setHighlightSections(false);
  cronTableView->setAlternatingRowColors(true);
  cronTableView->verticalHeader()->setDefaultSectionSize(23);
  cronTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  cronTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  cronTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  //cronTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  cronTableView->horizontalHeader()->resizeSection(1, 100);
  cronTableView->horizontalHeader()->resizeSection(2, 200);
  //cronTableView->horizontalHeader()->resizeSection(3, 200);
  cronTableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  cronTableView->horizontalHeader()->resizeSection(4, 55);
  cronTableView->horizontalHeader()->resizeSection(5, 150);
  cronTableView->setPalette(p);
  cronTableView->setStyleSheet("QHeaderView::section { color:blue; }");
  cronTableView->show();



  connect(addBandGroupButton, &QPushButton::released, this, &MainWindow::addBandGroup);
  connect(removeBandGroupButton, &QPushButton::released, this, &MainWindow::removeBandGroup);
  connect(saveBandGroupButton, &QPushButton::released, this, &MainWindow::saveBandGroup);

  connect(addGroupAntennaButton, &QPushButton::released, this, &MainWindow::addGroupAntenna);
  connect(removeGroupAntennaButton, &QPushButton::released, this, &MainWindow::removeGroupAntenna);
  connect(saveGroupAntennaButton, &QPushButton::released, this, &MainWindow::saveGroupAntenna);

  connect(removeBandButton, &QPushButton::released, this, &MainWindow::removeBand);
  connect(addBandButton, &QPushButton::released, this, &MainWindow::addBand);
  connect(saveBandButton, &QPushButton::released, this, &MainWindow::saveBand);

  connect(addGroupButton, &QPushButton::released, this, &MainWindow::addGroup);
  connect(saveGroupButton, &QPushButton::released, this, &MainWindow::saveGroup);
  connect(removeGroupButton, &QPushButton::released, this, &MainWindow::removeGroup);

  connect(addAntennaButton, &QPushButton::released, this, &MainWindow::addAntenna);
  connect(saveAntennaButton, &QPushButton::released, this, &MainWindow::saveAntenna);
  connect(removeAntennaButton, &QPushButton::released, this, &MainWindow::removeAntenna);

  connect(addCronButton, &QPushButton::released, this, &MainWindow::addCron);
  connect(saveCronButton, &QPushButton::released, this, &MainWindow::saveCron);
  connect(removeCronButton, &QPushButton::released, this, &MainWindow::removeCron);

}

void MainWindow::openDatabase()
{
  // create writeable data directory if doesn't exist
  auto writeable_data_dir = QDir {QStandardPaths::writableLocation (QStandardPaths::AppConfigLocation)};
  if (!writeable_data_dir.mkpath (".")) {
    qDebug() << "openDatabase: Failed to create data directory";
    exit(EXIT_FAILURE);
  }

  // set up SQLite database
  if (!QSqlDatabase::drivers ().contains ("QSQLITE")) {
    qDebug() << "openDatabase: Failed to find SQLite Qt driver";
    exit(EXIT_FAILURE);
  }
  //db = QSqlDatabase::addDatabase ("QSQLITE");
  db.setDatabaseName (writeable_data_dir.absoluteFilePath ("db.sqlite"));
  if (!db.open ()) {
    qDebug() << "openDatabase: Database Error: " << db.lastError ().text ();
    exit(EXIT_FAILURE);
  }
}

void MainWindow::initDatabase()
{
  QSqlQuery query(db);
  QString sql;
  // enum compass / list
  sql.append("CREATE TABLE IF NOT EXISTS groups (`id` INTEGER NOT NULL PRIMARY KEY, `name` TEXT UNIQUE, `label` TEXT, `display_mode` INTEGER DEFAULT 0 NOT NULL, `gain` INTEGER DEFAULT 0 NOT NULL, `priority` INTEGER DEFAULT 1 NOT NULL, `enabled` INTEGER DEFAULT 1 NOT NULL, `radios1_4` INTEGER DEFAULT 1 NOT NULL, `radios5_8` INTEGER DEFAULT 0 NOT NULL)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `groups` table";
      exit(EXIT_FAILURE);
  }
  sql.clear();
  sql.append("CREATE TABLE IF NOT EXISTS antennas (`id` INTEGER NOT NULL PRIMARY KEY, `name` TEXT UNIQUE, `label` TEXT, `switch_port` INTEGER, `vant` INTEGER UNIQUE, `start_deg` INTEGER, `stop_deg` INTEGER, `gain` INTEGER DEFAULT 0 NOT NULL, `priority` INTEGER DEFAULT 1 NOT NULL, `radios1_4` INTEGER DEFAULT 1 NOT NULL, `radios5_8` INTEGER DEFAULT 0 NOT NULL, `scan` INTEGER, `enabled` INTEGER DEFAULT 1 NOT NULL)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `antennas` table";
      exit(EXIT_FAILURE);
  }
  // bands
  sql.clear();
  sql.append("CREATE TABLE IF NOT EXISTS bands (`id` INTEGER NOT NULL PRIMARY KEY, `name` TEXT UNIQUE, `start_freq` INTEGER, `stop_freq` INTEGER, `cat_id` INTEGER, `gain` INTEGER DEFAULT 0 NOT NULL, `bpf` INTEGER DEFAULT 0 NOT NULL, `hpf` INTEGER DEFAULT 0 NOT NULL, `aux` INTEGER DEFAULT 0)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `bands` table";
      exit(EXIT_FAILURE);
  }
  sql.clear();
  sql.append("CREATE TABLE IF NOT EXISTS band_group_map (`id` INTEGER NOT NULL PRIMARY KEY, `band_id` INTEGER, `group_id` INTEGER)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `bands_groups` table";
      exit(EXIT_FAILURE);
  }
  sql.clear();
  sql.append("CREATE TABLE IF NOT EXISTS group_antenna_map (`id` INTEGER NOT NULL PRIMARY KEY, `group_id` INTEGER, `antenna_id` INTEGER)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `groups_antennas` table";
      exit(EXIT_FAILURE);
  }
  sql.clear();
  sql.append("CREATE TABLE IF NOT EXISTS cron (`id` INTEGER NOT NULL PRIMARY KEY, `radio_id` INTEGER, `antenna_id` INTEGER, `expression` TEXT, `enabled` INTEGER DEFAULT 0 NOT NULL, `next` TEXT)");
  if (!query.exec(sql)) {
      qDebug() << "initDatabase: Database Error: cannot create `cron` table";
      exit(EXIT_FAILURE);
  }
}

void MainWindow::initUiPtrs()
{
    radioEnableCheckBox[0] = radioEnableCheckBox_1;
    radioEnableCheckBox[1] = radioEnableCheckBox_2;
    radioEnableCheckBox[2] = radioEnableCheckBox_3;
    radioEnableCheckBox[3] = radioEnableCheckBox_4;
    radioEnableCheckBox[4] = radioEnableCheckBox_5;
    radioEnableCheckBox[5] = radioEnableCheckBox_6;
    radioEnableCheckBox[6] = radioEnableCheckBox_7;
    radioEnableCheckBox[7] = radioEnableCheckBox_8;

    radioNameLineEdit[0] = radioNameLineEdit_1;
    radioNameLineEdit[1] = radioNameLineEdit_2;
    radioNameLineEdit[2] = radioNameLineEdit_3;
    radioNameLineEdit[3] = radioNameLineEdit_4;
    radioNameLineEdit[4] = radioNameLineEdit_5;
    radioNameLineEdit[5] = radioNameLineEdit_6;
    radioNameLineEdit[6] = radioNameLineEdit_7;
    radioNameLineEdit[7] = radioNameLineEdit_8;

    radioPauseScanCheckBox[0] = radioPauseScanCheckBox_1;
    radioPauseScanCheckBox[1] = radioPauseScanCheckBox_2;
    radioPauseScanCheckBox[2] = radioPauseScanCheckBox_3;
    radioPauseScanCheckBox[3] = radioPauseScanCheckBox_4;
    radioPauseScanCheckBox[4] = radioPauseScanCheckBox_5;
    radioPauseScanCheckBox[5] = radioPauseScanCheckBox_6;
    radioPauseScanCheckBox[6] = radioPauseScanCheckBox_7;
    radioPauseScanCheckBox[7] = radioPauseScanCheckBox_8;

    radioBpfCheckBox[0] = radioBpfCheckBox_1;
    radioBpfCheckBox[1] = radioBpfCheckBox_2;
    radioBpfCheckBox[2] = radioBpfCheckBox_3;
    radioBpfCheckBox[3] = radioBpfCheckBox_4;
    radioBpfCheckBox[4] = radioBpfCheckBox_5;
    radioBpfCheckBox[5] = radioBpfCheckBox_6;
    radioBpfCheckBox[6] = radioBpfCheckBox_7;
    radioBpfCheckBox[7] = radioBpfCheckBox_8;

    radioHpfCheckBox[0] = radioHpfCheckBox_1;
    radioHpfCheckBox[1] = radioHpfCheckBox_2;
    radioHpfCheckBox[2] = radioHpfCheckBox_3;
    radioHpfCheckBox[3] = radioHpfCheckBox_4;
    radioHpfCheckBox[4] = radioHpfCheckBox_5;
    radioHpfCheckBox[5] = radioHpfCheckBox_6;
    radioHpfCheckBox[6] = radioHpfCheckBox_7;
    radioHpfCheckBox[7] = radioHpfCheckBox_8;

    radioGainSpinBox[0] = radioGainSpinBox_1;
    radioGainSpinBox[1] = radioGainSpinBox_2;
    radioGainSpinBox[2] = radioGainSpinBox_3;
    radioGainSpinBox[3] = radioGainSpinBox_4;
    radioGainSpinBox[4] = radioGainSpinBox_5;
    radioGainSpinBox[5] = radioGainSpinBox_6;
    radioGainSpinBox[6] = radioGainSpinBox_7;
    radioGainSpinBox[7] = radioGainSpinBox_8;

    radioAuxCheckBox[0] = radioAuxCheckBox_1;
    radioAuxCheckBox[1] = radioAuxCheckBox_2;
    radioAuxCheckBox[2] = radioAuxCheckBox_3;
    radioAuxCheckBox[3] = radioAuxCheckBox_4;
    radioAuxCheckBox[4] = radioAuxCheckBox_5;
    radioAuxCheckBox[5] = radioAuxCheckBox_6;
    radioAuxCheckBox[6] = radioAuxCheckBox_7;
    radioAuxCheckBox[7] = radioAuxCheckBox_8;

    radioBandDecoderComboBox[0] = radioBandDecoderComboBox_1;
    radioBandDecoderComboBox[1] = radioBandDecoderComboBox_2;
    radioBandDecoderComboBox[2] = radioBandDecoderComboBox_3;
    radioBandDecoderComboBox[3] = radioBandDecoderComboBox_4;
    radioBandDecoderComboBox[4] = radioBandDecoderComboBox_5;
    radioBandDecoderComboBox[5] = radioBandDecoderComboBox_6;
    radioBandDecoderComboBox[6] = radioBandDecoderComboBox_7;
    radioBandDecoderComboBox[7] = radioBandDecoderComboBox_8;

    radioSubRxSpinBox[0] = radioSubRxSpinBox_1;
    radioSubRxSpinBox[1] = radioSubRxSpinBox_2;
    radioSubRxSpinBox[2] = radioSubRxSpinBox_3;
    radioSubRxSpinBox[3] = radioSubRxSpinBox_4;
    radioSubRxSpinBox[4] = radioSubRxSpinBox_5;
    radioSubRxSpinBox[5] = radioSubRxSpinBox_6;
    radioSubRxSpinBox[6] = radioSubRxSpinBox_7;
    radioSubRxSpinBox[7] = radioSubRxSpinBox_8;

    rigctldCheckbox[0] = rigctldCheckbox_1;
    rigctldCheckbox[1] = rigctldCheckbox_2;
    rigctldCheckbox[2] = rigctldCheckbox_3;
    rigctldCheckbox[3] = rigctldCheckbox_4;
    rigctldCheckbox[4] = rigctldCheckbox_5;
    rigctldCheckbox[5] = rigctldCheckbox_6;
    rigctldCheckbox[6] = rigctldCheckbox_7;
    rigctldCheckbox[7] = rigctldCheckbox_8;

    rigctldIpLineEdit[0] = rigctldIpLineEdit_1;
    rigctldIpLineEdit[1] = rigctldIpLineEdit_2;
    rigctldIpLineEdit[2] = rigctldIpLineEdit_3;
    rigctldIpLineEdit[3] = rigctldIpLineEdit_4;
    rigctldIpLineEdit[4] = rigctldIpLineEdit_5;
    rigctldIpLineEdit[5] = rigctldIpLineEdit_6;
    rigctldIpLineEdit[6] = rigctldIpLineEdit_7;
    rigctldIpLineEdit[7] = rigctldIpLineEdit_8;

    rigctldPortLineEdit[0] = rigctldPortLineEdit_1;
    rigctldPortLineEdit[1] = rigctldPortLineEdit_2;
    rigctldPortLineEdit[2] = rigctldPortLineEdit_3;
    rigctldPortLineEdit[3] = rigctldPortLineEdit_4;
    rigctldPortLineEdit[4] = rigctldPortLineEdit_5;
    rigctldPortLineEdit[5] = rigctldPortLineEdit_6;
    rigctldPortLineEdit[6] = rigctldPortLineEdit_7;
    rigctldPortLineEdit[7] = rigctldPortLineEdit_8;

    radioManufComboBox[0] = radioManufComboBox_1;
    radioManufComboBox[1] = radioManufComboBox_2;
    radioManufComboBox[2] = radioManufComboBox_3;
    radioManufComboBox[3] = radioManufComboBox_4;
    radioManufComboBox[4] = radioManufComboBox_5;
    radioManufComboBox[5] = radioManufComboBox_6;
    radioManufComboBox[6] = radioManufComboBox_7;
    radioManufComboBox[7] = radioManufComboBox_8;

    radioModelComboBox[0] = radioModelComboBox_1;
    radioModelComboBox[1] = radioModelComboBox_2;
    radioModelComboBox[2] = radioModelComboBox_3;
    radioModelComboBox[3] = radioModelComboBox_4;
    radioModelComboBox[4] = radioModelComboBox_5;
    radioModelComboBox[5] = radioModelComboBox_6;
    radioModelComboBox[6] = radioModelComboBox_7;
    radioModelComboBox[7] = radioModelComboBox_8;

    radioSerialPortComboBox[0] = radioSerialPortComboBox_1;
    radioSerialPortComboBox[1] = radioSerialPortComboBox_2;
    radioSerialPortComboBox[2] = radioSerialPortComboBox_3;
    radioSerialPortComboBox[3] = radioSerialPortComboBox_4;
    radioSerialPortComboBox[4] = radioSerialPortComboBox_5;
    radioSerialPortComboBox[5] = radioSerialPortComboBox_6;
    radioSerialPortComboBox[6] = radioSerialPortComboBox_7;
    radioSerialPortComboBox[7] = radioSerialPortComboBox_8;

    radioBaudRateComboBox[0] = radioBaudRateComboBox_1;
    radioBaudRateComboBox[1] = radioBaudRateComboBox_2;
    radioBaudRateComboBox[2] = radioBaudRateComboBox_3;
    radioBaudRateComboBox[3] = radioBaudRateComboBox_4;
    radioBaudRateComboBox[4] = radioBaudRateComboBox_5;
    radioBaudRateComboBox[5] = radioBaudRateComboBox_6;
    radioBaudRateComboBox[6] = radioBaudRateComboBox_7;
    radioBaudRateComboBox[7] = radioBaudRateComboBox_8;

    radioPollTimeLineEdit[0] = radioPollTimeLineEdit_1;
    radioPollTimeLineEdit[1] = radioPollTimeLineEdit_2;
    radioPollTimeLineEdit[2] = radioPollTimeLineEdit_3;
    radioPollTimeLineEdit[3] = radioPollTimeLineEdit_4;
    radioPollTimeLineEdit[4] = radioPollTimeLineEdit_5;
    radioPollTimeLineEdit[5] = radioPollTimeLineEdit_6;
    radioPollTimeLineEdit[6] = radioPollTimeLineEdit_7;
    radioPollTimeLineEdit[7] = radioPollTimeLineEdit_8;

    radioCatFrame[0] = radioCatFrame_1;
    radioCatFrame[1] = radioCatFrame_2;
    radioCatFrame[2] = radioCatFrame_3;
    radioCatFrame[3] = radioCatFrame_4;
    radioCatFrame[4] = radioCatFrame_5;
    radioCatFrame[5] = radioCatFrame_6;
    radioCatFrame[6] = radioCatFrame_7;
    radioCatFrame[7] = radioCatFrame_8;

    radioProcFrame[0] = radioProcFrame_1;
    radioProcFrame[1] = radioProcFrame_2;
    radioProcFrame[2] = radioProcFrame_3;
    radioProcFrame[3] = radioProcFrame_4;
    radioProcFrame[4] = radioProcFrame_5;
    radioProcFrame[5] = radioProcFrame_6;
    radioProcFrame[6] = radioProcFrame_7;
    radioProcFrame[7] = radioProcFrame_8;

    radioGeneralFrame[0] = radioGeneralFrame_1;
    radioGeneralFrame[1] = radioGeneralFrame_2;
    radioGeneralFrame[2] = radioGeneralFrame_3;
    radioGeneralFrame[3] = radioGeneralFrame_4;
    radioGeneralFrame[4] = radioGeneralFrame_5;
    radioGeneralFrame[5] = radioGeneralFrame_6;
    radioGeneralFrame[6] = radioGeneralFrame_7;
    radioGeneralFrame[7] = radioGeneralFrame_8;

    radioCatButton[0] = radioCatButton_1;
    radioCatButton[1] = radioCatButton_2;
    radioCatButton[2] = radioCatButton_3;
    radioCatButton[3] = radioCatButton_4;
    radioCatButton[4] = radioCatButton_5;
    radioCatButton[5] = radioCatButton_6;
    radioCatButton[6] = radioCatButton_7;
    radioCatButton[7] = radioCatButton_8;

    radioFreqLabel[0] = radioFreqLabel_1;
    radioFreqLabel[1] = radioFreqLabel_2;
    radioFreqLabel[2] = radioFreqLabel_3;
    radioFreqLabel[3] = radioFreqLabel_4;
    radioFreqLabel[4] = radioFreqLabel_5;
    radioFreqLabel[5] = radioFreqLabel_6;
    radioFreqLabel[6] = radioFreqLabel_7;
    radioFreqLabel[7] = radioFreqLabel_8;

    radioPttLabel[0] = radioPttLabel_1;
    radioPttLabel[1] = radioPttLabel_2;
    radioPttLabel[2] = radioPttLabel_3;
    radioPttLabel[3] = radioPttLabel_4;
    radioPttLabel[4] = radioPttLabel_5;
    radioPttLabel[5] = radioPttLabel_6;
    radioPttLabel[6] = radioPttLabel_7;
    radioPttLabel[7] = radioPttLabel_8;

    radioNameLabel[0] = radioNameLabel_1;
    radioNameLabel[1] = radioNameLabel_2;
    radioNameLabel[2] = radioNameLabel_3;
    radioNameLabel[3] = radioNameLabel_4;
    radioNameLabel[4] = radioNameLabel_5;
    radioNameLabel[5] = radioNameLabel_6;
    radioNameLabel[6] = radioNameLabel_7;
    radioNameLabel[7] = radioNameLabel_8;

    radioBandLabel[0] = radioBandLabel_1;
    radioBandLabel[1] = radioBandLabel_2;
    radioBandLabel[2] = radioBandLabel_3;
    radioBandLabel[3] = radioBandLabel_4;
    radioBandLabel[4] = radioBandLabel_5;
    radioBandLabel[5] = radioBandLabel_6;
    radioBandLabel[6] = radioBandLabel_7;
    radioBandLabel[7] = radioBandLabel_8;

    radioGroupLabel[0] = radioGroupLabel_1;
    radioGroupLabel[1] = radioGroupLabel_2;
    radioGroupLabel[2] = radioGroupLabel_3;
    radioGroupLabel[3] = radioGroupLabel_4;
    radioGroupLabel[4] = radioGroupLabel_5;
    radioGroupLabel[5] = radioGroupLabel_6;
    radioGroupLabel[6] = radioGroupLabel_7;
    radioGroupLabel[7] = radioGroupLabel_8;

    radioAntennaLabel[0] = radioAntennaLabel_1;
    radioAntennaLabel[1] = radioAntennaLabel_2;
    radioAntennaLabel[2] = radioAntennaLabel_3;
    radioAntennaLabel[3] = radioAntennaLabel_4;
    radioAntennaLabel[4] = radioAntennaLabel_5;
    radioAntennaLabel[5] = radioAntennaLabel_6;
    radioAntennaLabel[6] = radioAntennaLabel_7;
    radioAntennaLabel[7] = radioAntennaLabel_8;

    clientsLabel[0] = clientsLabel_1;
    clientsLabel[1] = clientsLabel_2;
    clientsLabel[2] = clientsLabel_3;
    clientsLabel[3] = clientsLabel_4;
    clientsLabel[4] = clientsLabel_5;
    clientsLabel[5] = clientsLabel_6;
    clientsLabel[6] = clientsLabel_7;
    clientsLabel[7] = clientsLabel_8;
}

void MainWindow::about()
{
  QString msg;
  QString datetime = QStringLiteral(__DATE__) + QStringLiteral(" ") + QStringLiteral(__TIME__);
  #if defined(__GNUC__)
  QString compiler = QStringLiteral("GCC ") + QStringLiteral(__VERSION__);
  #else
  QString compiler = QStringLiteral(__VERSION__);
  #endif

  msg.append(QString("<p>SoftRX v0.1 Copyright 2021 E. Tichansky NO3M</p>"
                "<ul><li>Compiled %3 with %4</li>"
                "<li>Qt library version: %1</li>"
                "<li>hamlib http://www.hamlib.org %2</li>"
                "</ul><hr><p>SoftRX is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License "
                "as published by the Free Software Foundation, either version 3 of the License, or any later version, http://www.gnu.org/licenses/</p>")
                .arg(qVersion())
                .arg(hamlib_version)
                .arg(datetime)
                .arg(compiler)

              );

  QMessageBox::about(this, "About SoftRX", msg);
}


// radio window methods ===========================

void MainWindow::sendRadioWindowData(int nrig, const QJsonObject &object, QWebSocket *pClient)
{
  QJsonDocument doc(object);
  QByteArray data = doc.toJson(QJsonDocument::Compact);
  bool sent = false;
  QString recipient = "";
  for (const auto &client : qAsConst(m_clients)) {
    if (pClient == nullptr) {
      if (client.radio == nrig) {
        client.websocket->sendBinaryMessage(data);
        sent = true;
      }
      recipient = QString("[%1] ").arg(nrig+1);
    } else {
      if (client.websocket == pClient) {
        client.websocket->sendBinaryMessage(data);
        sent = true;
        recipient = QString("[%1:%2] ")
                      .arg(pClient->peerAddress().toString())
                      .arg(pClient->peerPort());
        break;
      }
    }
  }
  if (sent) {
    jsonLog->appendPlainText("[" + QDateTime::currentDateTime().toString("hh:mm:ss")
                             + "]"
                             + recipient
                             + QString::fromUtf8(data).simplified());
  }
}

void MainWindow::frameSetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("frame"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::lbPttState(int nrig, QWebSocket *pClient) {
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("ptt"));
  object.insert("method", QJsonValue::fromVariant("state"));
  object.insert("state", QJsonValue::fromVariant(currentPtt[nrig]));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::radioNameSetText(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("radioName"));
  object.insert("method", QJsonValue::fromVariant("setText"));
  object.insert("text", QJsonValue::fromVariant(settings->value(s_radioName[nrig], s_radioName_def).toString()));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::radioConnectionStatus(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("radioName"));
  object.insert("method", QJsonValue::fromVariant("status"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
  if (state) {
    radioNameLabel[nrig]->setStyleSheet("QLabel { color : blue; font-weight:600; }");
  } else {
    radioNameLabel[nrig]->setStyleSheet("QLabel { color : red; font-weight:600; }");
  }
}

void MainWindow::cbBandSetEnabled(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbBand"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  if (settings->value(s_radioBandDecoder[nrig], s_radioBandDecoder_def).toInt() == kManual) {
    object.insert("state", QJsonValue::fromVariant(true));
  } else {
    object.insert("state", QJsonValue::fromVariant(false));
  }
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbBandAddItems(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbBand"));
  object.insert("method", QJsonValue::fromVariant("addItem"));
  QJsonArray labels;
  labels.push_back(QJsonValue::fromVariant(""));
  QSqlQuery query(db);
  query.exec("SELECT * from bands ORDER BY start_freq");
  while (query.next()) {
    labels.push_back(QJsonValue::fromVariant(query.value("name").toString()));
  }
  object.insert("labels", QJsonValue(labels));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbBandSetText(int nrig, QString text, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbBand"));
  object.insert("method", QJsonValue::fromVariant("setCurrentText"));
  object.insert("text", QJsonValue::fromVariant(text));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbBandChanged(int nrig, QString text)
{
  QSqlQuery query(db);
  query.exec("SELECT * from bands");
  bool found = false;
  while (!found && query.next()) {
    if ( text == query.value("name").toString()) {
      found = true;
      if (query.value("id").toInt() != currentBand[nrig]) { // band changed
        radioBandLabel[nrig]->setText(query.value("name").toString());
        cbBandSetText(nrig, query.value("name").toString()); // update all clients
        currentBand[nrig] = query.value("id").toInt();
        bandChanged(nrig);
        //break;
      }
    }
  }
  if (!found) { // no matching band definition
    if (currentBand[nrig] != 0) {
      currentBand[nrig] = 0;
      radioBandLabel[nrig]->setText("");
      cbBandSetText(nrig, QStringLiteral("")); // update all clients
      bandChanged(nrig);
    }
  }
}

void MainWindow::pbScanStatus(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbScan"));
  object.insert("method", QJsonValue::fromVariant("status"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::pbScanSetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbScan"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::pbScanEnabledStatus(int nrig, QWebSocket *pClient)
{
  bool state = false;
  QSqlQuery query(db);
  query.exec(QString("SELECT * from antennas where id = %1")
                        .arg(currentAntenna[nrig]) );
  if (query.first()) {
    state = query.value("scan").toBool();
  }

  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbScanEnabled"));
  object.insert("method", QJsonValue::fromVariant("status"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::pbLockStatus(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbLock"));
  object.insert("method", QJsonValue::fromVariant("status"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::pbTrackSetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbTrack"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::pbTrackStatus(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("pbTrack"));
  object.insert("method", QJsonValue::fromVariant("status"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbLinkedSetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbLinked"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbLinkedAddItems(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbLinked"));
  object.insert("method", QJsonValue::fromVariant("addItem"));
  QJsonArray items;
  for (int i=0; i<NRIG; ++i) {
    QJsonObject item;
    if (settings->value(s_radioName[i], "").toString().isEmpty()) {
      item.insert("label", QJsonValue::fromVariant(QString::number(i+1)+" -------"));
    } else {
      item.insert("label", QJsonValue::fromVariant(settings->value(s_radioName[i], "").toString()));
    }

    if (settings->value(s_radioEnable[i], s_radioEnable_def).toBool() &&
        i != nrig) {
      item.insert("enabled", QJsonValue::fromVariant(true));
    } else {
      item.insert("enabled", QJsonValue::fromVariant(false));
    }
    items.push_back(item);
  }
  object.insert("items", QJsonValue(items));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbLinkedSetIndex(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbLinked"));
  object.insert("method", QJsonValue::fromVariant("setCurrentIndex"));
  object.insert("index", QJsonValue::fromVariant(currentTrackedRadio[nrig]));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbScanDelaySetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbScanDelay"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbScanDelayAddItems(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbScanDelay"));
  object.insert("method", QJsonValue::fromVariant("addItem"));
  QJsonArray labels;
  //QSqlQuery query(db);
  for (int i=0; i<10; ++i) {
    int delay = i*100 + 100;
    labels.push_back(QJsonValue::fromVariant(QString::number(delay)));
  }
  object.insert("labels", QJsonValue(labels));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbScanDelaySetIndex(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbScanDelay"));
  object.insert("method", QJsonValue::fromVariant("setCurrentIndex"));
  int idx = (currentScanDelay[nrig] - 100) / 100;
  object.insert("index", QJsonValue::fromVariant(idx));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::bearingLabelText(int nrig, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("bearingLabel"));
  object.insert("method", QJsonValue::fromVariant("setText"));
  if (currentBearing[nrig] >= 0) {
    object.insert("text", QJsonValue::fromVariant(QString::number(currentBearing[nrig])));
  } else {
    object.insert("text", QJsonValue::fromVariant(""));
  }
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbGroupSetEnabled(int nrig, bool state, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbGroup"));
  object.insert("method", QJsonValue::fromVariant("setEnabled"));
  object.insert("state", QJsonValue::fromVariant(state));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbGroupAddItems(int nrig, QWebSocket *pClient)
{
  bool found = false;
  QSqlQuery query(db);
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and groups.radios1_4 = 1");
  } else {
    match.append(" and groups.radios5_8 = 1");
  }
  sort.append(" order by groups.priority desc, groups.id desc");
  sql.append(kSelectGroupSQL.arg(currentBand[nrig])
                            .arg(match)
                            .arg(sort)
                            .arg("") );

  //qDebug() << sql;
  query.exec(sql);
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbGroup"));
  object.insert("method", QJsonValue::fromVariant("addItem"));
  QJsonArray labels;
  labels.push_back(QJsonValue::fromVariant(""));
  QString bandText = QStringLiteral("");
  while (query.next()) {
    if (selectAntenna(nrig, false, query.value("id").toInt())) { // only add if has valid antennas
      labels.push_back(QJsonValue::fromVariant(query.value("label").toString()));
      // find current group and re-select
      if (query.value("id").toInt() == currentGroup[nrig]) {
        bandText = query.value("label").toString();
        found = true;
      }
    }
  }
  object.insert("labels", QJsonValue(labels));
  sendRadioWindowData(nrig, object, pClient);

  if (currentGroup[nrig] == 0) {
    found = true;
  }

  // set previously selected group (by id) after combobox updated
  if (found) {
    cbGroupSetText(nrig, bandText, pClient);
  }

  // go to highest priority group if one exists
  if (!found && query.first()) {
    if (selectAntenna(nrig, false, query.value("id").toInt())) { // only add if has valid antennas
      found = true;
      cbGroupSetText(nrig, query.value("label").toString(), pClient);
      radioGroupLabel[nrig]->setText(query.value("label").toString());
      currentGroup[nrig] = query.value("id").toInt();
      groupChanged(nrig);
    }
  }

  if (!found) { // no groups found on this band
    if (currentGroup[nrig] != 0) {
      cbGroupSetText(nrig, QStringLiteral(""), pClient);
      radioGroupLabel[nrig]->setText("");
      currentGroup[nrig] = 0;
      groupChanged(nrig);
    }
  }
}

void MainWindow::cbGroupSetText(int nrig, QString text, QWebSocket *pClient)
{
  QJsonObject object;
  object.insert("object", QJsonValue::fromVariant("cbGroup"));
  object.insert("method", QJsonValue::fromVariant("setCurrentText"));
  object.insert("text", QJsonValue::fromVariant(text));
  sendRadioWindowData(nrig, object, pClient);
}

void MainWindow::cbGroupChanged(int nrig, QString text)
{
  QSqlQuery query(db);
  //query.exec("SELECT * from groups");
  QString sql,match,sort;
  if (nrig < 4) {
    match.append(" and groups.radios1_4 = 1");
  } else {
    match.append(" and groups.radios5_8 = 1");
  }
  sort.append(" order by groups.priority desc, groups.id desc");
  sql.append(kSelectGroupSQL.arg(currentBand[nrig])
                            .arg(match)
                            .arg(sort)
                            .arg("") );

  //qDebug() << sql;
  query.exec(sql);
  bool found = false;
  while (!found && query.next()) {
    if ( text == query.value("label").toString()) {
      found = true;
      if (query.value("id").toInt() != currentGroup[nrig]) { // group changed
        radioGroupLabel[nrig]->setText(query.value("label").toString());
        cbGroupSetText(nrig, query.value("label").toString());
        currentGroup[nrig] = query.value("id").toInt();
        if (trackingState[nrig]) {
          if (currentGroup[nrig] == currentGroup[currentTrackedRadio[nrig]]) {
            setAntennaTracking(nrig, false);
          }
          if (query.value("display_mode").toInt() != kDispCompass) {
            setAntennaTracking(nrig, false);
          }
        }
        groupChanged(nrig);
        //break;
      }
    }
  }
  if (!found) { // group not found
    if (currentGroup[nrig] != 0) {
      currentGroup[nrig] = 0;
      radioGroupLabel[nrig]->setText("");
      cbGroupSetText(nrig, QStringLiteral(""));
      groupChanged(nrig);
    }
  }
}


// SIGNALS
void MainWindow::connectMainWindowSignals()
{

  for (int i=0;i<NRIG;++i) {

    connect(radioManufComboBox[i], static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=](int idx){ populateModelCombo(i, idx); });
    connect(radioEnableCheckBox[i], &QCheckBox::stateChanged, this, [=](){ radioEnableCheckBox_stateChanged(i); });
    connect(rigctldCheckbox[i], &QCheckBox::stateChanged, this, [=](){ rigctldCheckbox_stateChanged(i); });
    connect(radioBandDecoderComboBox[i], static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=](){ radioBandDecoderComboBoxChanged(i); });
    connect(radioCatButton[i], &QPushButton::released, this, [=](){ radioConnection(i); });

  }

  connect(saveSettingsButton, &QPushButton::released, this, &MainWindow::saveSettingsButtonClicked);
  connect(saveSettingsButton_2, &QPushButton::released, this, &MainWindow::saveSettingsButtonClicked);
  connect(rejectChangesButton, &QPushButton::released, this, &MainWindow::rejectSettings);
  connect(rejectChangesButton_2, &QPushButton::released, this, &MainWindow::rejectSettings);
  connect(actionSoftRxAbout, &QAction::triggered, this, &MainWindow::about);
  connect(actionQuit, &QAction::triggered, this, &MainWindow::close);
  connect(databaseResetButton, &QPushButton::released, this, &MainWindow::resetDatabase);
  connect(pbCronStart, &QPushButton::released, this, &MainWindow::cronStart);
  connect(pbCronStop, &QPushButton::released, this, &MainWindow::cronStop);

}

// !SIGNALS


// WEBSOCKET
void MainWindow::socketDisconnected()
{
  QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
  //qDebug() << "disconnect " << pClient->peerAddress().toString() << ":" << pClient->peerPort();
  //statusBarUi->showMessage("Client "+pClient->peerAddress().toString()+":"
  //                         +QString::number(pClient->peerPort())+" disconnected", tmpStatusMsgDelay);
  int nrig = -1;
  if (pClient) {
    for (int i=0; i < m_clients.count(); ++i) {
      if (m_clients.at(i).websocket == pClient) {
        nrig = m_clients.at(i).radio;
        m_clients.removeAt(i);
        serverLog->appendPlainText(QString("[%1] %2:%3 disconnected")
                                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                      .arg(pClient->peerAddress().toString())
                                      .arg(pClient->peerPort()));
        break;
      }
    }
    //m_clients.removeAll(pClient);
    pClient->deleteLater();
  }
  if (nrig >= 0) { // check if any other clients for radio
    int cnt=0;
    for (const auto &client : qAsConst(m_clients)) {
      if (client.radio == nrig) {
        cnt++;
      }
    }
    if (cnt > 0) {
      clientsLabel[nrig]->setText(QString::number(cnt));
    } else { // no other clients registered for radio, do some cleanup
      setAntennaLock(nrig, false);
      setAntennaScanning(nrig, false);
      setAntennaTracking(nrig, false);

      clientsLabel[nrig]->setText("");
      //qDebug() << "No clients left for radio " << nrig+1 << ", cleared lock/scan/tracking states";
      statusBarUi->showMessage(QString("No clients left for radio %1, cleared states")
                                    .arg(nrig+1),
                                    tmpStatusMsgDelay);
    }
  }
}

void MainWindow::onNewConnection()
{
  auto pSocket = webSocketServer->nextPendingConnection();
  pSocket->setParent(this);
  //connect(pSocket, &QWebSocket::textMessageReceived,
  connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &MainWindow::processMessage);
  connect(pSocket, &QWebSocket::disconnected,
            this, &MainWindow::socketDisconnected);
  //m_clients << pSocket;
  //qDebug() << "connect " << pSocket->peerAddress().toString() << ":" << pSocket->peerPort();
  //statusBarUi->showMessage(pSocket->peerAddress().toString()+":"
  //                         +QString::number(pSocket->peerPort())+" connected", tmpStatusMsgDelay);
  serverLog->appendPlainText(QString("[%1] %2:%3 new connection")
                                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                                  .arg(pSocket->peerAddress().toString())
                                  .arg(pSocket->peerPort()));
}


// WEBSOCKET

// ANTENNA SELECTION

void MainWindow::antennaStep(int nrig, bool direction, bool scanable)
{

  setAntennaTracking(nrig, false);

  int display_mode = getDisplayMode(currentGroup[nrig]);
  int coChannelPort = getSwitchPort(currentAntenna[ coChannel[nrig] ]); // antenna switch port of co-channel radio

  QSqlQuery query(db);
  QString sql,match,sort;
  // radio range
  if (nrig < 4) {
    match.append(" and antennas.radios1_4 = 1");
  } else {
    match.append(" and antennas.radios5_8 = 1");
  }
  match.append(QString(" and antennas.switch_port <> %1")
                    .arg(coChannelPort));
  if (display_mode == kDispCompass) {
    if (direction == kNext) {
      sort.append(" order by antennas.start_deg asc");
    } else {
      sort.append(" order by antennas.start_deg desc");
    }
  } else {
    if (direction == kNext) {
      sort.append(" order by antennas.priority desc, antennas.id asc");
    } else {
      sort.append(" order by antennas.priority asc, antennas.id desc");
    }
  }
  sql.append(kSelectAntennaSQL.arg(currentGroup[nrig])
                              .arg(match)
                              .arg(sort) );

  //if (scanable) {
  //  sql.append(" and scan = 1");
  //}
  // fitler out co-channel antenna port conflict
  //qDebug() << coChannel[nrig] + 1 << " " << coChannelPort;

  //qDebug() << sql;
  bool found = false;
  bool foundCurrent = false;
  query.exec(sql);

  while (!found && query.next()) {
    if (foundCurrent && ( (scanable && query.value("scan").toBool()) || !scanable) ) {
      // other conditions?
      found = true;
      currentAntenna[nrig] = query.value("id").toInt();
      //if (display_mode == kDispCompass) {
        currentBearing[nrig] = calcCenterBearing(query.value("start_deg").toInt(), query.value("stop_deg").toInt());
        bearingChanged(nrig);
      //} else {
      //  currentBearing[nrig] = -1;
      //}
      antennaChanged(nrig);
    }
    if (currentAntenna[nrig] == query.value("id").toInt()) { // found current antenna
      foundCurrent = true;
    }
  }
  if (!found) {
    query.seek(QSql::BeforeFirstRow);
  }
  while (!found && query.next()) {
    if (currentAntenna[nrig] != query.value("id").toInt()) { // no action if back to current antenna
      if ( (scanable && query.value("scan").toBool()) || !scanable ) {
        found = true;
        currentAntenna[nrig] = query.value("id").toInt();
        //if (display_mode == kDispCompass) {
          currentBearing[nrig] = calcCenterBearing(query.value("start_deg").toInt(), query.value("stop_deg").toInt());
          bearingChanged(nrig);
        //} else {
        //  currentBearing[nrig] = -1;
        //}
        antennaChanged(nrig);
      }
    } else { // back to original antenna
      break;
    }
  }
  // if no candidates found, keep current antenna selection
}
// ANTENNA SELECTION



int MainWindow::getDisplayMode(int group)
{
  int display_mode = kDispNone;
  QSqlQuery query(db);
  query.exec(QString("SELECT display_mode from groups where id = %1")
                      .arg(group) );
  if (query.first()) {
    display_mode = query.value("display_mode").toInt();
  }
  return display_mode;
}
int MainWindow::getSwitchPort(int antenna)
{
  int port = -1;
  QSqlQuery query(db);
  query.exec(QString("SELECT switch_port from antennas where id = %1")
                      .arg(antenna) );
  if (query.first()) {
    port = query.value("switch_port").toInt();
  }
  return port;
}
