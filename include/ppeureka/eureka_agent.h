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
#include "ppeureka/eureka_connect.h"
#include "ppeureka/sync_list.h"
#include <random>


namespace ppeureka { namespace agent {

    using HttpClientPtr = std::shared_ptr<ppeureka::http::impl::Client>;
    using HttpMethod = ppeureka::http::impl::HttpMethod;
    using GetResponse = http::impl::Client::GetResponse;

    // 
    struct AgentSnap;

    class EurekaAgent
    {
        using job_thread = sync_list::job_thread;
        using lock_type = std::mutex;
        using auto_lock_type = std::unique_lock<lock_type>;

    public:
        
        using Timestamp = std::chrono::steady_clock::time_point;
        using Duration = std::chrono::steady_clock::duration;

        struct RegInsData
        {
            InstanceInfoPtr         ins;
            Timestamp               lastHeartTime;
            int64_t                 heartSucCount{0};
            int64_t                 heartErrCount{0};
        };

        struct CheckInsErrState
        {
            std::size_t             errStep{0}; // 0=no err, 1=first check has error, 2=second check has error....
            Timestamp               errTime;
            int                     inChoosingCount{0};
            std::size_t             goodCount{0}; // good count for cur check
            std::size_t             errorCount{0}; // error count for cur check
            std::size_t             errorCountPrev{0}; // error count for prev check

            bool isErr() const { return 0==errStep; };
            bool isInColdDown(const Duration &dur) const;
            void occurErr();
            void sucRequest();
            // if is err:
            //   cd gone and (first or good sequence), return true.
            //   return false.
            // if good:
            //   return true.
            bool tryChoose();
            void nextCheck();
            void reset();
        };

        struct CheckInsStatistics
        {
            struct SumAvg
            {
                int64_t     all{0};
                int64_t     count{0};

                int64_t avg() const { return count<=0 ? 0 : all/count; }
                void reset() { all = count = 0; }
                void add(int64_t v) {
                    all += v;
                    ++count;
                }
            };
            std::size_t             requestCountAll{0};
            std::list<SumAvg>       respSucTimeMicroSec{0};     // {0, 10} count, front is earliest. one rec every 3 seconds.
            std::list<SumAvg>       respErrTimeMicroSec{0};     // {0, 10} count, front is earliest.

            void nextCheck();
            void add(bool suc, int64_t respMicroSec);
        };

        struct CheckInsData
        {
            bool                    isDeleted{false};  // true if refresh app cannot find this instance
            InstanceInfoPtr         ins;
            HttpClientPtr           cli;
            CheckInsStatistics      statis;
            CheckInsErrState        errState;
        };
        using CheckInsDataPtr = std::shared_ptr<CheckInsData>;
        using CheckInsDataPtrMap = std::map<std::string, CheckInsDataPtr>; // insId->CheckInsData

        struct InsHttpClient;
        using InsHttpClientPtr = std::shared_ptr<InsHttpClient>;

        struct CheckAppData;
        // app has locked.
        // appLock is transfer to InsHttpClient
        using ChooseHttpClientFunction = std::function<InsHttpClientPtr(CheckAppData &app, lock_type *appLock)>;
        struct CheckAppData
        {
            CheckInsDataPtrMap      inses;
            std::deque<std::string> insIds; // the random sequence of insId.
            std::size_t             nextChooseInsIdIndex{0};    // 
            Timestamp               lastRefreshTime;

            ChooseHttpClientFunction chooseFunc;
        };

        struct InsHttpClient
        {
            InstanceInfoPtr         ins;

            InsHttpClient(const CheckInsDataPtr &checkIns_, EurekaAgent *eAgent_, lock_type *appLock_);
            virtual ~InsHttpClient();

            // throw Error when http client Error exception
            virtual GetResponse request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr);
            // throw Error when http client exception or HttpCode not 2xx
            virtual std::string requestRespData(HttpMethod method, const std::string& path, const std::string& query, const std::string *data = nullptr);

            InsHttpClient(const InsHttpClient &) = delete;
            InsHttpClient& operator=(const InsHttpClient &) = delete;
        protected:
            friend class EurekaAgent;
            EurekaAgent                  *eAgent{nullptr};
            CheckInsDataPtr              checkIns;
            lock_type                    *appLock;
        };
        
    public:
        explicit EurekaAgent(EurekaConnect &conn);

        // All net request may be Exception.
        // see EurekaConnect note.

        void start();
        void stop();

