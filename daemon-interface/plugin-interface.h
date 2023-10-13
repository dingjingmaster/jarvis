//
// Created by dingjing on 23-6-30.
//

#ifndef JARVIS_PLUGIN_INTERFACE_H
#define JARVIS_PLUGIN_INTERFACE_H
#include <QObject>
/**
 * @brief
 *
 */

typedef struct RequestData      RequestData;

struct RequestData
{
    QString             pluginName;         // 数据要传递给哪个 插件
    QByteArray          data;               // 传递的具体数据，这个数据是 protobuf 类型的，每个插件支持一种 protobuf 类型
};

class Q_DECL_EXPORT PluginInterface : public QObject
{
    Q_OBJECT
public:
    PluginInterface() = default;

    /**
     * @brief
     *  name 必须是唯一的
     */
    virtual QString name () = 0;
    virtual bool activate () = 0;
    virtual bool deactivate () = 0;
    virtual QString author () {return {};}
    virtual QString describe () {return {};}

    /**
     * @brief
     *  此处是将数据发送到具体插件，数据由插件处理，然后返回处理结果
     */
    virtual RequestData receiveRequest (QByteArray&) = 0;
};

//extern "C" Q_DECL_EXPORT PluginInterface* getInstance();

#endif //JARVIS_PLUGIN_INTERFACE_H
