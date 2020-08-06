//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include "ppeureka/eureka_connect.h"
#include "s11n_types.h"
#include "all_clients.h"
#include "ppeureka/helpers.h"
#include <time.h>
#include <thread>

namespace {
    using namespace ppeureka;
    using namespace ppeureka::agent;

    inline InstanceInfoPtrDeque toAppsInstances(const GetResponse &resp)
    {
        // {"applications": {
        auto &&json_str = std::get<2>(resp);
        auto json_obj = s11n::detail::parse_json(json_str);
        
        Applications apps;
        s11n::load(json_obj, apps, "applications");
        
        InstanceInfoPtrDeque ret;
        for (auto &&app : apps.apps)
        {
            if (!app)
                continue;
            
            for (auto &&ins : app->instances)
            {
                if (!ins)
                    continue;
                ret.emplace_back(ins);
            }
        }
        return ret;
    }

    inline InstanceInfoPtrDeque toAppInstances(const GetResponse &resp)
    {
        // {"application": {"instance": [
        auto &&json_str = std::get<2>(resp);
        auto json_obj = s11n::detail::parse_json(json_str);
        
        Application app;
        s11n::load(json_obj, app, "application");
        
        InstanceInfoPtrDeque ret;
        for (auto &&ins : app.instances)
        {
            if (!ins)
                continue;
            ret.emplace_back(ins);
        }
        return ret;
    }

    inline InstanceInfoPtrDeque toInstances(const GetResponse &resp)
    {
        //{"instance": {
        auto &&json_str = std::get<2>(resp);
        auto json_obj = s11n::detail::parse_json(json_str);
        
        InstanceInfoPtr ins;
        s11n::load(json_obj, ins, "instance");

        InstanceInfoPtrDeque ret;
        if (ins)
            ret.emplace_back(ins);
        return ret;
    }
}

namespace ppeureka { namespace agent {
    using s11n::load;
    using namespace http::impl;

    void EurekaConnect::start()
    {
        m_client.reset(create_client_pool(m_defaultConnCount, m_maxConnCount));
        m_client->start(currentEndPoint(), m_tls);
    }

    void EurekaConnect::stop()
    {
        if (!m_client)
            return;
        m_client->stop();
    }

    void EurekaConnect::switchEndpoint(std::size_t endpointIndex)
    {
        if (m_endpoints.empty())
        {
            m_endpointsIndex = 0;
            return;
        }
        m_endpointsIndex = endpointIndex % m_endpoints.size();

        if (m_client)
            m_client->setEndpoint(currentEndPoint());
    }

    std::string EurekaConnect::currentEndPoint() const
    {
        if (m_endpoints.empty())
            return "";
        return m_endpoints[m_endpointsIndex % m_endpoints.size()];
    }

    InstanceInfoPtr EurekaConnect::getEmptyIns(const std::string &appId, const std::string &insId, int port, const std::string &ipAddr)
    {
        auto p = std::make_shared<InstanceInfo>();
        p->app = appId;
        p->instanceId = insId;
        p->ipAddr = ipAddr;

        p->port = std::make_shared<Port>();
        p->port->port = port;
        p->port->enable = true;

        p->securePort = std::make_shared<Port>();
        p->securePort->port = port;
        p->securePort->enable = false;

        p->hostName = ipAddr;
        p->homePageUrl = "";
        p->statusPageUrl = "";
        p->healthCheckUrl = "";
        p->vipAddress = ipAddr;

        p->secureVipAddress = ipAddr;
        p->status = "UP";

        p->dataCenterInfo = std::make_shared<DataCenterInfo>();
        p->dataCenterInfo->name = "MyOwn";
        p->dataCenterInfo->className = "com.netflix.appinfo.InstanceInfo$DefaultDataCenterInfo";

        p->leaseInfo = std::make_shared<LeaseInfo>();

        p->metadata = std::make_shared<Metadata>();

        p->isCoordinatingDiscoveryServer = false;
        p->lastUpdatedTimestamp = time(nullptr)*1000;
        p->lastDirtyTimestamp = p->lastUpdatedTimestamp;
        p->actionType = "";
        p->overriddenstatus = "UNKNOWN";

        p->countryId = 1;

        return p;
    }

    InstanceInfoPtrDeque EurekaConnect::queryInsAll()
    {
        // {"applications": {"application": ["instance": [
        checkClientValid();

        auto resp = request(METHOD_GET, "/eureka/apps", "");
        return toAppsInstances(resp);
    }

    InstanceInfoPtrDeque EurekaConnect::queryInsByAppId(const std::string &appId)
    {
        // {"application": {"instance": [
        checkClientValid();

        auto resp = request(METHOD_GET, "/eureka/apps/" + helpers::encodeUrl(appId), "");
        return toAppInstances(resp);
    }

    InstanceInfoPtrDeque EurekaConnect::queryInsByAppIdInsId(const std::string &appId, const std::string &insId)
    {
        //{"instance": {
        // or 404
        checkClientValid();

        auto resp = request(METHOD_GET, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId), "");
        return toInstances(resp);
    }

