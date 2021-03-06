/*
# Copyright (c) 2018, Ole-André Rodlie <ole.andre.rodlie@gmail.com> All rights reserved.
#
# Available under the 3-clause BSD license
# See the LICENSE file for full details
*/

#include "systray.h"

SysTray::SysTray(QObject *parent)
    : QObject(parent)
    , tray(0)
    , man(0)
    , pm(0)
    , ss(0)
    , ht(0)
    , wasLowBattery(false)
    , lowBatteryValue(LOW_BATTERY)
    , critBatteryValue(CRITICAL_BATTERY)
    , hasService(false)
    , lidActionBattery(LID_BATTERY_DEFAULT)
    , lidActionAC(LID_AC_DEFAULT)
    , criticalAction(CRITICAL_DEFAULT)
    , autoSleepBattery(AUTO_SLEEP_BATTERY)
    , autoSleepAC(0)
    , timer(0)
    , timeouts(0)
    , showNotifications(true)
    , desktopSS(true)
    , desktopPM(true)
    , showBatteryPercent(true)
    , showTray(true)
    , disableLidACOnExternalMonitors(true)
    , disableLidBatteryOnExternalMonitors(true)
{
    // setup tray
    tray = new QSystemTrayIcon(QIcon::fromTheme(DEFAULT_BATTERY_ICON, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON))), this);
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    if (tray->isSystemTrayAvailable()) { tray->show(); }

    // setup manager
    man = new Power(this);
    connect(man, SIGNAL(updatedDevices()), this, SLOT(checkDevices()));
    connect(man, SIGNAL(closedLid()), this, SLOT(handleClosedLid()));
    connect(man, SIGNAL(openedLid()), this, SLOT(handleOpenedLid()));
    connect(man, SIGNAL(switchedToBattery()), this, SLOT(handleOnBattery()));
    connect(man, SIGNAL(switchedToAC()), this, SLOT(handleOnAC()));

    // setup org.freedesktop.PowerManagement
    pm = new PowerManagement();
    connect(pm, SIGNAL(HasInhibitChanged(bool)), this, SLOT(handleHasInhibitChanged(bool)));
    connect(pm, SIGNAL(update()), this, SLOT(loadSettings()));

    // setup org.freedesktop.ScreenSaver
    ss = new ScreenSaver();

    // setup monitor hotplug watcher
    ht = new HotPlug();
    qRegisterMetaType<QMap<QString,bool> >("QMap<QString,bool>");
    connect(ht, SIGNAL(status(QString,bool)), this, SLOT(handleDisplay(QString,bool)));
    connect(ht, SIGNAL(found(QMap<QString,bool>)), this, SLOT(handleFoundDisplays(QMap<QString,bool>)));
    ht->requestScan();

    // setup timer
    timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start();

    // load settings and register service
    loadSettings();
    registerService();
    QTimer::singleShot(1000, this, SLOT(checkDevices()));
}

SysTray::~SysTray()
{
    man->deleteLater();
    pm->deleteLater();
    ss->deleteLater();
    ht->deleteLater();
}

// what to do when user clicks systray, at the moment nothing
void SysTray::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    Q_UNUSED(reason)
    /*switch(reason) {
    case QSystemTrayIcon::Context:
    case QSystemTrayIcon::Trigger:
        //if (menu->actions().size()>0) { menu->popup(QCursor::pos()); }
    default:;
    }*/
}

void SysTray::checkDevices()
{
    if (tray->isSystemTrayAvailable() && !tray->isVisible() && showTray) { tray->show(); }
    if (!showTray && tray->isVisible()) { tray->hide(); }

    // get battery left and add tooltip
    double batteryLeft = man->batteryLeft();
    tray->setToolTip(tr("Battery at %1%").arg(batteryLeft));
    if (batteryLeft==100) { tray->setToolTip(tr("Charged")); }
    if (!man->onBattery() && batteryLeft<100) { tray->setToolTip(tray->toolTip().append(tr(" (Charging)"))); }

    // draw battery systray
    drawBattery(batteryLeft);

    // critical battery?
    if (batteryLeft<=(double)critBatteryValue && man->onBattery()) { handleCritical(); }

    // Register service if not already registered
    if (!hasService) { registerService(); }
}

