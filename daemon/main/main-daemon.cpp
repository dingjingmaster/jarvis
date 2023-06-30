//
// Created by dingjing on 23-6-30.
//

#include "main-daemon.h"

#include <QTimer>
#include <QDebug>
#include <utility>
#include <QCommandLineParser>

#include "plugin-manager.h"

static bool mFirstParse = true;

MainDaemon::MainDaemon(int argc, char **argv, const char* name)
    : SingletonApp (argc, argv, name)
{
    if (isPrimary()) {
        connect(this, &SingletonApp::receivedMessage, [=, this] (quint32 id, const QByteArray& msg) {
            parseCmd(id, msg, true);
        });
        PluginManager::getInstance()->registerDefaultPlugins();
    }

    auto message = arguments().join(' ').toUtf8();
    parseCmd(instanceId(), message, isPrimary());
}

void MainDaemon::parseCmd(quint32 id, const QByteArray& msg, bool isPrimary)
{
    QCommandLineParser parser;

    QCommandLineOption quitOption(QStringList() << "q" << "quit", tr("Close the graceful daemon"));
    QCommandLineOption restartOption(QStringList() << "r" << "restart", tr("Replace current graceful daemon"));

    parser.addOption(quitOption);
    parser.addOption(restartOption);

    if (isPrimary) {
        if (mFirstParse) {
            auto helpOption = parser.addHelpOption();
            auto versionOption = parser.addVersionOption();
            mFirstParse = false;
        }

        const QStringList args = QString(msg).split(' ');
        parser.process(args);
        if (parser.isSet(quitOption)) {
            PluginManager::getInstance()->managerStop();
            QTimer::singleShot(1, this, [=]() {
                qApp->quit();
            });
            return;
        }

        if (parser.isSet(restartOption)) {
            PluginManager::getInstance()->managerStop();
            PluginManager::getInstance()->managerStart();
            qDebug() << LOG_TAG "restart successful!!!";
            return;
        }
        PluginManager::getInstance()->managerStart();
    }
    else {
        auto helpOption = parser.addHelpOption();
        auto versionOption = parser.addVersionOption();

        if (arguments().count() < 2) {
            parser.showHelp();
        }

        parser.process(arguments());

        sendMessage(msg);
    }

    Q_UNUSED(id)
}
