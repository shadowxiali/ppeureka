//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include "ppeureka/eureka_agent.h"
#include <time.h>
#include <thread>
#include <random>
#include <algorithm>
#include "ppeureka/helpers.h"

namespace {
    using namespace ppeureka;
    using namespace ppeureka::agent;

    enum {
        DO_THREAD_COUNT = 4,
    };

    enum {
        REG_HEART_PERIOD_SECONDS = 3,
        CHECK_APP_PERIOD_SECONDS = 3,
    };

    enum {
        ERR_STEP_COUNT = 4,
    };

    

    inline int64_t PeriodSeconds(const EurekaAgent::Timestamp &tpNow, const EurekaAgent::Timestamp &tpPrev)
    {
        return std::chrono::duration_cast<std::chrono::seconds>(tpNow - tpPrev).count();
    }

    // first http, second https + ipAddr + port
    // return http://xxx:xx
    inline std::string getEndpoint(const InstanceInfoPtr &ins)
    {
        std::string ep;
        ep.reserve(ins->ipAddr.length() + 20);
        ep = ins->ipAddr;
        if (ins->port && ins->port->enable)
        {
            ep += ":";
            ep += std::to_string(ins->port->port);
        }
        else if (ins->securePort && ins->securePort->enable)
        {
            ep += ":";
            ep += std::to_string(ins->securePort->port);
            return ppeureka::helpers::ensureScheme(ep, "https");
        }
        else
        {
            // force default http port
        }
        
        return ppeureka::helpers::ensureScheme(ep);
    }

    inline int64_t GetColdDown(std::size_t errStep)
    {
        const int64_t sErrSteps[ERR_STEP_COUNT] = {1,5,10,30};
        if (0 == errStep)
            return 0;
        else if (errStep > ERR_STEP_COUNT)
            return sErrSteps[ERR_STEP_COUNT-1];
        return sErrSteps[errStep - 1];
    }

    inline int64_t GetTimeoutToErrStepDecrease(std::size_t errStep)
    {
        const int64_t sTimeout[ERR_STEP_COUNT] = {10,30,60,120};
        if (0 == errStep)
            return 0;
        else if (errStep > ERR_STEP_COUNT)
            return sTimeout[ERR_STEP_COUNT-1];
        return sTimeout[errStep - 1];
    }
}

namespace ppeureka { namespace agent {

    using DeferRun = ppeureka::helpers::DeferRun;


    EurekaAgent::EurekaAgent(EurekaConnect &conn)
     :m_conn(conn)
     {}

     void EurekaAgent::start()
     {
         m_stop_flag = false;
         m_do_thread.start(DO_THREAD_COUNT);
         m_timer_thread.start(1);
         m_timer_thread.emplace_back([this](){
             doTimer();
         });
     }

     void EurekaAgent::stop()
     {
         m_stop_flag = true;
         m_timer_thread.stop(true);
         m_do_thread.stop(true);
     }

    // return "app:ipAddr:port"
    std::string EurekaAgent::makeInsId(const std::string &app, const std::string &ipAddr, int port)
    {
        std::string s;
        s.reserve(app.length() + ipAddr.length() + 10);
        s += app;
        s += ":";
        s += ipAddr;
        s += ":";
        s += std::to_string(port);
        return s;
    }
    // register instantce and send heart period until stop
    void EurekaAgent::registerIns(const InstanceInfoPtr &ins)
    {
        if (m_stop_flag)
            throw Error("stoped");
        if (!ins)
            throw ParamError("invalid ins ptr");

        m_conn.registerIns(ins);

        auto_lock_type al{m_lockReg};
        auto it = m_regs.find(ins->instanceId);
        if (it == m_regs.end())
        {
            it = m_regs.emplace(ins->instanceId, std::make_shared<InnerRegInsData>()).first;
        }
        // first heart
        auto &innerReg = it->second;
        innerReg->regIns.ins = ins;
        innerReg->doing = true;
        m_do_thread.emplace_back([this, innerReg](){
            doRegHeart(*innerReg);
        });
    }
    void EurekaAgent::registerIns(const std::string &app, const std::string &ipAddr, int port)
    {
        if (m_stop_flag)
            throw Error("stoped");

        auto insId = makeInsId(app, ipAddr, port);
        auto ins = m_conn.getEmptyIns(app, insId, port, ipAddr);
        registerIns(ins);
    }