// what to do when user open/close lid
void SysTray::handleClosedLid()
{

    qDebug() << "enabled video output" << monitors;
    qDebug() << "internal monitor connected?" << internalMonitorIsConnected();
    qDebug() << "has external monitor?" << externalMonitorIsConnected();

    int type = lidNone;
    if (man->onBattery()) {  // on battery
        type = lidActionBattery;
        if (disableLidBatteryOnExternalMonitors && externalMonitorIsConnected()) {
            qDebug() << "external monitor is connected, ignore lid action";
            return;
        }
    } else { // on ac
        type = lidActionAC;
        if (disableLidACOnExternalMonitors && externalMonitorIsConnected()) {
            qDebug() << "external monitor is connected, ignore lid action";
            return;
        }
    }
    switch(type) {
    case lidLock:
        man->lockScreen();
        break;
    case lidSleep:
        man->suspend();
        break;
    case lidHibernate:
        man->hibernate();
        break;
    default: ;
    }
}

// do something when lid is opened
void SysTray::handleOpenedLid()
{
    // do nothing
}

// do something when switched to battery power
void SysTray::handleOnBattery()
{
    if (showNotifications && tray->isVisible()) {
        tray->showMessage(tr("On Battery"), tr("Switched to battery power."));
    }
}

// do something when switched to ac power
void SysTray::handleOnAC()
{
    if (showNotifications && tray->isVisible()) {
        tray->showMessage(tr("On AC"), tr("Switched to AC power."));
    }
}

// load default settings
void SysTray::loadSettings()
{
    if (Common::validPowerSettings("autoSleepBattery")) {
        autoSleepBattery = Common::loadPowerSettings("autoSleepBattery").toInt();
    }
    if (Common::validPowerSettings("autoSleepAC")) {
        autoSleepAC = Common::loadPowerSettings("autoSleepAC").toInt();
    }
    if (Common::validPowerSettings("lowBattery")) {
        lowBatteryValue = Common::loadPowerSettings("lowBattery").toInt();
    }
    if (Common::validPowerSettings("criticalBattery")) {
        critBatteryValue = Common::loadPowerSettings("criticalBattery").toInt();
    }
    if (Common::validPowerSettings("lidBattery")) {
        lidActionBattery = Common::loadPowerSettings("lidBattery").toInt();
    }
    if (Common::validPowerSettings("lidAC")) {
        lidActionAC = Common::loadPowerSettings("lidAC").toInt();
    }
    if (Common::validPowerSettings("criticalAction")) {
        criticalAction = Common::loadPowerSettings("criticalAction").toInt();
    }
    if (Common::validPowerSettings("desktop_ss")) {
        desktopSS = Common::loadPowerSettings("desktop_ss").toBool();
    }
    if (Common::validPowerSettings("desktop_pm")) {
        desktopPM = Common::loadPowerSettings("desktop_pm").toBool();
    }
    if (Common::validPowerSettings("tray_notify")) {
        showNotifications = Common::loadPowerSettings("tray_notify").toBool();
    }
    if (Common::validPowerSettings("show_battery_percent")) {
        showBatteryPercent = Common::loadPowerSettings("show_battery_percent").toBool();
    }
    if (Common::validPowerSettings("show_tray")) {
        showTray = Common::loadPowerSettings("show_tray").toBool();
    }
    if (Common::validPowerSettings("disable_lid_action_battery_external_monitor")) {
        disableLidBatteryOnExternalMonitors = Common::loadPowerSettings("disable_lid_action_battery_external_monitor").toBool();
    }
    if (Common::validPowerSettings("disable_lid_action_ac_external_monitor")) {
        disableLidACOnExternalMonitors = Common::loadPowerSettings("disable_lid_action_ac_external_monitor").toBool();
    }

    qDebug() << "no lid action ac external monitor" << disableLidACOnExternalMonitors;
    qDebug() << "no lid action battery external monitor" << disableLidBatteryOnExternalMonitors;
    qDebug() << "show tray" << showTray;
    qDebug() << "battery percent" << showBatteryPercent;
    qDebug() << "tray notify" << showNotifications;
    qDebug() << "desktop ss" << desktopSS;
    qDebug() << "desktop pm" << desktopPM;
    qDebug() << "auto sleep battery" << autoSleepBattery;
    qDebug() << "auto sleep ac" << autoSleepAC;
    qDebug() << "low battery setting" << lowBatteryValue;
    qDebug() << "critical battery setting" << critBatteryValue;
    qDebug() << "lid battery" << lidActionBattery;
    qDebug() << "lid ac" << lidActionAC;
    qDebug() << "critical action" << criticalAction;
}

