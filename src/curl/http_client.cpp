//  Copyright (c)  2014-2017 Andrey Upadyshev <oliora@gmail.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "http_client.h"
#include "../http_helpers.h"
#include <ppeureka/helpers.h>
#include <algorithm>
#include <tuple>
#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include <regex>

#if (LIBCURL_VERSION_MAJOR < 7)
#error "Where did you get such an ancient libcurl?"
#endif

// CURLOPT_SSL_VERIFYSTATUS was added in libcurl 7.41.0
// https://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYSTATUS.html
#if (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 41)
#define PPCONSUL_DISABLE_SSL_VERIFYSTATUS
#endif


namespace ppeureka { namespace curl {

    using namespace ppeureka::http::impl;

    namespace {
        struct CurlInitializer
        {
            CurlInitializer()
            {
                m_initialized = 0 == curl_global_init(CURL_GLOBAL_DEFAULT | CURL_GLOBAL_SSL);
            }

            ~CurlInitializer()
            {
                curl_global_cleanup();
            }

            CurlInitializer(const CurlInitializer&) = delete;
            CurlInitializer& operator= (const CurlInitializer&) = delete;

            explicit operator bool() const { return m_initialized; }

        private:
            bool m_initialized;
        };

#if !defined PPCONSUL_USE_BOOST_REGEX
        using std::regex;
        using std::regex_match;
        using std::cmatch;
#else
        using boost::regex;
        using boost::regex_match;
        using boost::cmatch;
#endif

        const regex g_statusLineRegex(R"***(HTTP\/1\.1 +(\d\d\d) +(.*)\r\n)***");
        const regex g_headerLineRegex(R"***(([^:]+): +(.+)\r\n)***");

        inline bool parseStatus(http::Status& status, const char *buf, size_t size)
        {
            cmatch match;
            if (!regex_match(buf, buf + size, match, g_statusLineRegex))
                return false;
            status = http::Status(std::atol(match[1].str().c_str()), match[2].str());
            return true;
        }

        void throwCurlError(CURLcode code, const char *err, bool isNetError, bool isParamError)
        {
            if (code == CURLE_ABORTED_BY_CALLBACK)
                throw ppeureka::OperationAborted();
            else if (isNetError)
                throw ppeureka::NetError(std::string(err) + " (" + std::to_string(code) + ")");
            else if (isParamError)
                throw ppeureka::ParamError(std::string(err) + " (" + std::to_string(code) + ")");
            else
                throw ppeureka::Error(std::string(err) + " (" + std::to_string(code) + ")");
        }

        enum { Buffer_Size = 16384 };

        using ReadContext = std::pair<const std::string *, size_t>;

        size_t headerStatusCallback(char *ptr, size_t size_, size_t nitems, void *outputStatus)
        {
            const auto size = size_ * nitems;
            parseStatus(*static_cast<http::Status *>(outputStatus), ptr, size);
            return size;
        }

        size_t headerCallback(char *ptr, size_t size_, size_t nitems, void *outputResponse_)
        {
            const auto size = size_ * nitems;
            auto outputResponse = reinterpret_cast<HttpClient::GetResponse *>(outputResponse_);

            if (parseStatus(std::get<0>(*outputResponse), ptr, size))
                return size;

            // Parse headers
            cmatch match;
            if (!regex_match(const_cast<const char *>(ptr), const_cast<const char *>(ptr) +size, match, g_headerLineRegex))
                return size;

            ResponseHeaders& headers = std::get<1>(*outputResponse);

            headers[match[1].str()] = match[2].str();
            return size;
        }

        size_t writeCallback(char *ptr, size_t size_, size_t nitems, void *outputStr)
        {
            const auto size = size_ * nitems;
            static_cast<std::string *>(outputStr)->append(ptr, size);
            return size;
        }

        size_t readCallback(char *buffer, size_t size_, size_t nitems, void *readContext)
        {
            const auto ctx = static_cast<ReadContext *>(readContext);

            if (!ctx->first)
            {
                return 0;
            }

            const auto remainingSize = ctx->first->size() - ctx->second;
            if (!remainingSize)
                return 0;

            auto size = (std::min)(size_ * nitems, remainingSize);
            memcpy(buffer, ctx->first->data() + ctx->second, size);
            ctx->second += size;
            return size;
        }

        int progressCallback(void *clientPtr, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
        {
            const auto* client = static_cast<const HttpClient*>(clientPtr);
            return client->isStopped();
        }
    }