    // unregister instance and stop the instance's heart period.
    void EurekaAgent::unregisterIns(const std::string &appId, const std::string &insId)
    {
        if (m_stop_flag)
            throw Error("stoped");
        
        m_conn.unregisterIns(appId, insId);

        auto_lock_type al{m_lockReg};
        m_regs.erase(insId);
    }

    // get the http client of the instance.
    //   when instance info refresh, the http client endpoint may be updated.
    //   so, the server which connected may be change.
    // Returns:
    //   if none, throw Error, so return ptr is always valid.
    EurekaAgent::InsHttpClientPtr EurekaAgent::getHttpClient(const std::string &appId, const std::string &insId)
    {
        InnerCheckAppDataPtr innerApp;
        {
            auto_lock_type al{m_lockApp};
            auto it = m_apps.find(appId);
            if (it != m_apps.end())
            {
                innerApp = it->second;
            }
        }
        while (true)
        {
            bool hasRefreshed{false};
            if (!innerApp)
            {
                hasRefreshed = true;
                innerApp  = refreshCheckApp(appId);
            }
            if (innerApp)
            {
                auto_lock_type al{innerApp->lock};
                auto it = innerApp->app.inses.find(insId);
                if (it != innerApp->app.inses.end())
                {
                    auto &chkIns = it->second;
                    return std::make_shared<InsHttpClient>(chkIns, this, &innerApp->lock);
                }
                if (!hasRefreshed)
                {
                    auto tpNow = std::chrono::steady_clock::now();
                    if (PeriodSeconds(tpNow, innerApp->app.lastRefreshTime) > CHECK_APP_PERIOD_SECONDS)
                    {
                        // to refresh, and try again
                        innerApp = nullptr;
                        continue;
                    }
                }
            }
            
            break;
        }
        
        throw Error{"not exist instance"};
    }
    // get one instance http client in app. default choose by defaultChooseHttpClient.
    //   when instance info refresh, the http client endpoint may be updated.
    // Returns:
    //   if none, throw Error, so return ptr is always valid.
    EurekaAgent::InsHttpClientPtr EurekaAgent::getHttpClient(const std::string &appId)
    {
        InnerCheckAppDataPtr innerApp;
        {
            auto_lock_type al{m_lockApp};
            auto it = m_apps.find(appId);
            if (it != m_apps.end())
            {
                innerApp = it->second;
            }
        }
        
        {
            if (!innerApp)
            {
                innerApp  = refreshCheckApp(appId);
            }
            if (innerApp)
            {
                return chooseHttpClient(innerApp->app, &innerApp->lock);
            }
        }
        
        throw Error{"not exist instance"};
    }

    std::string EurekaAgent::callHttpConfigServer(const std::string &path)
    {
        auto insCli = getHttpClient("CONFIG-SERVER");
        return insCli->requestRespData(HttpMethod::METHOD_GET, path, "");
    }
    std::string EurekaAgent::EurekaAgent::callHttpConfigServer(const std::string &serName, const std::string &tag)
    {
        std::string path;
        path.reserve(serName.length() + tag.length() + 10);
        path += "/";
        path += serName;
        if (!tag.empty())
        {
            path += "-";
            path += tag;
        }
        path += ".yml";
        return callHttpConfigServer(path);
    }


