//
// Created by dingjing on 23-6-29.
//
#include "3thrd/singleton/singleton-app-gui.h"

#include <gio/gio.h>

#include <QTextCodec>

#include "common/c-log.h"
#include "main/main-window.h"
#include "main/system-tray.h"


void messageOutput (QtMsgType type, const QMessageLogContext& context, const QString& msg);


int main (int argc, char* argv[])
{
    qInstallMessageHandler (messageOutput);

#if DEBUG
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, nullptr, LOG_TAG, nullptr);
#else
    log_init(LOG_TYPE_CONSOLE, LOG_INFO, LOG_ROTATE_FALSE, -1, nullptr, LOG_TAG, nullptr);
#endif

    logi("start progress " LOG_TAG "... ");

    QCoreApplication::setApplicationVersion (PACKAGE_VERSION);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QTextCodec::setCodecForLocale (QTextCodec::codecForName ("utf-8"));

    SingletonApp app (argc, argv, PACKAGE_NAME"-gui");

    logi("system tray icon is available: %s", SystemTray::isSystemTrayAvailable() ? "true" : "false");

    MainWindow mw;
    mw.show();

    int ret = SingletonApp::exec();

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