// register session service
void SysTray::registerService()
{
    if (hasService) { return; }
    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning("Cannot connect to D-Bus.");
        return;
    }
    if (desktopPM) {
    if (!QDBusConnection::sessionBus().registerService(PM_SERVICE)) {
        qWarning() << QDBusConnection::sessionBus().lastError().message();
        return;
    }
        if (!QDBusConnection::sessionBus().registerObject(PM_PATH, pm, QDBusConnection::ExportAllContents)) {
        qWarning() << QDBusConnection::sessionBus().lastError().message();
        return;
    }
        qDebug() << "Enabled org.freedesktop.PowerManagement";
    }
    if (desktopSS) {
        if (!QDBusConnection::sessionBus().registerService(SS_SERVICE)) {
            qWarning() << QDBusConnection::sessionBus().lastError().message();
            return;
        }
        if (!QDBusConnection::sessionBus().registerObject(SS_PATH, ss, QDBusConnection::ExportAllContents)) {
            qWarning() << QDBusConnection::sessionBus().lastError().message();
            return;
        }
        qDebug() << "Enabled org.freedesktop.ScreenSaver";
    }
    hasService = true;
}

// dbus session inhibit status handler
void SysTray::handleHasInhibitChanged(bool has_inhibit)
{
    qDebug() << "HasInhibitChanged?" << has_inhibit;
    if (has_inhibit) { resetTimer(); }
}

// handle critical battery
void SysTray::handleCritical()
{
    qDebug() << "critical battery level, action?" << criticalAction;
    switch(criticalAction) {
    case criticalHibernate:
        man->hibernate();
        break;
    case criticalShutdown:
        qDebug() << "feature not added!"; // TODO!!!!
        break;
    default: ;
    }
}

// draw battery percent over tray icon
void SysTray::drawBattery(double left)
{
    if (!showTray && tray->isVisible()) {
        tray->hide();
        return;
    }
    if (tray->isSystemTrayAvailable() && !tray->isVisible() && showTray) { tray->show(); }

    QIcon icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON)));
    if (left<=(double)lowBatteryValue && man->onBattery()) {
        icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_LOW, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_LOW)));
        if (!wasLowBattery) { tray->showMessage(tr("Low Battery!"), tr("You battery is almost empty, please consider connecting your computer to a power supply.")); }
        wasLowBattery = true;
    } else {
        wasLowBattery = false;
        if (left<=(double)lowBatteryValue) { // low (on ac)
            qDebug() << "low on ac";
            icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_LOW_AC, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_LOW_AC)));
        } else if (left<=critBatteryValue) { // critical
            qDebug() << "critical";
            if (man->onBattery()) {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_CRIT, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_CRIT)));
            } else {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_CRIT_AC, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_CRIT_AC)));
            }
        } else if (left>(double)lowBatteryValue && left<90) { // good
            qDebug() << "good";
            if (man->onBattery()) {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_GOOD, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_GOOD)));
            } else {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_GOOD_AC, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_GOOD_AC)));
            }
        } else if (left>=90 && left<99) { // almost full
            qDebug() << "almost full";
            if (man->onBattery()) {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_FULL, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_FULL)));
            } else {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_FULL_AC, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_FULL_AC)));
            }
        } else if(left>=99) { // full
            qDebug() << "full";
            if (man->onBattery()) {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_FULL, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_FULL)));
            } else {
                icon = QIcon::fromTheme(DEFAULT_BATTERY_ICON_CHARGED, QIcon(QString(":/icons/%1.png").arg(DEFAULT_BATTERY_ICON_CHARGED)));
            }
        } else {
            qDebug() << "something else";
            // TODO
        }
    }

    if (left > 99 || left == 0 || !man->onBattery() || !showBatteryPercent) {
        tray->setIcon(icon);
        return;
    }

    QPixmap pixmap = icon.pixmap(QSize(24, 24));
    QPainter painter(&pixmap);
    painter.setPen(QColor(Qt::black));
    painter.drawText(pixmap.rect().adjusted(1, 1, 1, 1), Qt::AlignCenter, QString("%1").arg(left));
    painter.setPen(QColor(Qt::white));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QString("%1").arg(left));
    tray->setIcon(pixmap);
}

