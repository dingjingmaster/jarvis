//
// Created by dingjing on 8/24/22.
//

#ifndef JARVIS_FUTURE_H
#define JARVIS_FUTURE_H

#include <future>
#include <chrono>
#include <utility>

#include "global.h"
#include "../core/common-scheduler.h"

template<typename RES>
class Future
{
public:
    Future(std::future<RES>&& fr)
        : mFuture(std::move(fr))
    {
    }

    Future() = default;
    Future(const Future&) = delete;
    Future(Future&& move) = default;

    Future& operator=(const Future&) = delete;
    Future& operator=(Future&& move) = default;

    void wait() const;

    template<class REP, class PERIOD>
    std::future_status waitFor(const std::chrono::duration<REP, PERIOD>& timeDuration) const;

    template<class CLOCK, class DURATION>
    std::future_status waitUntil(const std::chrono::time_point<CLOCK, DURATION>& timeoutTime) const;

    RES get()
    {
        this->wait();
        return this->mFuture.get();
    }

    bool valid() const { return this->mFuture.valid(); }

private:
    std::future<RES>                mFuture;
};

template<typename RES>
class Promise
{
public:
    Promise() = default;
    Promise(const Promise& promise) = delete;
    Promise(Promise&& move) = default;
    Promise& operator=(const Promise& promise) = delete;
    Promise& operator=(Promise&& move) = default;

    Future<RES> getFuture()
    {
        return Future<RES>(this->mPromise.get_future());
    }

    void setValue(const RES& value) { this->mPromise.setValue(value); }
    void setValue(RES&& value) { this->mPromise.set_value(std::move(value)); }

private:
    std::promise<RES>                   mPromise;
};

template<typename RES>
void Future<RES>::wait() const
{
    if (this->mFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        bool in_handler = Global::getScheduler()->isHandlerThread();

        if (in_handler)
            Global::syncOperationBegin();

        this->mFuture.wait();
        if (in_handler)
            Global::syncOperationEnd();
    }
}

template<typename RES>
template<class REP, class PERIOD>
std::future_status Future<RES>::waitFor(const std::chrono::duration<REP, PERIOD>& timeDuration) const
{
    std::future_status status = std::future_status::ready;

    if (this->mFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        bool in_handler = Global::getScheduler()->isHandlerThread();

        if (in_handler)
            Global::syncOperationBegin();

        status = this->mFuture.wait_for(timeDuration);
        if (in_handler)
            Global::syncOperationEnd();
    }

    return status;
}

template<typename RES>
template<class CLOCK, class DURATION>
std::future_status Future<RES>::waitUntil(const std::chrono::time_point<CLOCK, DURATION>& timeoutTime) const
{
    std::future_status status = std::future_status::ready;

    if (this->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        bool inHandler = Global::getScheduler()->isHandlerThread();

        if (inHandler)
            Global::syncOperationBegin();

        status = this->future.wait_until(timeoutTime);
        if (inHandler)
            Global::syncOperationEnd();
    }

    return status;
}

///// WFFuture<void> template specialization
template<>
inline void Future<void>::get()
{
    this->wait();
    this->mFuture.get();
}

template<>
class Promise<void>
{
public:
    Promise() = default;
    Promise(const Promise& promise) = delete;
    Promise(Promise&& move) = default;
    Promise& operator=(const Promise& promise) = delete;
    Promise& operator=(Promise&& move) = default;

    Future<void> getFuture()
    {
        return Future<void>(this->mPromise.get_future());
    }

    void setValue() { this->mPromise.set_value(); }
//	void set_value(const RES& value) { this->promise.set_value(value); }
//	void set_value(RES&& value) { this->promise.set_value(std::move(value)); }

private:
    std::promise<void>              mPromise;
};

#endif //JARVIS_FUTURE_H
