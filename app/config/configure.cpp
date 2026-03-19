/*
 * Copyright (c) 2026 dingjing
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "configure.h"

#include <QFile>
#include <QSettings>
#include <QSharedMemory>

#include <sys/stat.h>



static inline void create_config_file (const char* configFile)
{
    QFile f(configFile);
    if (!f.exists()) {
        f.open(QFile::NewOnly | QFile::ReadWrite | QFile::Text);
        f.close();
    }
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
}

Configure Configure::gInstance;

class ConfigurePrivate
{
#define CONFIG_MAIN                     INSTALL_DIR"/config/config.ini"
public:
    ~ConfigurePrivate();
    explicit ConfigurePrivate(Configure* parent);

    /* CONFIG_MAIN 配置文件读写 */
    QVariant getConfigValue (const QString& group, const QString& key, const QVariant& defaultValue=QVariant()) const;
    void setConfigValue (const QString& group, const QString& key, const QVariant& value) const;
    void lockConfig() const;
    void unlockConfig() const;

private:
    Configure*                          q_ptr = nullptr;
    QSettings*                          mSettings = nullptr;
    QSharedMemory*                      mSharedMem = nullptr;
};

ConfigurePrivate::~ConfigurePrivate()
{
    mSharedMem->detach();
    delete mSharedMem;

    delete mSettings;
    q_ptr = nullptr;
}

ConfigurePrivate::ConfigurePrivate(Configure* parent)
    : q_ptr(parent),
    mSettings(new QSettings(CONFIG_MAIN, QSettings::IniFormat)),
    mSharedMem(new QSharedMemory(CONFIG_MAIN))
{
    if (0 == getuid()) {
        const mode_t mask = umask(0);
        create_config_file(CONFIG_MAIN);
        umask(mask);
    }

    if (!mSharedMem->attach(QSharedMemory::ReadWrite)) {
        mSharedMem->create(8);
    }
}

QVariant ConfigurePrivate::getConfigValue(const QString& group, const QString& key, const QVariant& defaultValue) const
{
    lockConfig();
    mSettings->sync();
    mSettings->beginGroup(group);
    auto val = mSettings->value(key, defaultValue);
    mSettings->endGroup();
    mSettings->sync();
    unlockConfig();

    return val;
}

void ConfigurePrivate::setConfigValue(const QString& group, const QString& key, const QVariant& value) const
{
    lockConfig();
    mSettings->sync();
    mSettings->beginGroup(group);
    mSettings->setValue(key, value);
    mSettings->endGroup();
    mSettings->sync();
    unlockConfig();
}

void ConfigurePrivate::lockConfig() const
{
    mSharedMem->lock();
}

void ConfigurePrivate::unlockConfig() const
{
    mSharedMem->unlock();
}

Configure& Configure::getInstance()
{
    return gInstance;
}

qint32 Configure::getMysqlPort()
{
    Q_D(Configure);

    return d->getConfigValue("mysql", "port", 3306).toInt();
}

QString Configure::getMysqlDB()
{
    Q_D(Configure);

    return d->getConfigValue("mysql", "db").toString();
}

QString Configure::getMysqlHost()
{
    Q_D(Configure);

    return d->getConfigValue("mysql", "host").toString();
}

QString Configure::getMysqlUser()
{
    Q_D(Configure);

    return d->getConfigValue("mysql", "user").toString();
}

QString Configure::getMysqlPassword()
{
    Q_D(Configure);

    return d->getConfigValue("mysql", "passwd").toString();
}

Configure::Configure(QObject* parent)
    : QObject(parent), d_ptr(new ConfigurePrivate(this))
{
}

Configure::~Configure()
{
    delete d_ptr;
    d_ptr = nullptr;
}
