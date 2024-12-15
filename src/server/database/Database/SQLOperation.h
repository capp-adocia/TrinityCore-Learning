#ifndef _SQLOPERATION_H
#define _SQLOPERATION_H

#include "Define.h"
#include "DatabaseEnvFwd.h"

// 用于存储SQL语句或预编译语句的联合体
union SQLElementUnion
{
    PreparedStatementBase* stmt;
    /*
        PREPARE stmt FROM 'SELECT * FROM users WHERE username = ?';
        EXECUTE stmt USING @username;
    */
    char const* query;
    /*
        SELECT * FROM users WHERE username = 'example';
    */
};

// SQL语句或预编译语句的类型
enum SQLElementDataType
{
    SQL_ELEMENT_RAW, // 原始SQL语句
    SQL_ELEMENT_PREPARED // 预编译语句
};

//- The element
struct SQLElementData
{
    SQLElementUnion element;
    SQLElementDataType type;
};

class MySQLConnection;

class TC_DATABASE_API SQLOperation
{
    public:
        SQLOperation(): m_conn(nullptr) { }
        virtual ~SQLOperation() { }

        virtual int call()
        {
            Execute();
            return 0;
        }
        virtual bool Execute() = 0;
        virtual void SetConnection(MySQLConnection* con) { m_conn = con; }

        MySQLConnection* m_conn;

    private:
        SQLOperation(SQLOperation const& right) = delete;
        SQLOperation& operator=(SQLOperation const& right) = delete;
};

#endif
