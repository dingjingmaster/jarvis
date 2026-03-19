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
#include "db.h"

#include <QDebug>

#include "ormpp/dbng.hpp"
#include "ormpp/mysql.hpp"
#include "../config/configure.h"
#include "ormpp/connection_pool.hpp"


class DBPrivate
{
public:
    ~DBPrivate();
    DBPrivate(DB* parent);

public:
    // 暂时默认使用 mysql
    ormpp::dbng<ormpp::mysql>                           mMysql;
    ormpp::connection_pool<ormpp::dbng<ormpp::mysql>>*  mPool;

private:
    DB*                                 q_ptr = nullptr;
};

DBPrivate::~DBPrivate()
{
    q_ptr = nullptr;
}

DBPrivate::DBPrivate(DB* parent)
    : q_ptr(parent)
{
}

DB DB::gInstance;

DB& DB::getInstance()
{
    return gInstance;
}

bool DB::init()
{
    Q_D(DB);

    QString db = Configure::getInstance().getMysqlDB();
    QString host = Configure::getInstance().getMysqlHost();
    QString user = Configure::getInstance().getMysqlUser();
    QString password = Configure::getInstance().getMysqlPassword();
    qint32 port = Configure::getInstance().getMysqlPort();


    if (!d->mMysql.connect(host.toStdString(), user.toStdString(), password.toStdString(), db.toStdString(), 3, port)) {
        qWarning() << "Failed to connect to database, " << host << ":" << user << ":" << password << ":" << port;
        return false;
    }

    d->mPool = &ormpp::connection_pool<ormpp::dbng<ormpp::mysql>>::instance();
    d->mPool->init(8, host.toStdString(), user.toStdString(), password.toStdString(), db.toStdString(), 5, port);
    if (8 != d->mPool->size()) {
        qWarning() << "Failed to init connect pool";
        return false;
    }

    return true;
}

DB::DB(QObject* parent)
    : QObject(parent), d_ptr(new DBPrivate(this))
{
}

DB::~DB()
{
    delete d_ptr;
    d_ptr = nullptr;
}