    void HttpClient::setupTls(const TlsConfig& tlsConfig)
    {
        if (!tlsConfig.cert.empty())
            setopt(CURLOPT_SSLCERT, tlsConfig.cert.c_str());
        if (!tlsConfig.certType.empty())
            setopt(CURLOPT_CAPATH, tlsConfig.certType.c_str());
        if (!tlsConfig.key.empty())
            setopt(CURLOPT_SSLKEY, tlsConfig.key.c_str());
        if (!tlsConfig.keyType.empty())
            setopt(CURLOPT_CAPATH, tlsConfig.certType.c_str());
        if (!tlsConfig.caPath.empty())
            setopt(CURLOPT_CAPATH, tlsConfig.caPath.c_str());
        if (!tlsConfig.caInfo.empty())
            setopt(CURLOPT_CAINFO, tlsConfig.caInfo.c_str());

        if (tlsConfig.keyPass && *tlsConfig.keyPass)
            setopt(CURLOPT_KEYPASSWD, tlsConfig.keyPass);

        setopt(CURLOPT_SSL_VERIFYPEER, tlsConfig.verifyPeer ? 1 : 0);
        setopt(CURLOPT_SSL_VERIFYHOST, tlsConfig.verifyHost ? 2 : 0);

        if (tlsConfig.verifyStatus)
        {
#ifdef PPCONSUL_DISABLE_SSL_VERIFYSTATUS
            throw ppeureka::Error("ppeureka was built without support for CURLOPT_SSL_VERIFYSTATUS");
#else
            setopt(CURLOPT_SSL_VERIFYSTATUS, 1);
#endif
        }
    }

    HttpClient::~HttpClient() = default;

    void HttpClient::start(const std::string& endpoint, const TlsConfig& tlsConfig)
    {
        static const CurlInitializer g_initialized;

        if (!g_initialized)
            throw ppeureka::Error("CURL was not successfully initialized");

        m_handle.reset(curl_easy_init());
        if (!m_handle)
            throw ppeureka::Error("CURL handle creation failed");

        memset(m_errBuffer, 0, sizeof(m_errBuffer));
        if (auto err = curl_easy_setopt(handle(), CURLOPT_ERRORBUFFER, m_errBuffer))
            throwCurlError(err, "", false, false);

        setEndpoint(endpoint);

#if (LIBCURL_VERSION_NUM >= 0x073200)
        m_enableStop = true;
        setopt(CURLOPT_NOPROGRESS, 0l);
        setopt(CURLOPT_XFERINFOFUNCTION, &progressCallback);
        setopt(CURLOPT_XFERINFODATA, this);
#else
        m_enableStop = false;
        setopt(CURLOPT_NOPROGRESS, 1l);
        //throw ppeureka::Error("Ppconsul is built without support for stopping the client (libcurl 7.32.0 or newer is required)");
#endif

        // TODO: CURLOPT_NOSIGNAL?
        setopt(CURLOPT_WRITEFUNCTION, &writeCallback);
        setopt(CURLOPT_READFUNCTION, &readCallback);

        setupTls(tlsConfig);
    }

    void HttpClient::setEndpoint(const std::string &endpoint)
    {
        auto al = get_lock_param();

        m_endpoint = endpoint;
    }

    HttpClient::GetResponse HttpClient::request(HttpMethod method, const std::string& path, const std::string& query, const std::string *data)
    {
        std::string url;
        {
            auto al = get_lock_param();
            url = makeUrl(path, query);
        }
        auto al = get_lock_request();

        GetResponse r;
        std::get<2>(r).reserve(Buffer_Size);

        ReadContext ctx(data, 0u);

        setopt(CURLOPT_HEADERFUNCTION, &headerCallback);
        setopt(CURLOPT_CUSTOMREQUEST, nullptr);
        setopt(CURLOPT_URL, url.c_str());
        setopt(CURLOPT_WRITEDATA, &std::get<2>(r));
        setopt(CURLOPT_HEADERDATA, &r);

        if (METHOD_GET == method)
        {
            setopt(CURLOPT_HTTPGET, 1l);
        }
        else if (METHOD_POST == method)
        {
            setopt(CURLOPT_POST, 1l);
            if (data)
            {
                setopt(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(data->size()));
            }
            else
            {
                setopt(CURLOPT_POSTFIELDSIZE_LARGE, 0);
            }
            setopt(CURLOPT_READDATA, &ctx);
        }
        else if (METHOD_PUT == method)
        {
            setopt(CURLOPT_UPLOAD, 1l);
            setopt(CURLOPT_PUT, 1l);
            if (data)
            {
                setopt(CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(data->size()));
            }
            else
            {
                setopt(CURLOPT_POSTFIELDSIZE_LARGE, 0);
            }
            setopt(CURLOPT_READDATA, &ctx);
        }
        else if (METHOD_DELETE == method)
        {
            setopt(CURLOPT_HTTPGET, 1l);
            setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        else
        {
            throw ppeureka::Error("not supported method");
        }

        perform();

        return r;
    }

    void HttpClient::stop()
    {
        //if (!m_enableStop)
        //    throw ppeureka::Error("Must enable stop at construction time");
        m_stopped.store(true, std::memory_order_relaxed);
    }

    template<class Opt, class T>
    inline void HttpClient::setopt(Opt opt, const T& t)
    {
        const auto err = curl_easy_setopt(handle(), opt, t);
        if (err)
            throwCurlError(err, m_errBuffer, false, true);
    }

    inline void HttpClient::perform()
    {
        // set json only
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "accept: application/json");
        headers = curl_slist_append(headers, "content-type: application/json");
        setopt(CURLOPT_HTTPHEADER, headers);

        const auto err = curl_easy_perform(handle());
        curl_slist_free_all(headers);
        headers = nullptr;

        if (err)
            throwCurlError(err, m_errBuffer, true, false);
    }

}}
