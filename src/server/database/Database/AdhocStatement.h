#ifndef _ADHOCSTATEMENT_H
#define _ADHOCSTATEMENT_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "SQLOperation.h"

/*! Raw, ad-hoc query. */
// 异步或同步地执行 SQL 查询
class TC_DATABASE_API BasicStatementTask : public SQLOperation
{
    public:
        BasicStatementTask(char const* sql, bool async = false);
        ~BasicStatementTask();

        bool Execute() override;
        QueryResultFuture GetFuture() const { return m_result->get_future(); }

    private:
        char const* m_sql;      //- Raw query to be executed
        bool m_has_result;
        QueryResultPromise* m_result;
};

#endif
