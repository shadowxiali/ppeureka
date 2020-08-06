//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include "ppeureka/types.h"
#include "s11n.h"


namespace ppeureka {

    inline void load(const s11n::Json& src, Port& dst)
    {
        using s11n::load;

        load(src, dst.port, "$");
        load(src, dst.enable, "@enabled");
    }

    inline void load(const s11n::Json& src, LeaseInfo& dst)
    {
        using s11n::load;

        load(src, dst.renewalIntervalInSecs, "renewalIntervalInSecs");
        load(src, dst.durationInSecs, "durationInSecs");
        load(src, dst.registrationTimestamp, "registrationTimestamp");
        load(src, dst.lastRenewalTimestamp, "lastRenewalTimestamp");
        load(src, dst.evictionTimestamp, "evictionTimestamp");
        load(src, dst.serviceUpTimestamp, "serviceUpTimestamp");
    }

    inline void load(const s11n::Json& src, DataCenterInfo& dst)
    {
        using s11n::load;

        load(src, dst.name, "name");
        load(src, dst.className, "@class");
    }

    inline void load(const s11n::Json& src, InstanceInfo& dst)
    {
        using s11n::load;

        load(src, dst.app, "app");
        load(src, dst.instanceId, "instanceId");
        load(src, dst.ipAddr, "ipAddr");
        load(src, dst.port, "port");
        load(src, dst.securePort, "securePort");

        load(src, dst.hostName, "hostName");
        load(src, dst.homePageUrl, "homePageUrl");
        load(src, dst.statusPageUrl, "statusPageUrl");
        load(src, dst.healthCheckUrl, "healthCheckUrl");
        load(src, dst.vipAddress, "vipAddress");

        load(src, dst.secureVipAddress, "secureVipAddress");
        load(src, dst.status, "status");
        load(src, dst.dataCenterInfo, "dataCenterInfo");
        load(src, dst.leaseInfo, "leaseInfo");
        load(src, dst.metadata, "metadata");

        load(src, dst.isCoordinatingDiscoveryServer, "isCoordinatingDiscoveryServer");
        load(src, dst.lastUpdatedTimestamp, "lastUpdatedTimestamp");
        load(src, dst.lastDirtyTimestamp, "lastDirtyTimestamp");
        load(src, dst.actionType, "actionType");
        load(src, dst.overriddenstatus, "overriddenstatus");

        load(src, dst.countryId, "countryId");

        dst.statusCheck = CheckStatus::OUT_OF_SERVICE;
        if (0 == dst.status.compare("UP") || 0 == dst.status.compare("up"))
            dst.statusCheck = CheckStatus::UP;
    }

    inline void load(const s11n::Json& src, Application& dst)
    {
        using s11n::load;

        load(src, dst.name, "name");
        load(src, dst.instances, "instance");
    }

    inline void load(const s11n::Json& src, Applications& dst)
    {
        using s11n::load;

        load(src, dst.versionsDelta, "versions__delta");
        load(src, dst.appsHashCode, "apps__hashcode");
        load(src, dst.apps, "application");
    }

    // ================= Value TO Json ==============================

    
    inline void to_json(s11n::Json::object &dst, const Port &src)
    {
        using s11n::to_json;

        to_json(dst, src.port, "$");
        to_json(dst, src.enable, "@enabled");
    }

    inline void to_json(s11n::Json::object &dst, const LeaseInfo &src)
    {
        using s11n::to_json;

        to_json(dst, static_cast<int>(src.renewalIntervalInSecs), "renewalIntervalInSecs");
        to_json(dst, static_cast<int>(src.durationInSecs), "durationInSecs");
        
        to_json(dst, src.registrationTimestamp, "registrationTimestamp");
        to_json(dst, src.lastRenewalTimestamp, "lastRenewalTimestamp");
        to_json(dst, src.evictionTimestamp, "evictionTimestamp");
        to_json(dst, src.serviceUpTimestamp, "serviceUpTimestamp");
    }

    inline void to_json(s11n::Json::object &dst, const DataCenterInfo &src)
    {
        using s11n::to_json;

        to_json(dst, src.name, "name");
        to_json(dst, src.className, "@class");
    }

    inline void to_json(s11n::Json::object &dst, const InstanceInfo &src)
    {
        using s11n::to_json;

        to_json(dst, src.app, "app");
        to_json(dst, src.instanceId, "instanceId");
        to_json(dst, src.ipAddr, "ipAddr");
        to_json(dst, src.port, "port");
        to_json(dst, src.securePort, "securePort");

        to_json(dst, src.hostName, "hostName");
        to_json(dst, src.homePageUrl, "homePageUrl");
        to_json(dst, src.statusPageUrl, "statusPageUrl");
        to_json(dst, src.healthCheckUrl, "healthCheckUrl");
        to_json(dst, src.vipAddress, "vipAddress");

        to_json(dst, src.secureVipAddress, "secureVipAddress");
        to_json(dst, src.status, "status");
        to_json(dst, src.dataCenterInfo, "dataCenterInfo");
        to_json(dst, src.leaseInfo, "leaseInfo");
        to_json(dst, src.metadata, "metadata");

        to_json(dst, src.isCoordinatingDiscoveryServer, "isCoordinatingDiscoveryServer");
        to_json(dst, src.lastUpdatedTimestamp, "lastUpdatedTimestamp");
        to_json(dst, src.lastDirtyTimestamp, "lastDirtyTimestamp");
        // server do not accept actionType=="" 
        if (src.actionType.empty())
            dst["actionType"] = s11n::Json{nullptr};
        else
            to_json(dst, src.actionType, "actionType");
        to_json(dst, src.overriddenstatus, "overriddenstatus");

        to_json(dst, src.countryId, "countryId");
    }
}