    void EurekaAgent::setChooseHttpClient(const std::string &appId, ChooseHttpClientFunction f)
    {
        InnerCheckAppDataPtr innerApp;

        {
            auto_lock_type al{m_lockApp};
            auto it = m_apps.find(appId);
            if (it == m_apps.end())
            {
                it = m_apps.emplace(appId, std::make_shared<InnerCheckAppData>()).first;
            }
            innerApp = it->second;
        }

        if (innerApp)
        {
            auto_lock_type al{innerApp->lock};
            innerApp->app.chooseFunc = std::move(f);
        }
    }

    EurekaAgent::InsHttpClientPtr EurekaAgent::defaultChooseHttpClient(EurekaAgent::CheckAppData &app, lock_type *appLock)
    {
        if (app.insIds.empty())
            throw Error{"empty instances"};
        
        auto insCount = app.insIds.size();
        std::size_t firstIndex = app.nextChooseInsIdIndex % insCount;
        for (std::size_t i=0; i < insCount; ++i)
        {
            auto insIndex = (firstIndex + i) % insCount;
            const auto &insId = app.insIds[insIndex];
            auto itIns = app.inses.find(insId);
            if (itIns == app.inses.end())
            {
                assert(0);
                throw Error{"logic error by ins id not match."};
            }
            auto &chkIns = itIns->second;
            if (!chkIns->errState.tryChoose())
            {
                continue;
            }
            
            // choose this
            return std::make_shared<InsHttpClient>(chkIns, this, appLock);
        }

        throw Error{"none instance match"};
    }


    // the snapshot of agent
    void EurekaAgent::getSnap(AgentSnap &snap)
    {
        // register data
        {
            auto_lock_type al{m_lockReg};
            for (auto &&stReg : m_regs)
            {
                auto &srcItem = stReg.second;
                auto &snapItem = snap.regs[stReg.first];
                snapItem.lastHeartTime = srcItem->regIns.lastHeartTime;
                snapItem.heartSucCount = srcItem->regIns.heartSucCount;
                snapItem.heartErrCount = srcItem->regIns.heartErrCount;
            }
        }

        // reqInsData
        {
            auto_lock_type al{m_lockApp};
            for (auto &&stApp : m_apps)
            {
                auto &snapApp = snap.apps[stApp.first];
                for (auto &stIns : stApp.second->app.inses)
                {
                    auto &snapIns = snapApp[stIns.first];
                    auto &srcIns = *stIns.second;
                    snapIns.endpoint = getEndpoint(srcIns.ins);
                    snapIns.statis = srcIns.statis;
                    snapIns.errState = srcIns.errState;
                }
            }
        }
    }

    void EurekaAgent::onInsHttpClientConstruct(const EurekaAgent::InsHttpClient &httpCli)
    {
        // now appLock is locked, so not need lock
        ++httpCli.checkIns->errState.inChoosingCount;
    }
    void EurekaAgent::onInsHttpClientDestroy(const EurekaAgent::InsHttpClient &httpCli)
    {
        auto_lock_type al{*httpCli.appLock};
        --httpCli.checkIns->errState.inChoosingCount;
    }
    void EurekaAgent::onInsHttpClientRequestDone(const EurekaAgent::InsHttpClient &httpCli, bool suc, int64_t respMicroSec)
    {
        auto_lock_type al{*httpCli.appLock};
        auto chkIns = httpCli.checkIns;
        chkIns->statis.add(suc, respMicroSec);

        if (!suc)
            chkIns->errState.occurErr();
        else
            chkIns->errState.sucRequest();
    }

    void EurekaAgent::doTimer()
    {
        auto tpPrevHeart = std::chrono::steady_clock::now();
        auto tpPrevCheckApp = tpPrevHeart;
        while (!m_stop_flag)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            if (m_stop_flag)
                break;

            auto tpNow = std::chrono::steady_clock::now();
            if (PeriodSeconds(tpNow, tpPrevHeart) >= REG_HEART_PERIOD_SECONDS)
            {
                tpPrevHeart = tpNow;
                doTimerRegHeart();
            }

            if (PeriodSeconds(tpNow, tpPrevCheckApp) >= CHECK_APP_PERIOD_SECONDS)
            {
                tpPrevCheckApp = tpNow;
                doTimerCheckApp();
            }
        }
    }

