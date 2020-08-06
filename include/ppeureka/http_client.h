//  Copyright (c)  2014-2017 Andrey Upadyshev <oliora@gmail.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/http_status.h"
#include "ppeureka/response.h"
#include <tuple>
#include <string>


namespace ppeureka { namespace http { namespace impl {

    enum HttpMethod
    {
        METHOD_GET = 0,
        METHOD_POST,
        METHOD_PUT,
        METHOD_DELETE,
    };

    struct TlsConfig
    {
        TlsConfig() = default;

        std::string cert;
        std::string certType;
        std::string key;
        std::string keyType;
        std::string caPath;
        std::string caInfo;
        bool verifyPeer = true;
        bool verifyHost = true;
        bool verifyStatus = false;

        // Note that keyPass is c-str rather than std::string. That's to make it possible
        // to keep the actual password in a specific location like in protected memory or
        // wiped-afer use memory block and so on.
        // if set to pool, keyPass must valid until pool stop.
        const char *keyPass = nullptr;
    };

    class Client
    {
    public:
        using GetResponse = std::tuple<http::Status, ResponseHeaders, std::string>;

        virtual void start(const std::string& endpoint, const TlsConfig& tlsConfig) = 0;

        // update endpoint
        // if a pool, all request later will use the newer endpoint
        virtual void setEndpoint(const std::string &endpoint) = 0;

        // when method in (POST,PUT), data should be set.
        //   if a pool, concurrent requests will use different client and reqeusts parallel.
        //   if not pool, concurrent requests will sequential.
        // Returns {status, headers, body}
        // Exception:
        //    ppeureka::OperationAborted when request cancelled.
        //    ppeureka::ParamError when parameter error.
        //    ppeureka::NetError when net error.
        //    ppeureka::Error when others.
        virtual GetResponse request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr) = 0;

        // stop only set stop flag, not sync stop request
        virtual void stop() = 0;

        virtual ~Client() {};
    };

    // new client, need delete
    Client *create_client();

    // new client, need delete
    Client *create_client_pool(std::size_t defaultConnCount=3, std::size_t maxConnCount=1000);
}}}
