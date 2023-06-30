//
// Created by dingjing on 23-6-30.
//

#ifndef JARVIS_PLUGIN_MANAGER_H
#define JARVIS_PLUGIN_MANAGER_H
#include <QObject>

#include <QMap>
#include <QLibrary>

#include "daemon-iface/plugin-iface.h"

class PluginManager : QObject
{
    Q_OBJECT
public:
    static PluginManager* getInstance();
    ~PluginManager() override;
    PluginManager(PluginIface&)=delete;
    PluginManager* operator= (const PluginManager&)=delete;

    void registerDefaultPlugins ();
    void registerPlugin (const std::shared_ptr<PluginIface>& p);

private:
    PluginManager();

public Q_SLOTS:
    void managerStop ();
    void managerStart ();

private:
    QMap<QString, std::shared_ptr<QLibrary>>        mPlugins1;
    QMap<QString, std::shared_ptr<PluginIface>>     mPlugins2;
    static PluginManager*                           gInstance;
};


#endif //JARVIS_PLUGIN_MANAGER_H
