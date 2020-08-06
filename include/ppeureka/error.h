//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "ppeureka/config.h"
#include "ppeureka/http_status.h"
#include <stdexcept>


namespace ppeureka {

    class Error: public std::exception 
    {
    public:
        Error() = default;
        explicit Error(std::string message)
        : m_message(std::move(message))
        {}

        virtual const char *what() const PPEUREKA_NOEXCEPT override { return m_message.c_str(); }

    protected:
        std::string m_message;
    };

    class ParamError: public Error{
    public:
        using Error::Error;
    };

    class NetError: public Error{
    public:
        using Error::Error;
    };
    
    class FormatError: public Error{
    public:
        using Error::Error;
    };

    class OperationAborted: public Error
    {
    public:
        OperationAborted() = default;

        virtual const char *what() const PPEUREKA_NOEXCEPT override { return "Operation aborted"; }
    };

    class BadStatus: public Error
    {
        using Error::m_message;
    public:
        explicit BadStatus(http::Status status, std::string message = "")
        : m_status(std::move(status))
        , Error(std::move(message))
        {}

        int code() const PPEUREKA_NOEXCEPT{ return m_status.code(); }

        const http::Status& status() const PPEUREKA_NOEXCEPT{ return m_status; }
        const std::string& message() const PPEUREKA_NOEXCEPT{ return m_message; }

    protected:
        http::Status m_status;
    };

    class NotFoundError: public BadStatus
    {
    public:
        enum { Code = 404 };

        /*explicit NotFoundError(http::Status status, std::string message = "")
        : BadStatus(std::move(status), std::move(message))
        {}*/

        NotFoundError()
        : BadStatus(http::Status(Code, "Not Found"))
        {}
    };

    void throwStatusError(http::Status status, std::string data = "");

}
