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

#pragma once

#include "defines.hpp"

class hamlibModel
{
public:
    QByteArray model_name;
    int        model_nr;
    bool operator<(const hamlibModel &other) const;
};

class hamlibmfg
{
public:
    QByteArray         mfg_name;
    QList<hamlibModel> models;
    bool operator<(const hamlibmfg &other) const;
};

class QSettings;
class QTcpSocket;

/*!
   Radio serial communications for both radios using Hamlib library.

   note that this class will run in its own QThread
 */
class RigSerial : public QObject
{
Q_OBJECT

public:
    RigSerial(int);
    ~RigSerial();
    double getRigFreq();
    bool getRigPtt();
    QString hamlibModelName(int i, int indx) const;
    int hamlibNMfg() const;
    int hamlibNModels(int i) const;
    int hamlibModelIndex(int, int) const;
    void hamlibModelLookup(int, int&, int&) const;
    QString hamlibMfgName(int i) const;
    bool radioOpen();
    void sendRaw(QByteArray cmd);

    static QList<hamlibmfg>        mfg;
    static QList<QByteArray>       mfgName;

signals:
    void radioError(const QString &);

public slots:
    void run();
    void stopSerial();
    void closeRig();
    void closeSocket();

protected:

private slots:
    void rxSocket();
    void tcpError(QAbstractSocket::SocketError e);

private:
    static int list_caps(const struct rig_caps *caps, void *data);

    void openRig();
    void openSocket();
    void timeoutTimer();

    bool             radioOK;
    double           rigFreq;
    bool             rigPtt;
    int              model;
    int              nrig;
    QReadWriteLock   lock;
    RIG              *rig;
    QSettings        *settings;
    QTcpSocket       *socket;
    QTimer           timer{this};
};
