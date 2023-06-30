//
// Created by dingjing on 23-6-30.
//

#ifndef JARVIS_PLUGIN_IFACE_H
#define JARVIS_PLUGIN_IFACE_H
#include <QObject>
/**
 * @brief
 *
 */

class Q_DECL_EXPORT PluginIface : public QObject
{
    Q_OBJECT
public:
    PluginIface() = default;
    virtual ~PluginIface() = default;

    virtual QString name () = 0;
    virtual bool activate () = 0;
    virtual bool deactivate () = 0;
    virtual QString author () {return {};}
    virtual QString describe () {return {};}
};

//extern "C" Q_DECL_EXPORT PluginInterface* getInstance();

#endif //JARVIS_PLUGIN_IFACE_H
