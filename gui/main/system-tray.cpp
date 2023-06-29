//
// Created by dingjing on 23-6-29.
//

#include "system-tray.h"

SystemTray::SystemTray(QObject *parent)
    : QSystemTrayIcon (parent)
{
    setIcon (QIcon(":/img/icons/tray-icon.png"));
}
