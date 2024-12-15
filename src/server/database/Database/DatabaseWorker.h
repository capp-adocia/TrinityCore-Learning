#ifndef _WORKERTHREAD_H
#define _WORKERTHREAD_H

#include "Define.h"
#include <atomic>
#include <thread>

template <typename T>
class ProducerConsumerQueue;

class MySQLConnection;
class SQLOperation;
// 处理数据库连接的工作线程
// 其中生产者（其他线程）将 SQL 操作放入队列，消费者（DatabaseWorker 的工作线程）从队列中取出并执行操作。
class TC_DATABASE_API DatabaseWorker
{
    public:
        DatabaseWorker(ProducerConsumerQueue<SQLOperation*>* newQueue, MySQLConnection* connection);
        ~DatabaseWorker();

    private:
        ProducerConsumerQueue<SQLOperation*>* _queue;
        MySQLConnection* _connection;

        void WorkerThread();
        std::thread _workerThread;

        std::atomic<bool> _cancelationToken;

        DatabaseWorker(DatabaseWorker const& right) = delete;
        DatabaseWorker& operator=(DatabaseWorker const& right) = delete;
};

#endif
