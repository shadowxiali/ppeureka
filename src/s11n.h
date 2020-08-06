//  Copyright (c) 2020-2020 shadowxiali <276404541@qq.com>
//
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ppeureka/config.h"
#include "ppeureka/error.h"
#include <json11.hpp>
#include <vector>
#include <chrono>
#include <set>
#include <map>
#include <string>
#include <memory>
#include <deque>


namespace ppeureka { namespace s11n {

    using json11::Json;

    // ================= Json To Value ==============================

    namespace detail {
        inline Json parse_json(const std::string &s)
        {
            std::string err;
            auto obj = Json::parse(s, err);
            if (!err.empty())
                throw FormatError(std::move(err));
            return obj;
        }
    }

    inline int jtoi(const Json& src)
    {
        if (src.is_number())
            return static_cast<int>(src.int_value());
        else if (src.is_string())
            return std::stoi(src.string_value());
        else if (src.is_bool())
            return src.bool_value() ? 1 : 0;
        return 0;
    }

    inline int64_t jtoll(const Json& src)
    {
        if (src.is_number())
            return static_cast<int64_t>(src.number_value() + 0.0000001);
        else if (src.is_string())
            return std::stoll(src.string_value());
        else if (src.is_bool())
            return src.bool_value() ? 1 : 0;
        return 0;
    }

    inline uint64_t jtoull(const Json& src)
    {
        if (src.is_number())
            return static_cast<uint64_t>(src.number_value() + 0.0000001);
        else if (src.is_string())
            return std::stoull(src.string_value());
        else if (src.is_bool())
            return src.bool_value() ? 1 : 0;
        return 0;
    }

    inline bool jtob(const Json& src)
    {
        if (src.is_bool())
            return src.bool_value();
        else if (src.is_number())
            return src.int_value() != 0;
        else if (src.is_string())
            return src.string_value().compare("true") == 0;
        return false;
    }

    inline void load(const Json& src, bool& dst)
    {
        dst = jtob(src);
    }

    inline void load(const Json& src, int& dst)
    {
        dst = jtoi(src);
    }

    inline void load(const Json& src, int64_t& dst)
    {
        // TODO: support full precision of int64_t in json11
        dst = jtoll(src);
    }

    inline void load(const Json& src, uint64_t& dst)
    {
        // TODO: support full precision of uint64_t in json11
        dst = jtoull(src);
    }

    inline void load(const Json& src, std::string& dst)
    {
        dst = src.string_value();
    }

    template<class T>
    void load(const Json& src, std::shared_ptr<T>& dst);

    template<class T>    
    void load(const Json& src, std::vector<T>& dst)
    {
        const auto& arr = src.array_items();
        
        dst.clear();
        dst.reserve(arr.size());

        for (const auto& i : arr)
        {
            T t;
            load(i, t);
            dst.push_back(std::move(t));
        }
    }

    template<class T>    
    void load(const Json& src, std::deque<T>& dst)
    {
        const auto& arr = src.array_items();
        
        dst.clear();

        for (const auto& i : arr)
        {
            T t;
            load(i, t);
            dst.push_back(std::move(t));
        }
    }

    template<class T>
    void load(const Json& src, std::set<T>& dst)
    {
        const auto& arr = src.array_items();

        dst.clear();

        for (const auto& i : arr)
        {
            T t;
            load(i, t);
            dst.insert(std::move(t));
        }
    }

    template<class T>
    void load(const Json& src, std::map<std::string, T>& dst)
    {
        const auto& obj = src.object_items();

        dst.clear();

        for (const auto& i : obj)
        {
            T t;
            load(i.second, t);
            dst.emplace(i.first, std::move(t));
        }
    }

    template<class T>
    void load(const Json& src, std::shared_ptr<T>& dst)
    {
        if (src.is_null())
        {
            dst.reset();
            return;
        }
        if (!dst)
        {
            dst = std::make_shared<T>();
        }
        load(src, *dst);
    }

    template<class T>
    void load(const Json& src, T& dst, const char *name)
    {
        load(src[name], dst);
    }

    template<class T>
    T parseJson(const std::string& jsonStr)
    {
        using namespace s11n;
        
        auto obj = detail::parse_json(jsonStr);
        T t;
        load(obj, t);
        return t;
    }

    // ================= Value TO Json ==============================

    inline void to_json(Json::object &dst, bool src, const char *name)
    {
        dst[name] = src;
    }

    inline void to_json(Json::object &dst, const std::string &src, const char *name)
    {
        dst[name] = src;
    }

    inline void to_json(Json::object &dst, int src, const char *name)
    {
        dst[name] = src;
    }

    inline void to_json(Json::object &dst, int64_t src, const char *name)
    {
        // json11 cannot input int64_t
        dst[name] = static_cast<double>(src);
    }

    template<class T>
    void to_json(Json::object &dst, const std::shared_ptr<T>& src, const char *name);

    template<class T>
    void to_json(Json::object &dst, const std::map<std::string, T>& src, const char *name)
    {
        dst[name] = Json::object(src.begin(), src.end());
    }

    template<class T>
    void to_json(Json::object &dst, const std::map<std::string, T>& src)
    {
        dst.insert(src.begin(), src.end());
    }

    template<class T>
    void to_json(Json::object &dst, const std::shared_ptr<T>& src, const char *name)
    {
        if (!src)
        {
            dst[name] = Json{nullptr};
            return;
        }
        // as object
        Json::object sub_obj;
        to_json(sub_obj, *src);
        dst[name] = std::move(sub_obj);
    }
}}