    void EurekaAgent::doTimerRegHeart()
    {
        auto_lock_type al{m_lockReg};
        for (auto &&stReg : m_regs) 
        {
            auto &innerReg = stReg.second;
            if (innerReg->doing)
                continue;

            auto_lock_type al2{innerReg->lock};
            auto tpNow = std::chrono::steady_clock::now();
            int64_t period = 10;
            if (innerReg->regIns.ins->leaseInfo)
                period = innerReg->regIns.ins->leaseInfo->renewalIntervalInSecs / 3 + 1;
            if (PeriodSeconds(tpNow, innerReg->regIns.lastHeartTime) >= period)
            {
                innerReg->doing = true;
                m_do_thread.emplace_back([this, innerReg](){
                    doRegHeart(*innerReg);
                });
            }
        }
    }

    void EurekaAgent::doTimerCheckApp()
    {
        // refresh apps
        std::list<std::string> needCheckApps;
        {
            auto_lock_type al{m_lockApp};
            for (auto &&stApp : m_apps)
            {
                if (stApp.second->doing)
                {
                    continue;
                }
                needCheckApps.emplace_back(stApp.first);
            }
        }

        for (auto &&appId : needCheckApps)
        {
            try 
            {
                refreshCheckApp(appId);
            }
            catch(Error &)
            {
                // TODO trace it
            }
        }

        // all instance err check
        std::list<InnerCheckAppDataPtr> needCheckInnerApps;
        {
            auto_lock_type al{m_lockApp};
            for (auto &&stApp : m_apps)
            {
                needCheckInnerApps.emplace_back(stApp.second);
            }
        }
        for (auto &innerApp : needCheckInnerApps)
        {
            auto_lock_type al2{innerApp->lock};
            for (auto &&stIns : innerApp->app.inses)
            {
                auto &innerIns = stIns.second;
                innerIns->statis.nextCheck();
                innerIns->errState.nextCheck();
            }
        }
    }

    void EurekaAgent::doRegHeart(EurekaAgent::InnerRegInsData &innerReg)
    {
        DeferRun dr1([&](){
            innerReg.doing = false;
        });

        try 
        {
            auto &ins = innerReg.regIns;
            ins.lastHeartTime = std::chrono::steady_clock::now();
            m_conn.sendHeart(ins.ins->app, ins.ins->instanceId);
            ins.heartSucCount += 1;
        }
        catch(Error &)
        {
            // TODO trace it
            innerReg.regIns.heartErrCount += 1;
        }
    }

    EurekaAgent::InsHttpClientPtr EurekaAgent::chooseHttpClient(EurekaAgent::CheckAppData &app, lock_type *appLock)
    {
        if (app.chooseFunc)
            return app.chooseFunc(app, appLock);
        return defaultChooseHttpClient(app, appLock);
    }

