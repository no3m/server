/*! Copyright 2010-2021 R. Torsten Clay N4OGW

This file is part of so2sdr.

so2sdr is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

so2sdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with so2sdr.  If not, see <http://www.gnu.org/licenses/>.

*/

/*!
Modifications by NO3M for softRX
2021-05-08
*/

#include "serial.hpp"

// need to define this internal hamlib function
extern "C" HAMLIB_EXPORT(int) write_block(hamlib_port_t *p, const char *txbuffer, size_t count);

// initialize statics
QList<hamlibmfg> RigSerial::mfg;
QList<QByteArray> RigSerial::mfgName;

/*! comparison for radio manufacturer class. Compare by name
*/
bool hamlibmfg::operator<(const hamlibmfg &other) const
{
  return (mfg_name<other.mfg_name);
}

/*! comparison for rig models within a manufacturer. Alphabetize by name
*/
bool hamlibModel::operator<(const hamlibModel &other) const
{
  return (model_name<other.model_name);
}

RigSerial::RigSerial(int nr)
{
  nrig=nr;

  // turn off all the debug messages coming from hamlib
  rig_set_debug_level(RIG_DEBUG_NONE);

  // load all backends and step through them. list_caps function defined below.
  // only radio 1 will do this, since one static copy is kept for both radios
  if (nr==0) {
    rig_load_all_backends();
    rig_list_foreach(list_caps,nullptr);

    // sort list by manufacturer name
    std::sort(mfg.begin(),mfg.end());
    std::sort(mfgName.begin(),mfgName.end());

    // sort list of rigs for each manuacturer
    for (int i=0;i<mfg.size();i++) {
      std::sort(mfg[i].models.begin(),mfg[i].models.end());
    }
  }
  settings=nullptr;
  rig=nullptr;
  socket=nullptr;
  radioOK=false;
  rigFreq=0;
  rigPtt=false;
  model=1;
  lock.unlock();
}

/*! static function passed to rig_list_foreach
*
* see hamlib examples rigctl.c
*/
int RigSerial::list_caps(const struct rig_caps *caps, void *data)
{
  Q_UNUSED(data)

  hamlibModel newrig;
  newrig.model_name = caps->model_name;
  newrig.model_nr   = caps->rig_model;
  if (!mfgName.contains(caps->mfg_name)) {
    mfgName.append(caps->mfg_name);

    hamlibmfg newmfg;
    newmfg.mfg_name  = caps->mfg_name;
    newmfg.models.clear();
    newmfg.models.append(newrig);
    mfg.append(newmfg);
  } else {
    int indx=0;
    for (int j = 0; j < mfg.size(); j++) {
      if (mfg.at(j).mfg_name == caps->mfg_name) {
        indx = j;
        break;
      }
    }
    mfg[indx].models.append(newrig);
  }
  return -1;
}


/*! figure out the manufacturer and model index for combo boxes
given the hamlib model number
*/
void RigSerial::hamlibModelLookup(int hamlib_nr, int &indx_mfg, int &indx_model) const
{
  // not the fastest way to search...
  bool found = false;
  for (int i = 0; i < mfg.size(); i++) {
    for (int j = 0; j < mfg.at(i).models.size(); j++) {
      if (hamlib_nr == mfg.at(i).models.at(j).model_nr) {
        found      = true;
        indx_mfg   = i;
        indx_model = j;
        break;
      }
    }
    if (found) break;
  }
  if (!found) {
    indx_mfg   = 0;
    indx_model = 0;
  }
}

/*! number of radio manufacturers defined in hamlib

*/
int RigSerial::hamlibNMfg() const
{
  return(mfg.size());
}

QString RigSerial::hamlibMfgName(int i) const
{
  if (i >= 0 && i < mfg.size()) {
    return(mfg.at(i).mfg_name);
  } else {
    return("");
  }
}

QString RigSerial::hamlibModelName(int i, int indx) const
{
  if (i >= 0 && i < mfg.size()) {
    if (indx >= 0 && indx < mfg.at(i).models.size()) {
      return(mfg.at(i).models.at(indx).model_name);
    }
  }
  return("");
}

