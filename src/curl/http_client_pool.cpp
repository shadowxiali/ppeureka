//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "http_client_pool.h"
#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <functional>
#include <thread>
#include <algorithm>
#include <map>
#include "ppeureka/helpers.h"

namespace {
    using namespace ppeureka::curl;
    using DeferRun = ppeureka::helpers::DeferRun;

    // all HttpClientPool use one timer thread to free empty client
    class PoolGlobalTimerThread
    {
        using CheckFlag = std::atomic<bool>;
        using CheckFlagPtr = std::shared_ptr<CheckFlag>;
    public:
        void incStart(HttpClientPool *poolIns)
        {
            if (!poolIns)
            {
                assert(poolIns);
                return;
            }
            bool hasAdd{false};
            {
                std::lock_guard<std::mutex> al{m_lock};
                auto it = m_runPools.find(poolIns);
                if (it == m_runPools.end())
                {
                    hasAdd = true;
                    m_runPools.emplace(poolIns, std::make_shared<CheckFlag>(false));
                }
            }
            if (!hasAdd)
                return;
            if (++m_count == 1)
            {
                m_stop_flag = false;
                m_timer_thread = std::thread([this](){
                    doTimer();
                });
            }
        }

        void decStop(HttpClientPool *poolIns)
        {
            if (!poolIns)
            {
                assert(poolIns);
                return;
            }

            CheckFlagPtr cfPtr;
            {
                std::lock_guard<std::mutex> al{m_lock};
                auto it = m_runPools.find(poolIns);
                if (it != m_runPools.end())
                {
                    cfPtr = it->second;
                    m_runPools.erase(poolIns);
                }
            }
            if (!cfPtr)
                return;
            while (*cfPtr)
            {
                // wait clear check flag
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            if (--m_count == 0)
            {
                assert(m_runPools.empty());
                m_stop_flag = true;
                m_timer_thread.join();
            }
        }

    private:
        void doTimer()
        {
            while (true)
            {
                int i = 0;
                while (i++ < 30*100 && !m_stop_flag)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                }
                if (m_stop_flag)
                {
                    break;
                }

                checkReleaseClient();
            }
        }
        void checkReleaseClient()
        {
            decltype(m_runPools) curChecks;
            {
                std::lock_guard<std::mutex> al{m_lock};
                for (auto &&st : m_runPools)
                {
                    auto &cf = st.second;
                    *cf = true; // set check flag
                    curChecks.emplace(st);
                }
            }
            DeferRun dr([&](){
                for (auto &&st : curChecks)
                {
                    auto &cf = st.second;
                    *cf = false; // clear check flag
                }
            });

            for (auto &&st : curChecks)
            {
                auto &cf = st.second;
                auto &poolIns = st.first;
                poolIns->checkReleaseClient();
                *cf = false; // clear check flag
            }
        }

    private:
        std::atomic<bool>           m_stop_flag{false};
        std::atomic<std::size_t>    m_count{0};
        std::thread                 m_timer_thread;
        std::mutex                  m_lock;
        std::map<HttpClientPool *, CheckFlagPtr>  m_runPools;
    };
    PoolGlobalTimerThread sTimerThread;
}

namespace ppeureka { namespace curl {

    using namespace ppeureka::http::impl;

    using DeferRun = ppeureka::helpers::DeferRun;

    HttpClientPool::~HttpClientPool()
    {
        stop();
    };

    void HttpClientPool::start(const std::string& endpoint, const TlsConfig& tlsConfig)
    {
        m_stopped = false;

        setEndpoint(endpoint);

        auto al = get_lock();
        m_tls = tlsConfig;
        m_lastCheckMaxUsingCount = 0;

        // match agent, to reduce resouce when instance query in memory.
        // default count
        //while (m_pool.size() < m_defaultConnCount)
        //{
        //    HttpClientPtr cli{create_client()};
        //    cli->start(m_endpoint, m_tls);
        //    m_pool.emplace_back(cli);
        //}

        sTimerThread.incStart(this);
    }

    void HttpClientPool::setEndpoint(const std::string &endpoint)
    {
        auto al = get_lock();
        m_endpoint = endpoint;

        for (auto &&cli : m_pool)
        {
            cli->setEndpoint(endpoint);
        }
    }

    HttpClientPool::GetResponse HttpClientPool::request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data)
    {
        ++m_requesting_count;
        DeferRun dr1([this](){
            --m_requesting_count;
        });

        if (isStopped())
            throw Error("stoped");

        auto cli = getClient();
        if (!cli)
            throw NetError("no client in pool");
        
        DeferRun dr2([this, cli](){
            freeClient(cli);
        });

        return cli->request(method, path, query, data);
    }

    void HttpClientPool::stop()
    {
        m_stopped = true;

        {
            auto al = get_lock();
            for (auto &&cli : m_pool)
            {
                cli->stop();
            }
            for (auto &&cli : m_using)
            {
                cli->stop();
            }
        }
        
        sTimerThread.decStop(this);

        // wait requesting 0
        while (m_requesting_count > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        auto al = get_lock();
        m_pool.clear();
        m_using.clear();
    }

    HttpClientPool::HttpClientPtr HttpClientPool::getClient()
    {
        auto al = get_lock();
        if (m_pool.empty())
        {
            if (m_using.size() + m_pool.size() >= m_maxConnCount)
                throw Error("limit to max conn count");

            HttpClientPtr cli{create_client()};
            cli->start(m_endpoint, m_tls);
            m_pool.emplace_back(cli);
        }
        auto cli = m_pool.front();
        m_pool.pop_front();

        m_using.emplace_back(cli);
        if (m_lastCheckMaxUsingCount < m_using.size())
            m_lastCheckMaxUsingCount = m_using.size();
        
        return cli;
    }

    void HttpClientPool::freeClient(HttpClientPool::HttpClientPtr cli)
    {
        if (!cli)
            return;

        auto al = get_lock();
        auto it = std::find(m_using.begin(), m_using.end(), cli);
        assert(it != m_using.end());
        m_using.erase(it);

        m_pool.emplace_front(cli);
    }

    void HttpClientPool::checkReleaseClient()
    {
       if (isStopped())
       {
           return;
       }

       decltype(m_pool) tmpPool;
       {
           auto al = get_lock();
           std::size_t lastCheckMaxUsingCount = m_lastCheckMaxUsingCount;
           m_lastCheckMaxUsingCount = 0;
           auto keepCount = lastCheckMaxUsingCount + lastCheckMaxUsingCount/2 + 1;
           keepCount = keepCount > m_defaultConnCount ? keepCount : m_defaultConnCount;
           
           while (m_pool.size() > keepCount)
           {
               auto cli = m_pool.back();
               m_pool.pop_back();
               tmpPool.emplace_back(cli);
           }
       }
    }
}}