    EurekaAgent::InnerCheckAppDataPtr EurekaAgent::refreshCheckApp(const std::string &appId)
    {
        CheckInsDataPtrMap eraseInses;
        InnerCheckAppDataPtr innerApp;

        {
            auto_lock_type al{m_lockApp};
            auto it = m_apps.find(appId);
            if (it == m_apps.end())
            {
                it = m_apps.emplace(appId, std::make_shared<InnerCheckAppData>()).first;
            }
            innerApp = it->second;
            innerApp->doing = true;
        }
        auto doingPtr = &innerApp->doing;
        DeferRun dr([&](){
            *doingPtr = false;
        });

        InstanceInfoPtrDeque insesInQuery;
        try
        {
            insesInQuery = m_conn.queryInsByAppId(appId);
        }
        catch (NotFoundError &e)
        {
            // not found same as empty instances
        }

        {
            auto_lock_type al{innerApp->lock};
            innerApp->app.lastRefreshTime = std::chrono::steady_clock::now();
            eraseInses = innerApp->app.inses; // default full erase

            std::string prevNextInsId;
            if (!innerApp->app.insIds.empty())
            {
                auto i = innerApp->app.nextChooseInsIdIndex % innerApp->app.insIds.size();
                prevNextInsId = innerApp->app.insIds[i];
            }

            bool hasAdd{false};
            for (auto &&insQ : insesInQuery)
            {
                auto itIns = eraseInses.find(insQ->instanceId);
                if (itIns != eraseInses.end())
                {
                    // exists in check
                    auto &chkIns = itIns->second;
                    auto epExists = getEndpoint(chkIns->ins);
                    auto epQ = getEndpoint(insQ);
                    if (epQ != epExists)
                    {
                        // endpoint update
                        chkIns->cli->setEndpoint(epQ);
                        // clear err state when endpoint update
                        chkIns->errState.reset();
                    }
                    chkIns->ins = insQ; // update instance info

                    eraseInses.erase(itIns);
                }
                else
                {
                    // not exists in check, add
                    hasAdd = true;
                    auto chkIns = std::make_shared<CheckInsData>();
                    chkIns->ins = insQ;
                    chkIns->cli.reset(ppeureka::http::impl::create_client_pool());
                    ppeureka::http::impl::TlsConfig defaultTls;
                    chkIns->cli->start(getEndpoint(insQ), defaultTls);

                    innerApp->app.inses.emplace(insQ->instanceId, chkIns);
                }
            }//end for insesInQuery

            if (hasAdd || !eraseInses.empty())
            {
                // instance count change, rebuild insIds
                auto &app = innerApp->app;
                app.insIds.clear();
                for (auto &&st : app.inses)
                {
                    app.insIds.emplace_back(st.first);
                }
                std::shuffle(app.insIds.begin(), app.insIds.end(), innerApp->rndEng);

                if (!prevNextInsId.empty())
                {
                    // try set next to prev next
                    auto it_find = std::find(app.insIds.begin(), app.insIds.end(), prevNextInsId);
                    if (it_find != app.insIds.end())
                    {
                        app.nextChooseInsIdIndex = it_find - app.insIds.begin();
                    }
                }
            }

            // erase the not exists in current query
            for (auto &&st : eraseInses)
            {
                st.second->isDeleted = true;
                innerApp->app.inses.erase(st.first);
            }
        }//end lock
        
        // stop the erased instances
        for (auto &&st : eraseInses)
        {
            auto &chkIns = st.second;
            chkIns->cli->stop();
        }

        return innerApp;
    }


    void EurekaAgent::CheckInsStatistics::nextCheck()
    {
        if (respSucTimeMicroSec.size() >= 10)
            respSucTimeMicroSec.pop_front();
        respSucTimeMicroSec.emplace_back();

        if (respErrTimeMicroSec.size() >= 10)
            respErrTimeMicroSec.pop_front();
        respErrTimeMicroSec.emplace_back();
    }
    void EurekaAgent::CheckInsStatistics::add(bool suc, int64_t respMicroSec)
    {
        auto *avgs = suc ? &respSucTimeMicroSec : &respErrTimeMicroSec;
        if (avgs->empty())
            avgs->emplace_back();
        auto &lastOne = avgs->back();
        lastOne.add(respMicroSec);
    }