int RigSerial::hamlibNModels(int i) const
{
  if (i >= 0 && i < mfg.size()) {
    return(mfg.at(i).models.size());
  } else {
    return(0);
  }
}

/*! returns hamlib model number

indx1=index in mfg combo box
indx2=index in model combo box
*/
int RigSerial::hamlibModelIndex(int indx1, int indx2) const
{
  if (indx1 >= 0 && indx1 < mfg.size()) {
    if (indx2 >= 0 && indx2 < mfg.at(indx1).models.size()) {
      return(mfg.at(indx1).models.at(indx2).model_nr);
    }
  }
  return(0);
}

RigSerial::~RigSerial()
{
  closeRig();
  rig_cleanup(rig);
  if (socket) delete socket;
  if (settings) delete settings;
}


/*!
Called when thread started
*/
void RigSerial::run()
{
  if (!settings) settings=new QSettings("softrx", "settings");
  openRig();
  openSocket();
  connect(&timer, &QTimer::timeout, this, &RigSerial::timeoutTimer);
  timer.start(settings->value(s_radioPollTime[nrig],s_radioPollTime_def).toInt());
  //qDebug() << nrig << " " << timer.thread();
}

/*! stop timers
*/
void RigSerial::stopSerial()
{
  timer.stop();
}

void RigSerial::timeoutTimer()
{
  // using rigctld for this radio
  if (settings->value(s_rigctld[nrig],s_rigctld_def).toBool() &&
                                   socket->isOpen()) {

    socket->write(";\\get_freq\n");
    socket->write(";\\get_ptt\n");

  } else {
    // using hamlib over serial port
    // poll frequency and ptt status from radio
    if (radioOK) {
      freq_t freq;
      int status = rig_get_freq(rig, RIG_VFO_CURR, &freq);
      if (status == RIG_OK) {
        double ff = Hz(freq);
        if (ff != 0.0) rigFreq = ff;
      }
      ptt_t ptt;
      status = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
      if (status == RIG_OK) {
        rigPtt = (bool)ptt;
      }
    }
  }
}

/*! returns true if radio opened successfully
*/
bool RigSerial::radioOpen()
{
  lock.lockForRead();
  bool b = radioOK;
  lock.unlock();
  return(b);
}

/*! returns current radio frequency in Hz
*/
double RigSerial::getRigFreq()
{
  lock.lockForRead();
  double f = rigFreq;
  lock.unlock();
  return(f);
}

bool RigSerial::getRigPtt()
{
  lock.lockForRead();
  bool p = rigPtt;
  lock.unlock();
  return(p);
}

void RigSerial::closeSocket()
{
  if (socket) {
    if (socket->isOpen()) socket->close();
    disconnect(socket,nullptr,nullptr,nullptr);
  }
}

