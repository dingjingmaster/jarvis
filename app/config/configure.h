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
#ifndef Jarvis_JARVIS_CONFIGURE_H
#define Jarvis_JARVIS_CONFIGURE_H

#include <QMap>
#include <QString>
#include <QVariant>


class ConfigurePrivate;
class Configure : private QObject
{
    Q_OBJECT
public:
    static Configure& getInstance();

    qint32 getMysqlPort();
    QString getMysqlDB();
    QString getMysqlHost();
    QString getMysqlUser();
    QString getMysqlPassword();

private:
    explicit Configure(QObject* parent = nullptr);
    ~Configure() override;

private:
    ConfigurePrivate*       d_ptr;
    static Configure        gInstance;
    Q_DECLARE_PRIVATE(Configure);
};



#endif // Jarvis_JARVIS_CONFIGURE_H