    bool EurekaAgent::CheckInsErrState::isInColdDown(const Duration &dur) const
    {
        if (errStep <= 0)
            return false;
        auto cd = GetColdDown(errStep);
        cd *= 1000; // milli seconds
        return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() <= cd;
    }
    void EurekaAgent::CheckInsErrState::occurErr()
    {
        if (0 == errorCount)
            errTime = std::chrono::steady_clock::now();
        ++errorCount;
    }
    void EurekaAgent::CheckInsErrState::sucRequest()
    {
        ++goodCount;
    }
    bool EurekaAgent::CheckInsErrState::tryChoose()
    {
        // no error, can choose
        if (0 == errStep && 0 == errorCount)
            return true;
        
        auto tpNow = std::chrono::steady_clock::now();
        auto dur = tpNow - errTime;
        if (isInColdDown(dur))
            return false;

        if (0 == inChoosingCount || 0 == errorCountPrev)
            return true;

        return false;
    }
    void EurekaAgent::CheckInsErrState::nextCheck()
    {
        errorCountPrev = errorCount;
        if (errStep > 0)
        {
            auto tpNow = std::chrono::steady_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(tpNow - errTime).count();
            if (0 == errorCount && goodCount > 0)
            {
                // become good, cd decrease
                --errStep;
            }
            else if (dur >= GetTimeoutToErrStepDecrease(errStep)*1000)
            {
                // no choose and elapsed {10,30,60,120} seconds
                --errStep;
            }
            else if (errorCount > 0 && errStep < ERR_STEP_COUNT)
            {
                // still error
                ++errStep;
            }
        }
        else if (errorCount > 0)
        {
            errStep = 1;
        }

        errorCount = 0;
        goodCount = 0;
    }
    void EurekaAgent::CheckInsErrState::reset()
    {
        *this = CheckInsErrState{};
    }


    EurekaAgent::InsHttpClient::InsHttpClient(const CheckInsDataPtr &checkIns_, EurekaAgent *eAgent_, lock_type *appLock_)
        : ins(checkIns_->ins)
        , eAgent(eAgent_)
        , checkIns(checkIns_)
        , appLock(appLock_)
    {
        if (eAgent)
            eAgent->onInsHttpClientConstruct(*this);
    }
    EurekaAgent::InsHttpClient::~InsHttpClient()
    {
        if (eAgent)
            eAgent->onInsHttpClientDestroy(*this);
    }
    GetResponse EurekaAgent::InsHttpClient::request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data)
    {
        auto tpPrev = std::chrono::steady_clock::now();
        try 
        {
            auto resp = checkIns->cli->request(method, path, query, data);

            auto tpNow = std::chrono::steady_clock::now();
            auto respMicroSec = std::chrono::duration_cast<std::chrono::microseconds>(tpNow - tpPrev).count();
            eAgent->onInsHttpClientRequestDone(*this, true, respMicroSec);
            return resp;
        }
        catch(Error &e)
        {
            // TODO trace it
            auto tpNow = std::chrono::steady_clock::now();
            auto respMicroSec = std::chrono::duration_cast<std::chrono::microseconds>(tpNow - tpPrev).count();
            eAgent->onInsHttpClientRequestDone(*this, false, respMicroSec);

            throw e;
        }
    }
    std::string EurekaAgent::InsHttpClient::requestRespData(HttpMethod method, const std::string& path, const std::string& query, const std::string *data)
    {
        auto tpPrev = std::chrono::steady_clock::now();
        bool hasDone = false;
        try 
        {
            auto resp = checkIns->cli->request(method, path, query, data);
            
            // status error
            auto &&status = std::get<0>(resp);
            if (status.code()/100 == 5)
            {
                // 5xx httpcode
                throw BadStatus(http::Status(status.code()), "5xx httpcode");
            }

            auto tpNow = std::chrono::steady_clock::now();
            auto respMicroSec = std::chrono::duration_cast<std::chrono::microseconds>(tpNow - tpPrev).count();
            eAgent->onInsHttpClientRequestDone(*this, true, respMicroSec);
            hasDone = true;
            if (status.code()/100 != 2)
            {
                // not 2xx httpcode
                throw BadStatus(http::Status(status.code()), "not 2xx httpcode");
            }
            return std::move(std::get<2>(resp));
        }
        catch(Error &e)
        {
            // TODO trace it
            if (!hasDone)
            {
                auto tpNow = std::chrono::steady_clock::now();
                auto respMicroSec = std::chrono::duration_cast<std::chrono::microseconds>(tpNow - tpPrev).count();
                eAgent->onInsHttpClientRequestDone(*this, false, respMicroSec);
                hasDone = true;
            }

            throw e;
        }
    }
}}