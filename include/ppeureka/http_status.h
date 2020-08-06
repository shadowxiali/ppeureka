//  Copyright (c) 2014-2017 Andrey Upadyshev <oliora@gmail.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include <string>


namespace ppeureka { namespace http {

    enum HttpCode
    {
        HC_OK = 200,
        HC_Created = 201,
        HC_NoContent = 204,

        HC_TemporaryRedirect = 307,
        
        HC_BadRequest = 400,
        HC_Forbidden = 403,
        HC_NotFound = 404,
        HC_PreconditionFailed = 412,

        HC_InternalServerError = 500,
    };

    class Status
    {
    public:
        explicit Status(int code = 0) PPEUREKA_NOEXCEPT
        : m_code(code)
        {}

        Status(int code, std::string message)
        : m_code(code)
        , m_message(std::move(message))
        {}

        // Returns true if code() is 2xx (i.e. success) and false otherwise
        bool success() const PPEUREKA_NOEXCEPT{ return 2 == m_code / 100; }

        int code() const PPEUREKA_NOEXCEPT{ return m_code; }
        const std::string& message() const PPEUREKA_NOEXCEPT{ return m_message; }

    private:
        int m_code;
        std::string m_message;
    };

}}
