//
// Created by dingjing on 23-6-30.
//

#ifndef JARVIS_MAIN_DAEMON_H
#define JARVIS_MAIN_DAEMON_H
#include "3thrd/singleton/singleton-app.h"

class PluginManager;
class MainDaemon : public SingletonApp
{
    Q_OBJECT
public:
    explicit MainDaemon (int argc, char* argv[], const char* name);

protected Q_SLOTS:
    void parseCmd (quint32 id, const QByteArray& msg, bool isPrimary);

private:

};


#endif //JARVIS_MAIN_DAEMON_H
