/*
#
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
#
*/

#ifndef HOTPLUG_H
#define HOTPLUG_H

#include <QObject>
#include <QThread>
#include <QMap>
#include <QDebug>

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>

#undef Bool // fix X11 inc

#define INTERNAL_MONITOR "LVDS"
#define VIRTUAL_MONITOR "VIRTUAL"
#define TURN_OFF_MONITOR "xrandr --output %1 --off"
#define TURN_ON_MONITOR "xrandr --output %1 --auto"
#define XCONFIG "lumina-xconfig --reset-monitors"

#define OCNE(X) ((XRROutputChangeNotifyEvent*)X)

class HotPlug : public QObject
{
    Q_OBJECT

public:
    explicit HotPlug(QObject *parent = 0);
    ~HotPlug();

private:
    QThread t;
    bool _scanning;

signals:
    void status(QString display, bool connected);
    void found(QMap<QString,bool> devices);

public slots:
    void requestScan();
    void requestSetScan(bool scanning);
private slots:
    void scan();
    void getScreens(Display *dpy);
    void setScan(bool scanning);
};

#endif // HOTPLUG_H
