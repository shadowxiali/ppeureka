//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <ppeureka/http_client.h>
#include "http_helpers.h"
#include <curl/curl.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <list>

namespace ppeureka { namespace curl {

    class HttpClientPool: public ppeureka::http::impl::Client
    {
        using HttpClientPtr = std::shared_ptr<ppeureka::http::impl::Client>;

    public:
        using GetResponse = std::tuple<http::Status, ResponseHeaders, std::string>;
        using TlsConfig = ppeureka::http::impl::TlsConfig;
        using HttpMethod = ppeureka::http::impl::HttpMethod;

        using lock_type = std::mutex;
        using auto_lock_type = std::unique_lock<lock_type>;

        HttpClientPool(std::size_t defaultConnCount, std::size_t maxConnCount)
            : m_defaultConnCount(defaultConnCount)
            , m_maxConnCount(maxConnCount) {
        }

        // == Client interface ==
        virtual ~HttpClientPool() override;
        void setEndpoint(const std::string &endpoint) override;
        void start(const std::string& endpoint, const TlsConfig& tlsConfig) override;
        GetResponse request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr) override;
        void stop() override;
        // == Client interface ==

        bool isStopped() const { return m_stopped.load(std::memory_order_relaxed); }

        HttpClientPool(const HttpClientPool&) = delete;
        HttpClientPool(HttpClientPool&&) = delete;
        HttpClientPool& operator= (const HttpClientPool&) = delete;
        HttpClientPool& operator= (HttpClientPool&&) = delete;

        void checkReleaseClient();

    private:
        auto_lock_type get_lock() const { return auto_lock_type{m_lock}; }

        HttpClientPtr getClient();
        void freeClient(HttpClientPtr cli);

        std::atomic_bool m_stopped{false};
        std::size_t      m_defaultConnCount{1};
        std::size_t      m_maxConnCount{1000};

        std::size_t      m_lastCheckMaxUsingCount{0};

        mutable lock_type        m_lock;
        std::string      m_endpoint;
        TlsConfig        m_tls;

        std::atomic<std::size_t>    m_requesting_count{0};
        std::list<HttpClientPtr>    m_pool;
        std::list<HttpClientPtr>    m_using;
    };

}}