        // return "app:ipAddr:port"
        std::string makeInsId(const std::string &app, const std::string &ipAddr, int port);
        // register instantce and send heart period until stop
        // if exception, heart will not period
        void registerIns(const InstanceInfoPtr &ins);
        void registerIns(const std::string &app, const std::string &ipAddr, int port);

        // unregister instance and stop the instance's heart period.
        // if exception, heart will continue period
        void unregisterIns(const std::string &appId, const std::string &insId);

        // get the http client of the instance.
        //   when instance info refresh, the http client endpoint may be updated.
        //   so, the server which connected may be change.
        // Returns:
        //   if none, throw Error, so return ptr is always valid.
        InsHttpClientPtr getHttpClient(const std::string &appId, const std::string &insId);
        // get one instance http client in app. default choose by defaultChooseHttpClient.
        //   when instance info refresh, the http client endpoint may be updated.
        // Returns:
        //   if none, throw Error, so return ptr is always valid.
        InsHttpClientPtr getHttpClient(const std::string &appId);

        std::string callHttpConfigServer(const std::string &path);
        std::string callHttpConfigServer(const std::string &serName, const std::string &tag);


        void setChooseHttpClient(const std::string &appId, ChooseHttpClientFunction f);
        // get the http client of random instance in app instances.
        //   if none match, throw Error, so return ptr must always valid.
        // Error instance:
        //   if some instance has occur net error or http code 5xx, it will enter into err state, and apply {1,5,10,30} seconds choose cold down,
        //      when CD in, can not be choose.
        //      when CD gone, can be choose sequent if first or good.
        //      when at the next check time, 
        //          if become good, the cold down period decrease.
        //          if no choose and elapsed {10,30,60,120} seconds, the cold down period decrease.
        //          if still error, the cold down period increase.
        //   if err state cannot be choose, next one will be choose, so maybe avalanche.
        //   if the instance endpoint updated, the error record will be reset.
        InsHttpClientPtr defaultChooseHttpClient(CheckAppData &app, lock_type *appLock);


        // the snapshot of agent
        void getSnap(AgentSnap &snap);
        

    private:
        struct InnerRegInsData
        {
            lock_type           lock;
            RegInsData          regIns;
            std::atomic<bool>   doing{false};
        };
        using InnerRegInsDataPtr = std::shared_ptr<InnerRegInsData>;
        using InnerRegInsDataPtrMap = std::map<std::string, InnerRegInsDataPtr>;    // insId -> data
        
        struct InnerCheckAppData
        {
            InnerCheckAppData() { rndEng.seed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())); }

            lock_type           lock;
            CheckAppData        app;
            std::atomic<bool>   doing{false};
            std::default_random_engine  rndEng;
        };
        using InnerCheckAppDataPtr = std::shared_ptr<InnerCheckAppData>;
        using InnerCheckAppDataPtrMap = std::map<std::string, InnerCheckAppDataPtr>;    // appId -> data

    private:
        void onInsHttpClientConstruct(const InsHttpClient &httpCli);
        void onInsHttpClientDestroy(const InsHttpClient &httpCli);
        void onInsHttpClientRequestDone(const InsHttpClient &httpCli, bool suc, int64_t respMicroSec);

        void doTimer();
        void doTimerRegHeart();
        void doTimerCheckApp();
        void doRegHeart(InnerRegInsData &innerReg);

        InsHttpClientPtr chooseHttpClient(CheckAppData &app, lock_type *appLock);

        // req apps by conn, add into or refresh m_apps ins, return query app.
        // may be except
        InnerCheckAppDataPtr refreshCheckApp(const std::string &appId);

    private:
        EurekaConnect &m_conn;
        std::atomic<bool>   m_stop_flag{false};
        job_thread          m_timer_thread;
        job_thread          m_do_thread;

        lock_type               m_lockReg;
        InnerRegInsDataPtrMap   m_regs;
        
        lock_type               m_lockApp;
        InnerCheckAppDataPtrMap m_apps;
    };

    struct AgentSnap
    {
        // regInsData
        struct RegInsSnapData
        {
            EurekaAgent::Timestamp  lastHeartTime;
            int64_t                 heartSucCount{0};
            int64_t                 heartErrCount{0};
        };
        // insId -> RegSnapData
        std::map<std::string, RegInsSnapData>   regs;

        // reqInsData
        struct ReqInsSnapData
        {
            std::string                          endpoint;
            EurekaAgent::CheckInsStatistics      statis;
            EurekaAgent::CheckInsErrState        errState;
        };
        // insId -> ReqInsSnapData
        using ReqAppSnapData = std::map<std::string, ReqInsSnapData>;
        // appId -> ReqAppSnapData
        std::map<std::string, ReqAppSnapData>   apps;
    };
}}