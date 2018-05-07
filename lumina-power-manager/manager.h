/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#ifndef MANAGER_H
#define MANAGER_H

#include <QObject>
#include <QMap>
#include <QtDBus>

class Device : public QObject
{
    Q_OBJECT
public:
    explicit Device(const QString block, QObject *parent = NULL);
    QString name;
    QString path;
    bool isRechargable;
    bool isPresent;
    double percentage;
    bool online;
    bool hasPowerSupply;
    bool isBattery;
    bool isAC;
    QString vendor;
    QString nativePath;
    double capacity;
    double energy;
    double energyFullDesign;
    double energyFull;
    double energyEmpty;
private:
    QDBusInterface *dbus;
signals:
    void deviceChanged(QString devicePath);
public slots:
private slots:
    void updateDeviceProperties();
    void handlePropertiesChanged();
};

class Manager : public QObject
{
    Q_OBJECT
public:
    explicit Manager(QObject *parent = NULL);
    QMap<QString,Device*> devices;
private:
    QDBusInterface *dbus;
    QTimer timer;
    bool wasDocked;
    bool wasLidClosed;
    bool wasOnBattery;
signals:
    void updatedDevices();
    void closedLid();
    void openedLid();
    void switchedToBattery();
    void switchedToAC();
    //void lowBattery(bool low);
public slots:
    bool isDocked();
    bool lidIsPresent();
    bool lidIsClosed();
    //bool onLowBattery();
    bool onBattery();
    bool canHibernate();
    bool canSuspend();
    double batteryLeft();
    void suspend();
    void hibernate();
private slots:
    void setupDBus();
    void scanDevices();
    void deviceAdded(const QDBusObjectPath &obj);
    void deviceRemoved(const QDBusObjectPath &obj);
    void deviceChanged();
    void handleDeviceChanged(QString devicePath);
    void checkUPower();
    void notifyResume();
    void notifySleep();
};

#endif // MANAGER_H