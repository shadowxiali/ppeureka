//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include "ppeureka/error.h"
#include "ppeureka/types.h"
#include "ppeureka/http_status.h"
#include "ppeureka/response.h"
#include "ppeureka/http_client.h"
#include "ppeureka/helpers.h"


namespace ppeureka { namespace agent {

    using TlsConfig = http::impl::TlsConfig;
    using GetResponse = http::impl::Client::GetResponse;

    // RetryFunction
    //   callback function when request has NetError or http code not in (2xx, 4xx).
    //   when NetError, Connect will choose next endpoint to request.
    // Params:
    //   tryCount - has request count, first is 1.
    //   resp - when recv server resp, it is not null; others it is null.
    // Returns:
    //   true - try next request.
    //   false - end request.
    using RetryFunction = std::function<bool(std::size_t tryCount, const GetResponse *resp)>;

    class EurekaConnect
    {
        using HttpMethod = ppeureka::http::impl::HttpMethod;
    public:
        EurekaConnect() = default;

        // connection count set
        void setDefaultConnCount(std::size_t defaultConnCount=3) { m_defaultConnCount = defaultConnCount; };
        void setMaxConnCount(std::size_t maxConnCount=1000) { m_maxConnCount = maxConnCount; };

        // if tls.keyPass valid, it must valid until stop
        void setTls(const TlsConfig &tls) { m_tls = tls; };
        void setEndpoints(const StringList &endpoints) { m_endpoints = endpoints; };
        void setRetryFunction(RetryFunction f) { m_retryFunc = std::move(f); };

        // Exception:
        //    ppeureka::ParamError when parameter error.
        //    ppeureka::Error when others.
        void start();
        // set stop flag, and wait all request end.
        void stop();

        // switch current endpoint
        // Params:
        //   endpointIndex - will % size()
        void switchEndpoint(std::size_t endpointIndex);
        std::string currentEndPoint() const;

        // concurrent request parallel.
        // All net request may be Exception.
        // Exception:
        //    ppeureka::OperationAborted when request cancelled.
        //    ppeureka::ParamError when parameter error.
        //    ppeureka::NetError when net error.
        //    ppeureka::BadStatus when http code not match.
        //    ppeureka::FormatError when json parse fail.
        //    ppeureka::Error when others.

        InstanceInfoPtrDeque queryInsAll();
        InstanceInfoPtrDeque queryInsByAppId(const std::string &appId);
        InstanceInfoPtrDeque queryInsByAppIdInsId(const std::string &appId, const std::string &insId);
        InstanceInfoPtrDeque queryInsByVip(const std::string &vip);
        InstanceInfoPtrDeque queryInsBySVip(const std::string &svip);

        InstanceInfoPtr getEmptyIns(const std::string &appId, const std::string &insId, int port, const std::string &ipAddr=""); 

        void registerIns(const InstanceInfoPtr &ins);
        void unregisterIns(const std::string &appId, const std::string &insId);
        void sendHeart(const std::string &appId, const std::string &insId);
        void statusOutOfService(const std::string &appId, const std::string &insId);
        void statusUp(const std::string &appId, const std::string &insId);
        void updateMetadata(const std::string &appId, const std::string &insId, const std::string &key, const std::string &value);

        // the default retry implement:
        //   when NetError or http code not in (2xx, 4xx), do retry, and when NetError, Connect will choose next endpoint to request.
        // when tryCount great than 2 * endpoints.size(), retry end.
        bool defaultRetry(std::size_t tryCount, const GetResponse *resp);

    private:
        void checkClientValid();
        // Params:
        //   needRetry - out, if http code not in (2xx, 4xx), set true; other false.
        // Return:
        //   true - if http code in (2xx)
        //   false - if not in (2xx)
        bool checkHttpCodeSuc(const GetResponse &resp, bool &needRetry);
        bool doRetry(std::size_t tryCount, const GetResponse *resp);

        

        GetResponse request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr);

    private:
        

        std::size_t      m_defaultConnCount{3};
        std::size_t      m_maxConnCount{1000};

        std::unique_ptr<http::impl::Client> m_client;
        std::atomic<std::size_t>            m_endpointsIndex{0};
        StringList                          m_endpoints;
        http::impl::TlsConfig               m_tls;
        RetryFunction                       m_retryFunc{nullptr};
    };



}}
