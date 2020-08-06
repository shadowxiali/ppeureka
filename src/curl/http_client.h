//  Copyright (c)  2014-2017 Andrey Upadyshev <oliora@gmail.com>
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


namespace ppeureka { namespace curl {

    namespace detail
    {
        struct CurlEasyDeleter
        {
            void operator() (CURL *handle) const PPEUREKA_NOEXCEPT
            {
                curl_easy_cleanup(handle);
            }
        };
    }

    class HttpClient: public ppeureka::http::impl::Client
    {
    public:
        using GetResponse = std::tuple<http::Status, ResponseHeaders, std::string>;
        using TlsConfig = ppeureka::http::impl::TlsConfig;
        using HttpMethod = ppeureka::http::impl::HttpMethod;

        using lock_type = std::mutex;
        using auto_lock_type = std::unique_lock<lock_type>;

        HttpClient() = default;

        // == Client interface ==
        virtual ~HttpClient() override;
        void setEndpoint(const std::string &endpoint) override;
        void start(const std::string& endpoint, const TlsConfig& tlsConfig) override;
        GetResponse request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr) override;
        void stop() override;
        // == Client interface ==

        bool isStopped() const { return m_stopped.load(std::memory_order_relaxed); }

        HttpClient(const HttpClient&) = delete;
        HttpClient(HttpClient&&) = delete;
        HttpClient& operator= (const HttpClient&) = delete;
        HttpClient& operator= (HttpClient&&) = delete;

    private:
        void setupTls(const ppeureka::http::impl::TlsConfig& tlsConfig);

        auto_lock_type get_lock_param() const { return auto_lock_type{m_lock_param}; }
        auto_lock_type get_lock_request() const { return auto_lock_type{m_lock_request}; }

        template<class Opt, class T>
        void setopt(Opt opt, const T& t);

        std::string makeUrl(const std::string& path, const std::string& query) const { return ppeureka::http::impl::makeUrl(m_endpoint, path, query); }

        CURL *handle() const PPEUREKA_NOEXCEPT { return m_handle.get(); }

        void perform();

        mutable lock_type  m_lock_request;
        mutable lock_type  m_lock_param;
        std::string m_endpoint;
        std::unique_ptr<CURL, detail::CurlEasyDeleter> m_handle;
        char m_errBuffer[CURL_ERROR_SIZE]; // Replace with unique_ptr<std::array<char, CURL_ERROR_SIZE>> if moving is needed
        bool m_enableStop{true};
        std::atomic_bool m_stopped{false};
    };

}}