    InstanceInfoPtrDeque EurekaConnect::queryInsByVip(const std::string &vip)
    {
        // {"applications": {"application": ["instance": [
        checkClientValid();

        auto resp = request(METHOD_GET, "/eureka/vips/" + helpers::encodeUrl(vip), "");
        return toAppsInstances(resp);
    }

    InstanceInfoPtrDeque EurekaConnect::queryInsBySVip(const std::string &svip)
    {
        // {"applications": {"application": ["instance": [
        checkClientValid();

        auto resp = request(METHOD_GET, "/eureka/svips/" + helpers::encodeUrl(svip), "");
        return toAppsInstances(resp);
    }

    void EurekaConnect::registerIns(const InstanceInfoPtr &ins)
    {
        if (!ins)
            throw ParamError("instance nullptr");
        // {"instance": {
        s11n::Json::object jobj;
        s11n::Json::object jinso;
        to_json(jinso, *ins);
        jobj["instance"] = std::move(jinso);
        auto s = s11n::Json(std::move(jobj)).dump();

        request(METHOD_POST, "/eureka/apps/" + helpers::encodeUrl(ins->app), "", &s);
    }

    void EurekaConnect::unregisterIns(const std::string &appId, const std::string &insId)
    {
        request(METHOD_DELETE, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId), "");
    }

    void EurekaConnect::sendHeart(const std::string &appId, const std::string &insId)
    {
        request(METHOD_PUT, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId), "");
    }

    void EurekaConnect::statusOutOfService(const std::string &appId, const std::string &insId)
    {
        request(METHOD_PUT, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId) + "/status", "value=OUT_OF_SERVICE");
    }

    void EurekaConnect::statusUp(const std::string &appId, const std::string &insId)
    {
        request(METHOD_DELETE, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId) + "/status", "value=UP");
    }

    void EurekaConnect::updateMetadata(const std::string &appId, const std::string &insId, const std::string &key, const std::string &value)
    {
        request(METHOD_PUT, "/eureka/apps/" + helpers::encodeUrl(appId) + "/" + helpers::encodeUrl(insId) + "/metadata", helpers::encodeUrl(key) + "=" + helpers::encodeUrl(value));
    }

    void EurekaConnect::checkClientValid()
    {
        if (!m_client)
            throw Error("need start suc.");
    }

    bool EurekaConnect::checkHttpCodeSuc(const GetResponse &resp, bool &needRetry)
    {
        needRetry = false;
        auto &&status = std::get<0>(resp);
        if (status.success())
            return true;
        if (4 == status.code()/100)
            return false;
        needRetry = true;
        return false;
    }

    bool EurekaConnect::doRetry(std::size_t tryCount, const GetResponse *resp)
    {
        if (m_retryFunc)
            return m_retryFunc(tryCount, resp);
        return defaultRetry(tryCount, resp);
    }

    bool EurekaConnect::defaultRetry(std::size_t tryCount, const GetResponse *resp)
    {
        if (tryCount > 2*m_endpoints.size())
            return false;
        if (resp)
        {
            // httpcode valid
            auto &&status = std::get<0>(*resp);
            auto hc = status.code();
            if (http::HC_InternalServerError == hc)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
            }
            else if (http::HC_TemporaryRedirect == hc)
            {
                // Location
                auto &&headers = std::get<1>(*resp);
                auto it = headers.find("Location");
                if (it == headers.end())
                    it = headers.find("location");
                if (it != headers.end() && !it->second.empty())
                {
                    m_client->setEndpoint(it->second);
                }
                else
                {
                    // fail, no retry
                    return false;
                }
            }
            return true;
        }
        // net error, swith next
        std::size_t n = m_endpointsIndex;
        switchEndpoint(++n);
        return true;
    }

    GetResponse EurekaConnect::request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data)
    {
        std::size_t tryCount = 0;
        while (true)
        {
            ++tryCount;
            try
            {
                auto resp = m_client->request(method, path, query, data);
                bool needRetry = false;
                if (checkHttpCodeSuc(resp, needRetry))
                {
                    // suc
                    return resp;
                }
                if (needRetry && doRetry(tryCount, &resp))
                {
                    continue;
                }
                // status error
                auto &&status = std::get<0>(resp);
                if (status.code() == 404)
                {
                    throw NotFoundError{};
                }
                std::string msg = status.message() + "(" + std::to_string(status.code()) + ")";
                throw BadStatus(status, std::move(msg));
            }
            catch (const NetError &e)
            {
                // NetError try next ?
                if (!doRetry(tryCount, nullptr))
                {
                    // break
                    throw e;
                }
            }
        }
    }
}}



namespace ppeureka { namespace http { namespace impl {
    Client *create_client()
    {
        return new ppeureka::curl::HttpClient();
    }

    Client *create_client_pool(std::size_t defaultConnCount, std::size_t maxConnCount)
    {
        return new ppeureka::curl::HttpClientPool(defaultConnCount, maxConnCount);
    }
}}}