// timeout, check if idle
// timeouts and xss must be >= user value and service has to be empty before auto sleep
void SysTray::timeout()
{
    if (!showTray && tray->isVisible()) { tray->hide(); }
    if (tray->isSystemTrayAvailable() && !tray->isVisible() && showTray) { tray->show(); }

    qDebug() << "timeout?" << timeouts;
    qDebug() << "XSS?" << xIdle();
    qDebug() << "inhibit?" << pm->HasInhibit();

    int autoSleep = 0;
    if (man->onBattery()) { autoSleep = autoSleepBattery; }
    else { autoSleep = autoSleepAC; }

    bool doSleep = false;
    if (autoSleep>0 && timeouts>=autoSleep && xIdle()>=autoSleep && !pm->HasInhibit()) { doSleep = true; }
    if (!doSleep) { timeouts++; }
    else {
        timeouts = 0;
        man->suspend();
    }
}

// get user idle time
int SysTray::xIdle()
{
    long idle = 0;
    Display *display = XOpenDisplay(0);
    if (display != 0) {
        XScreenSaverInfo *info = XScreenSaverAllocInfo();
        XScreenSaverQueryInfo(display, DefaultRootWindow(display), info);
        if (info) {
            idle = info->idle;
            XFree(info);
        }
    }
    XCloseDisplay(display);
    int hours = idle/(1000*60*60);
    int minutes = (idle-(hours*1000*60*60))/(1000*60);
    return minutes;
}

// reset the idle timer
void SysTray::resetTimer()
{
    timeouts = 0;
}

void SysTray::handleDisplay(QString display, bool connected)
{
    qDebug() << display << connected;
    if (monitors[display] == connected) { return; }

    bool wasConnected = monitors[display];
    monitors[display] = connected;
    if (wasConnected && !connected) {
        // Turn off monitor using xrandr when disconnected.
        // This is needed else we end up with a virtual screen with the apps from that screen.
        qDebug() << "remove screen" << display;
        QProcess::startDetached(QString(TURN_OFF_MONITOR).arg(display));
    } else if (!wasConnected && connected) {
        // Turn on monitor using xrandr, then launch lumina-xconfig
        qDebug() << "add screen" << display;
        QProcess::startDetached(QString(TURN_ON_MONITOR).arg(display));
        QProcess::startDetached(XCONFIG);
    }
}

void SysTray::handleFoundDisplays(QMap<QString, bool> displays)
{
    qDebug() << displays;
    monitors = displays;
}

bool SysTray::internalMonitorIsConnected()
{
    QMapIterator<QString, bool> i(monitors);
    while (i.hasNext()) {
        i.next();
        if (i.key().startsWith(INTERNAL_MONITOR)) {
            qDebug() << "internal monitor connected?" << i.key() << i.value();
            return i.value();
        }
    }
    return false;
}

bool SysTray::externalMonitorIsConnected()
{
    QMapIterator<QString, bool> i(monitors);
    while (i.hasNext()) {
        i.next();
        if (!i.key().startsWith(INTERNAL_MONITOR) && !i.key().startsWith(VIRTUAL_MONITOR)) {
            qDebug() << "external monitor connected?" << i.key() << i.value();
            if (i.value()) { return true; }
        }
    }
    return false;
}
