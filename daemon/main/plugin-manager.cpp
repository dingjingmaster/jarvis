//
// Created by dingjing on 23-6-30.
//

#include "plugin-manager.h"

#include <glib.h>
#include <csetjmp>
#include "common/c-log.h"

static jmp_buf env;
PluginManager* PluginManager::gInstance = nullptr;

static void recv_sigsegv (int sig);

void PluginManager::managerStop()
{
    logd("plugin manager stop...");

    for (auto p = mPlugins2.constBegin(); p != mPlugins2.constEnd(); ++p) {
        int r = sigsetjmp(env, 1);
        if (0 == r) {
            signal (SIGSEGV, recv_sigsegv);
            p.value()->deactivate();
            logi("plugin: '%s' deactivated!", p.value()->name().toUtf8().constData());
        }
        else {
            loge("plugin: '%s', deactivated SIGSEGV!", p.key().toUtf8().constData());
        }
        signal (SIGSEGV, SIG_DFL);
    }

    for (auto p = mPlugins1.constBegin(); p != mPlugins1.constEnd(); ++p) {
        int r = sigsetjmp(env, 1);
        if (0 == r) {
            signal (SIGSEGV, recv_sigsegv);

            typedef PluginInterface* (*createPlugin) ();
            auto pp = (createPlugin)p.value()->resolve("getInstance");
            if (!pp) {
                loge("plugin: %s resolve 'getInstance' error!", p.key().toUtf8().constData());
                continue;
            }

            pp()->deactivate();
            logi("plugin: '%s' deactivated!", pp()->name().toUtf8().constData());
        }
        else {
            loge("The plugin '%s' deactivated SIGSEGV", p.key().toUtf8().constData());
            continue;
        }
        signal (SIGSEGV, SIG_DFL);
    }
    logd("plugin manager stopped!");
}

void PluginManager::managerStart()
{
    g_autoptr (GError) error = nullptr;

    logd("plugin manager start");

    GDir* dir = g_dir_open (PLUGINS_DIR, 0, &error);
    if (nullptr == dir) {
        loge("%s", error->message);
        return;
    }

    const char* name;
    while ((name = g_dir_read_name(dir))) {
        g_autofree char* filename = g_build_filename(PLUGINS_DIR, name, NULL);

        auto lib = std::make_shared<QLibrary>();
        lib->setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::ExportExternalSymbolsHint);
        if (!lib->load()) {
            loge("load '%s' error: %s", filename, lib->errorString().toUtf8().constData());
            continue;
        }

        typedef PluginInterface* (*createPlugin) ();
        auto p = (createPlugin)lib->resolve("getInstance");
        if (!p) {
            loge ("create module class failed, error: '%s'", lib->errorString().toUtf8().data());
            continue;
        }

        {
            int r = sigsetjmp(env, 1);
            if (0 == r) {
                signal (SIGSEGV, recv_sigsegv);
                if (mPlugins1.contains(p()->name())) {
                    logw("plugin '%s' -- '%s' has loaded!", p()->name().toUtf8().constData(), filename);
                    continue;
                }
                p()->activate();
                logi("plugin: '%s' activated!", p()->name().toUtf8().constData());
            }
            else {
                loge("The plugin '%s' execute SIGSEGV", filename);
                continue;
            }
            signal (SIGSEGV, SIG_DFL);
        }
        mPlugins1[name] = lib;
    }

    g_dir_close (dir);

    for (auto p = mPlugins2.constBegin(); p != mPlugins2.constEnd(); ++p) {
        int r = sigsetjmp(env, 1);
        if (0 == r) {
            signal (SIGSEGV, recv_sigsegv);
            p.value()->activate();
            logi("plugin: '%s' activated!", p.value()->name().toUtf8().constData());
        }
        else {
            loge("plugin: '%s', activated SIGSEGV!", p.key().toUtf8().constData());
        }
        signal (SIGSEGV, SIG_DFL);
    }

    logi("plugins load finished!");
}

PluginManager::PluginManager()
{

}

PluginManager::~PluginManager()
{
    logd("");
    managerStop();
    mPlugins1.clear();
    mPlugins2.clear();
}

PluginManager *PluginManager::getInstance()
{
    static gint64 lc = 0;

    if (g_once_init_enter(&lc)) {
        if (!gInstance) {
            gInstance = new PluginManager;
            g_once_init_leave(&lc, 1);
        }
    }

    return gInstance;
}

void PluginManager::registerPlugin(const std::shared_ptr<PluginInterface>& p)
{
    logd("register plugin");

    int r = sigsetjmp(env, 1);
    if (0 == r) {
        signal (SIGSEGV, recv_sigsegv);
        if (mPlugins2.contains(p->name())) {
            logw("plugin '%s' has loaded!", p->name().toUtf8().constData());
            goto end;
        }
        mPlugins2[p->name()] = p;
        logi("plugin: '%s' registered!", p->name().toUtf8().constData());
    }

end:
    signal (SIGSEGV, SIG_DFL);
}

void PluginManager::registerDefaultPlugins()
{

}

static void recv_sigsegv (int sig)
{
    siglongjmp (env, 1);
}
