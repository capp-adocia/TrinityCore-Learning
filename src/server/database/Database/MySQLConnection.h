#ifndef _MYSQLCONNECTION_H
#define _MYSQLCONNECTION_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

template <typename T>
class ProducerConsumerQueue;

class DatabaseWorker;
class MySQLPreparedStatement;
class SQLOperation;

// 连接支持的模式
enum ConnectionFlags
{
    CONNECTION_ASYNC = 0x1,
    CONNECTION_SYNCH = 0x2,
    CONNECTION_BOTH = CONNECTION_ASYNC | CONNECTION_SYNCH // 异步或同步
};

// 连接信息
struct TC_DATABASE_API MySQLConnectionInfo
{
    explicit MySQLConnectionInfo(std::string const& infoString);

    std::string user;
    std::string password;
    std::string database;
    std::string host;
    std::string port_or_socket;
    std::string ssl;
};
// 使用的是MySQL最基础的C库libmysqlclient
class TC_DATABASE_API MySQLConnection // MySQL基类连接
{
    template <class T> friend class DatabaseWorkerPool;
    friend class PingOperation;

    public:
        MySQLConnection(MySQLConnectionInfo& connInfo); // 同步连接的构造函数
        MySQLConnection(ProducerConsumerQueue<SQLOperation*>* queue, MySQLConnectionInfo& connInfo);  //! Constructor for 异步 connections.
        virtual ~MySQLConnection();

        virtual uint32 Open();
        void Close();

        bool PrepareStatements(); // 准备所有语句

        bool Execute(char const* sql); // 执行sql语句
        bool Execute(PreparedStatementBase* stmt); // 执行准备好的sql语句
        ResultSet* Query(char const* sql);
        PreparedResultSet* Query(PreparedStatementBase* stmt);
        bool _Query(char const* sql, MySQLResult** pResult, MySQLField** pFields, uint64* pRowCount, uint32* pFieldCount);
        bool _Query(PreparedStatementBase* stmt, MySQLPreparedStatement** mysqlStmt, MySQLResult** pResult, uint64* pRowCount, uint32* pFieldCount);

        void BeginTransaction();
        void RollbackTransaction();
        void CommitTransaction();
        int ExecuteTransaction(std::shared_ptr<TransactionBase> transaction);
        size_t EscapeString(char* to, const char* from, size_t length);
        void Ping(); // 检查连接是否仍然有效

        uint32 GetLastError();

    protected:
        // 尝试获取锁。如果另一个线程获取了锁
        // 调用父线程将只是尝试另一个连接
        bool LockIfReady();
        
        // 调用父数据库池。将允许其他线程访问此连接
        void Unlock();

        uint32 GetServerVersion() const;
        MySQLPreparedStatement* GetPreparedStatement(uint32 index);
        void PrepareStatement(uint32 index, std::string const& sql, ConnectionFlags flags);

        virtual void DoPrepareStatements() = 0; // 子类实现对应的sql语句初始化准备
        // 将MySQL
        typedef std::vector<std::unique_ptr<MySQLPreparedStatement>> PreparedStatementContainer;

        PreparedStatementContainer           m_stmts;         // 存储准备好的sql语句
        bool                                 m_reconnecting;  // 重连
        bool                                 m_prepareError;  // 准备语句时是否有错误

    private:
        bool _HandleMySQLErrno(uint32 errNo, uint8 attempts = 5);

        ProducerConsumerQueue<SQLOperation*>* m_queue;      // 同步连接共享的队列
        std::unique_ptr<DatabaseWorker> m_worker;           // 工作线程
        MySQLHandle*          m_Mysql;                      //! MySQL Handle. // 连接
        MySQLConnectionInfo&  m_connectionInfo;             //! Connection info (used for logging) // 连接信息
        ConnectionFlags       m_connectionFlags;            //! Connection flags (for preparing relevant statements) // 连接标志
        std::mutex            m_Mutex; // Mutex for prepared statements // 准备语句的互斥锁

        // 虽然MySQLConnection禁止拷贝构造和赋值，但可以有多个这个实例
        MySQLConnection(MySQLConnection const& right) = delete; // 禁止拷贝构造
        MySQLConnection& operator=(MySQLConnection const& right) = delete; // 禁止赋值操作
};

#endif
