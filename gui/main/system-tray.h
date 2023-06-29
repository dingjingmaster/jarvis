//
// Created by dingjing on 23-6-29.
//

#ifndef JARVIS_SYSTEM_TRAY_H
#define JARVIS_SYSTEM_TRAY_H
#include <QSystemTrayIcon>

class SystemTray : public QSystemTrayIcon
{
    Q_OBJECT
public:
    explicit SystemTray (QObject* parent=nullptr);

};


#endif //JARVIS_SYSTEM_TRAY_H
