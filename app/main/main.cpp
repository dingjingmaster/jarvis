//
// Created by dingjing on 8/26/22.
//
#include <signal.h>
#include <execinfo.h>

#include "../core/c-log.h"
#include "manager/facilities.h"
#include "../modules/http-server.h"
#include "../spider/spider-manager.h"
#include "http-router.h"

static Facilities::WaitGroup waitGroup(1);

static void signal_handler (int sig, siginfo_t* siginfo, void* context);

int main (int argc, char* argv[])
{
#if DEBUG
    log_init(LOG_TYPE_CONSOLE, LOG_DEBUG, LOG_ROTATE_FALSE, -1, NULL, LOG_TAG, NULL);
#else
    log_init(LOG_TYPE_FILE, LOG_INFO, LOG_ROTATE_FALSE, -1, "/tmp/", LOG_TAG, "log");
#endif
    logi("start progress " LOG_TAG "... ");

    // 注册信号处理函数
    static struct sigaction sigAction;
    sigAction.sa_sigaction = *signal_handler;
    sigAction.sa_flags |= SA_SIGINFO;

    if (sigaction(SIGSEGV, &sigAction, NULL)) {
        loge("sigaction error: %s", strerror(errno));
        return errno;
    }

    //SpiderManager::instance()->runAll();

#ifdef DEBUG
    int port = 8889;
#else
    int port = 8888;
#endif
    HttpServer server ([] (HttpTask* task) {
        // 静态资源请求，请求成功则直接返回
        if (!strcmp(HTTP_METHOD_GET, task->getReq()->getMethod())
            && HttpRouter::getInstance()->responseStaticResource(task)) {
            return;
        }
        else if (HttpRouter::getInstance()->responseDynamicResource(task)) {
            return;
        }
    });

    if (0 == server.start(port)) {
        logi("http server 'http://127.0.0.1:%d' is running!", port);
    }

    waitGroup.wait();

    logi("stop progress " LOG_TAG "!");

    return 0;
}

static void signal_handler (int sig, siginfo_t* siginfo, void* context)
{
    void*   bt[256] = {0};
    char** btSymbols = nullptr;

    int bufSize = 0;
    char buf[256000] = {0};

    int n = backtrace (bt, sizeof(bt) / sizeof(bt[0]));
    btSymbols = backtrace_symbols(bt, n);
    for (int i = 0; i < n; ++i) {
        char fileName[2048] = {0};
        char p[12] = {0};
        sscanf(btSymbols[i], "%*[^(]%[^)]s", fileName);
        sscanf(fileName, "(+%s", p);
        memset(fileName, 0, sizeof(fileName));
        if (strlen(p) > 0) {
            snprintf(fileName, sizeof(fileName) - 1, "addr2line %s -e %s", p, INSTALL_NAME);
            FILE* fr = popen(fileName, "r");
            memset(fileName, 0, sizeof(fileName));
            fread(fileName, sizeof(fileName) - 1, 1, fr);
            fclose(fr);
            for (int j = 0; j < sizeof(fileName); ++j) {
                if ('\n' == fileName[j] || 0 == fileName[j]) {
                    fileName[j] = 0;
                    break;
                }
            }
        }

        int fl = strlen(fileName);
        int l = strlen(btSymbols[i]);
        if (bufSize + l + fl + 2 >= sizeof(buf) - 1) {
            break;
        }

        strncat(buf + bufSize, btSymbols[i], sizeof(buf) - bufSize - 1);
        bufSize += l;
        strncat(buf + bufSize, " ", sizeof(buf) - bufSize - 1);
        bufSize += 1;
        strncat(buf + bufSize, fileName, sizeof(buf) - bufSize - 1);
        bufSize += fl;
        strncat(buf + bufSize, "\n", sizeof(buf) - bufSize - 1);
        bufSize += 1;
    }

    loge("\n%s", buf);

    free(btSymbols);
    signal(sig, SIG_DFL);
    raise(sig);
}