/*! initialize TcpSocket for rigctld
*/
void RigSerial::openSocket()
{
  closeSocket();
  if (settings->value(s_rigctld[nrig],s_rigctld_def).toBool()) {
    radioOK=true;
    if (!socket) socket=new QTcpSocket();

    // QHostAddress doesn't understand "localhost"
    if (settings->value(s_rigctldIp[nrig],s_rigctldIp_def).toString().simplified()=="localhost") {
      socket->connectToHost(QHostAddress::LocalHost,
        settings->value(s_rigctldPort[nrig],s_rigctldPort_def).toInt());
      } else {
        socket->connectToHost(QHostAddress(settings->value(s_rigctldIp[nrig],s_rigctldIp_def).toString()).toString(),
        settings->value(s_rigctldPort[nrig],s_rigctldPort_def).toInt());
      }
      connect(socket, &QTcpSocket::readyRead, this, &RigSerial::rxSocket);
      #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
      connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this, &RigSerial::tcpError);
      #else
      connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::errorOccurred), this, &RigSerial::tcpError);
      #endif

    }
  }

  /*! initialize radio
  *
  * this should run in the new thread
  */
  void RigSerial::openRig()
  {
    radioOK      = false;
    model=settings->value(s_radioModel[nrig],s_radioModel_def).toInt();
    if (rig) {
      rig_close(rig);
      rig_cleanup(rig);
    }
    rig = rig_init(model);
    token_t t = rig_token_lookup(rig, "rig_pathname");
    rig_set_conf(rig,t,settings->value(s_radioSerialPort[nrig],s_radioSerialPort_def).toString().toLatin1().data());
    rig->state.rigport.parm.serial.rate=settings->value(s_radioBaudRate[nrig],s_radioBaudRate_def).toInt();
    //t = rig_token_lookup(rig,"ptt_type");
    rigFreq = 0;
    int r = rig_open(rig);
    if (model == RIG_MODEL_DUMMY) {
      rig_set_freq(rig, RIG_VFO_A, rigFreq);
      rig_set_vfo(rig, RIG_VFO_A);
    }
    if (r == RIG_OK) {
      radioOK = true;
    } else {
      emit(radioError("ERROR: radio "+QString::number(nrig+1)+" could not be opened"));
      radioOK = false;
      rig_close(rig);
    }
  }

  /*! shut down radio interface
  */
  void RigSerial::closeRig()
  {
    rig_close(rig);
    rig_cleanup(rig);
    rig=nullptr;
    radioOK = false;
    rigFreq = 0;
    rigPtt = false;
  }

  /*! send a raw byte string to the radio: careful, there is no checking here!
  */
  void RigSerial::sendRaw(QByteArray cmd)
  {
    if (!cmd.contains('<')) {
      write_block(&rig->state.rigport,cmd.data(),cmd.size());
    } else {
      // numbers inside "< >" will be interpreted as hexadecimal bytes
      QByteArray data;
      data.clear();
      int i0=0;
      do {
        int i1=cmd.indexOf("<",i0);
        if (i1!=-1) {
          data=data+cmd.mid(i0,i1-i0);
        } else {
          break;
        }
        int i2=cmd.indexOf(">",i1);
        if (i2==-1 || (i2-i1)!=3) break;
        QByteArray hex=cmd.mid(i1+1,i2-i1-1);
        bool ok=false;
        char c=hex.toInt(&ok,16);
        if (ok) {
          data=data+c;
        }
        i0=i2+1;
      } while (i0<cmd.size());
      if ((i0+1)<cmd.size()) {
        data=data+cmd.mid(i0,-1);
      }
      write_block(&rig->state.rigport,data.data(),data.size());
    }
  }

  /*!
  * \brief RigSerial::rxSocket
  * \param nrig
  *
  * Parse data coming from rigctld
  */
  void RigSerial::rxSocket()
  {
    const QByteArray cmdNames[] = { "get_freq:",
    "get_ptt:" };
    const int nCmdNames=2;

    radioOK=true;
    lock.lockForWrite();
    while (socket->bytesAvailable()) {
      QByteArray data=socket->readAll();
      QList<QByteArray> cmdList=data.split(';');
      bool ok;
      double f;
      bool p;
      for (int i = 0; i < nCmdNames; i++) {
        if (cmdList.at(0)==cmdNames[i]) {
          switch (i) {
            case 0: // "get_freq:;Frequency: 28009360;RPRT 0"
            cmdList[1].remove(0,10);
            f=cmdList[1].toDouble(&ok);
            if (ok) rigFreq=f;
            break;
            case 1: // "get_ptt:;PTT: 0;RPRT 0"
            cmdList[1].remove(0,4);
            p=cmdList[1].toInt(&ok);
            if (ok) rigPtt=(bool)p;
            break;
          }
        }
      }
    }
    lock.unlock();
  }

  /*! Slot called on error of tcpsocket (rigctld) */
  void RigSerial::tcpError(QAbstractSocket::SocketError e)
  {
    Q_UNUSED(e)
    radioOK=false;
    emit(radioError("ERROR: Rigctld radio "+QString::number(nrig+1)+" "+ socket->errorString().toLatin1()));
  }
