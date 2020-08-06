//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include <list>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <assert.h>

namespace sync_list {

template<typename VALUE_TYPE, class ALLOC_TYPE = std::allocator<VALUE_TYPE> >
class sync_list
{
public:
    using LIST_TYPE = std::list<VALUE_TYPE, ALLOC_TYPE>;
    using value_type = VALUE_TYPE;
    using allocator_type = ALLOC_TYPE;
    using size_type = typename LIST_TYPE::size_type;
    using lock_type = std::mutex;
    using auto_lock_type = std::unique_lock<lock_type>;

    sync_list() = default;

    auto_lock_type get_lock() const {
        return auto_lock_type{m_lock};
    }

    // !! must get_lock first
    LIST_TYPE &get_data() {
        return m_data;
    }

    LIST_TYPE clear() {
        auto al = get_lock();
        LIST_TYPE tmp = std::move(m_data);
        return tmp;
    }

    void notify_pop_wait() {
        m_wait.notify_all();
    }

    // if set false, emplace_front/back will fail
    //    default is true.
    void enable_emplace(bool enable) {
        m_enable_emplace = enable;
    }
    bool is_enable_emplace() const { 
        return m_enable_emplace; 
    }

    // if set false, pop_front/back will fail(all pop waiting will be notified and return fail)
    //    default is true.
    void enable_pop(bool enable) {
        m_enable_pop = enable;
        if (!enable)
            notify_pop_wait();
    }
    bool is_enable_pop() const { 
        return m_enable_pop; 
    }

    size_type size() const {
        auto al = get_lock();
        return m_data.size();
    }

    template<class... _Valty>
    bool emplace_front(_Valty&&... _Val) {   // insert element at beginning
        if (!m_enable_emplace)
            return false;
        auto al = get_lock();
        m_data.emplace_front(std::forward<_Valty>(_Val)...);
        m_wait.notify_one();
        return true;
    }

    template<class... _Valty>
    bool emplace_back(_Valty&&... _Val) {   // insert element at end
        if (!m_enable_emplace)
            return false;
        auto al = get_lock();
        m_data.emplace_back(std::forward<_Valty>(_Val)...);
        m_wait.notify_one();
        return true;
    }

    template<class _Rep, class _Period>
    bool pop_front(value_type &v, const std::chrono::duration<_Rep, _Period>& dur) {
        if (!m_enable_pop)
            return false;
        auto al = get_lock();
        m_wait.wait_for(al, dur);
        if (!m_enable_pop)
            return false;
        if (m_data.empty())
            return false;
        v = std::move(m_data.front());
        m_data.pop_front();
        return true;
    }

    template<class _Rep, class _Period>
    bool pop_back(value_type &v, const std::chrono::duration<_Rep, _Period>& dur) {
        if (!m_enable_pop)
            return false;
        auto al = get_lock();
        m_wait.wait_for(al, dur);
        if (!m_enable_pop)
            return false;
        if (m_data.empty())
            return false;
        v = std::move(m_data.back());
        m_data.pop_back();
        return true;
    }

private:
    std::atomic<bool>       m_enable_emplace{true};
    std::atomic<bool>       m_enable_pop{true};
    mutable lock_type       m_lock;
    std::condition_variable m_wait;
    LIST_TYPE               m_data;
};



class job_thread 
{
    enum {
        NO_STOP = 0,   // do not stop
        STOP_WAIT = 1,  // stop and wait all jobs done
        STOP_NO_WAIT = 2,   // stop and no wait
    };

public:
    using job_object = std::function<void(void)>;
    using job_list = sync_list<job_object>;
public:
    job_thread() = default;

    // !! ensure start/add_thread/stop in one thread call

    void start(std::size_t thread_count=1) {
        m_stop_flag = NO_STOP;

        m_jobs.enable_emplace(true);
        m_jobs.enable_pop(true);

        add_thread(thread_count);
    }

    void add_thread(std::size_t thread_count=1) {
        if (NO_STOP != m_stop_flag) {
            return;
        }
        thread_count = thread_count == 0 ? 1 : thread_count;
        thread_count = thread_count > 1000 ? 1000 : thread_count;
        
        for (std::size_t i = 0; i < thread_count; ++i) {
            m_threads.emplace_back(std::thread([this](){
                run();
            }));
        }
    }

    void stop(bool wait_all_job=true) {
        m_stop_flag = STOP_WAIT;
        m_jobs.enable_emplace(false); // disable push now
        if (!wait_all_job) {
            m_stop_flag = STOP_NO_WAIT;
            m_jobs.enable_pop(false);
            m_jobs.clear();
        }
        m_jobs.notify_pop_wait();
        for (auto &&thd : m_threads) {
            thd.join();
        }
        m_threads.clear();
    }

    // wait jobs run to empty
    void wait_empty() {
        while (size() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

    std::size_t size() const {
        return m_jobs.size();
    }

    std::size_t thread_count() const {
        return m_threads.size();
    }

    template<class... _Valty>
    bool emplace_front(_Valty&&... _Val) {
        return m_jobs.emplace_front(std::forward<_Valty>(_Val)...);
    }

    template<class... _Valty>
    bool emplace_back(_Valty&&... _Val) {
        return m_jobs.emplace_back(std::forward<_Valty>(_Val)...);
    }

    job_list &get_jobs() {
        return m_jobs;
    }

private:
    void run() {
        while (true) {
            job_object job;
            int milli = NO_STOP == m_stop_flag  ? 1000 : 1;
            std::chrono::milliseconds dur{milli};
            if (m_jobs.pop_back(job, dur)) {
                if (job) {
                    job();
                }
                if (STOP_NO_WAIT == m_stop_flag) {
                    break;
                }
            }
            else if (NO_STOP != m_stop_flag) {
                // no job poped and stop flag, break
                break;
            }
        }
    }

private:
    std::atomic<int>        m_stop_flag{NO_STOP};
    job_list                m_jobs;
    std::list<std::thread>  m_threads;
};


}