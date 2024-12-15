#ifndef _QUERY_CALLBACK_H
#define _QUERY_CALLBACK_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include <functional>
#include <future>
#include <list>
#include <queue>
#include <utility>
// 封装数据库查询的异步处理和回调机制。
// 通过使用 std::future 来异步地处理查询，并允许在查询完成后执行自定义的回调函数、支持链式回调
class TC_DATABASE_API QueryCallback
{
public:
    explicit QueryCallback(QueryResultFuture&& result);
    explicit QueryCallback(PreparedQueryResultFuture&& result);
    QueryCallback(QueryCallback&& right);
    QueryCallback& operator=(QueryCallback&& right);
    ~QueryCallback();

    QueryCallback&& WithCallback(std::function<void(QueryResult)>&& callback);
    QueryCallback&& WithPreparedCallback(std::function<void(PreparedQueryResult)>&& callback);

    QueryCallback&& WithChainingCallback(std::function<void(QueryCallback&, QueryResult)>&& callback);
    QueryCallback&& WithChainingPreparedCallback(std::function<void(QueryCallback&, PreparedQueryResult)>&& callback);

    // Moves std::future from next to this object
    void SetNextQuery(QueryCallback&& next);

    // returns true when completed
    bool InvokeIfReady();

private:
    QueryCallback(QueryCallback const& right) = delete;
    QueryCallback& operator=(QueryCallback const& right) = delete;

    template<typename T> friend void ConstructActiveMember(T* obj);
    template<typename T> friend void DestroyActiveMember(T* obj);
    template<typename T> friend void MoveFrom(T* to, T&& from);

    union
    {
        QueryResultFuture _string;
        PreparedQueryResultFuture _prepared;
    };
    bool _isPrepared;

    struct QueryCallbackData; // 数据库查询回调数据
    std::queue<QueryCallbackData, std::list<QueryCallbackData>> _callbacks;
};

// QueryCallback的嵌套结构体QueryCallbackData，用于存储回调函数和数据
struct QueryCallback::QueryCallbackData
{
public:
    friend class QueryCallback;

    QueryCallbackData(std::function<void(QueryCallback&, QueryResult)>&& callback) : _string(std::move(callback)), _isPrepared(false) { }
    QueryCallbackData(std::function<void(QueryCallback&, PreparedQueryResult)>&& callback) : _prepared(std::move(callback)), _isPrepared(true) { }
    QueryCallbackData(QueryCallbackData&& right)
    {
        _isPrepared = right._isPrepared;
        ConstructActiveMember(this);
        MoveFrom(this, std::move(right));
    }
    QueryCallbackData& operator=(QueryCallbackData&& right)
    {
        if (this != &right)
        {
            if (_isPrepared != right._isPrepared)
            {
                DestroyActiveMember(this);
                _isPrepared = right._isPrepared;
                ConstructActiveMember(this);
            }
            MoveFrom(this, std::move(right));
        }
        return *this;
    }
    ~QueryCallbackData() { DestroyActiveMember(this); }

private:
    QueryCallbackData(QueryCallbackData const&) = delete;
    QueryCallbackData& operator=(QueryCallbackData const&) = delete;

    template<typename T> friend void ConstructActiveMember(T* obj);
    template<typename T> friend void DestroyActiveMember(T* obj);
    template<typename T> friend void MoveFrom(T* to, T&& from);

    union
    {
        std::function<void(QueryCallback&, QueryResult)> _string;
        std::function<void(QueryCallback&, PreparedQueryResult)> _prepared;
    };
    bool _isPrepared;
};


#endif // _QUERY_CALLBACK_H
