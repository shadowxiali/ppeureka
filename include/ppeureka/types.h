//  Copyright (c) 2014-2017 Andrey Upadyshev <oliora@gmail.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <chrono>
#include <stdint.h>
#include <iostream>
#include <memory>
#include <deque>
#include <functional>
#include <atomic>


namespace ppeureka {

    using Metadata = std::map<std::string, std::string>;
    using MetadataPtr = std::shared_ptr<Metadata>;

    using StringList = std::vector<std::string>;

    enum class CheckStatus
    {
        UP, // UP
        OUT_OF_SERVICE, // OUT_OF_SERVICE
    };

    struct Port
    {
        int port{0};
        bool enable{false};
    };
    using PortPtr = std::shared_ptr<Port>;

    struct LeaseInfo
    {
        int64_t renewalIntervalInSecs{30};
        int64_t durationInSecs{90};
        int64_t registrationTimestamp{0};  // time_t * 1000 + milliseconds
        int64_t lastRenewalTimestamp{0};  // time_t * 1000 + milliseconds
        int64_t evictionTimestamp{0};  // time_t * 1000 + milliseconds
        int64_t serviceUpTimestamp{0};  // time_t * 1000 + milliseconds
    };
    using LeaseInfoPtr = std::shared_ptr<LeaseInfo>;

    struct DataCenterInfo
    {
        std::string name;
        std::string className;
    };
    using DataCenterInfoPtr = std::shared_ptr<DataCenterInfo>;

    struct InstanceInfo
    {
        std::string app;
        std::string instanceId;
        std::string ipAddr;

        CheckStatus statusCheck; // enum of status
        PortPtr     port;
        PortPtr     securePort;

        std::string hostName;
        std::string homePageUrl;
        std::string statusPageUrl;
        std::string healthCheckUrl;
        
        std::string vipAddress;
        std::string secureVipAddress;
        std::string status;
        
        DataCenterInfoPtr dataCenterInfo;
        LeaseInfoPtr      leaseInfo;
        MetadataPtr       metadata;

        bool        isCoordinatingDiscoveryServer{false};
        int64_t     lastUpdatedTimestamp{0};
        int64_t     lastDirtyTimestamp{0};
        std::string actionType;
        std::string overriddenstatus;
        int64_t     countryId{0};
    };
    using InstanceInfoPtr = std::shared_ptr<InstanceInfo>;
    using InstanceInfoPtrDeque = std::deque<InstanceInfoPtr>;

    struct Application
    {
        std::string            name;
        InstanceInfoPtrDeque   instances;
    };
    using ApplicationPtr = std::shared_ptr<Application>;
    using ApplicationPtrDeque = std::deque<ApplicationPtr>;

    struct Applications
    {
        std::string  versionsDelta;
        std::string  appsHashCode;

        ApplicationPtrDeque  apps;
    };
    using ApplicationsPtr = std::shared_ptr<Applications>;

    inline std::ostream& operator<< (std::ostream& os, const CheckStatus& s)
    {
        switch (s)
        {
        case CheckStatus::UP:
            os << "UP";
            break;
        case CheckStatus::OUT_OF_SERVICE:
            os << "OUT_OF_SERVICE";
            break;
        default:
            os << "?";
            break;
        }

        return os;
    }

}
