//  Copyright (c) 2014-2017 Andrey Upadyshev <oliora@gmail.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include <stdint.h>
#include <chrono>
#include <unordered_map>
#include <string>

namespace ppeureka {

    using ResponseHeaders = std::unordered_map<std::string, std::string>;
    
    template<class Data>
    struct Response
    {
    public:
        using DataType = Data;

        Response()
        {}

        Response(const ResponseHeaders& headers)
        : m_headers(headers)
        {}

        Response(ResponseHeaders&& headers)
        : m_headers(std::move(headers))
        {}

        Response(const ResponseHeaders& headers, const DataType& data)
        : m_value(data)
        , m_headers(headers)
        {}

        Response(const ResponseHeaders& headers, DataType&& data)
        : m_value(std::move(data))
        , m_headers(headers)
        {}

        ResponseHeaders& headers() { return m_headers; }
        const ResponseHeaders& headers() const { return m_headers; }

        void headers(const ResponseHeaders& headers) { m_headers = headers; }

        DataType& data() { return m_value; }
        const DataType& data() const { return m_value; }

        void data(const DataType& data) { m_value = data; }
        void data(DataType&& data) { m_value = std::move(data); }

    private:
        DataType m_value;
        ResponseHeaders m_headers;
    };

    template<class DataType>
    Response<DataType> makeResponse(const ResponseHeaders& headers, DataType&& data)
    {
        return Response<DataType>(headers, std::forward<DataType>(data));
    }
}
