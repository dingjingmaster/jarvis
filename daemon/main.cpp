//
// Created by dingjing on 23-6-30.
//
#include <QCoreApplication>

#include <QDebug>
#include <QTextCodec>

#include "common/c-log.h"
#include "main/main-daemon.h"


void messageOutput (QtMsgType type, const QMessageLogContext& context, const QString& msg);

int main (int argc, char* argv[])
{
#if DEBUG
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, nullptr, LOG_TAG, nullptr);
#else
    log_init(LOG_TYPE_FILE, LOG_INFO, LOG_ROTATE_FALSE, 2<<30, "/tmp/", LOG_TAG, "log");
#endif

    qInstallMessageHandler (messageOutput);

    logi("start progress " LOG_TAG "... ");

    QCoreApplication::setApplicationVersion (PACKAGE_VERSION);
    QTextCodec::setCodecForLocale (QTextCodec::codecForName ("utf-8"));

    MainDaemon app(argc, argv, PACKAGE_NAME "-daemon");

    int ret = MainDaemon::exec();

    logi(LOG_TAG " exited code: %d!", ret);

    return ret;
}

void messageOutput (QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    log_level_t level = LOG_DEBUG;
    unsigned int line = context.line;
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";

    switch (type) {
        case QtWarningMsg: {
            level = LOG_WARNING;
            break;
        }
        case QtFatalMsg:
        case QtCriticalMsg: {
            level = LOG_ERR;
            break;
        }
        case QtInfoMsg: {
            level = LOG_INFO;
            break;
        }
        case QtDebugMsg:
        default: {
            level = LOG_DEBUG;
            break;
        }
    }
    log_print(level, LOG_TAG, file, (int) line, function, "%s", msg.toUtf8().constData());
}
