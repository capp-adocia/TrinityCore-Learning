#include "DatabaseLoader.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBUpdater.h"
#include "Log.h"

#include <mysqld_error.h>

DatabaseLoader::DatabaseLoader(std::string const& logger, uint32 const defaultUpdateMask)
    : _logger(logger)
    , _autoSetup(sConfigMgr->GetBoolDefault("Updates.AutoSetup", true)),
    _updateFlags(sConfigMgr->GetIntDefault("Updates.EnableDatabases", defaultUpdateMask))
{}

template <class T>
DatabaseLoader& DatabaseLoader::AddDatabase(DatabaseWorkerPool<T>& pool, std::string const& name)
{
    bool const updatesEnabledForThis = DBUpdater<T>::IsEnabled(_updateFlags);

    _open.push([this, name, updatesEnabledForThis, &pool]() -> bool
    {
        // 检查是否有数据库信息
        std::string const dbString = sConfigMgr->GetStringDefault(name + "DatabaseInfo", "");
        /*
            返回一个MySQL连接信息
            std::string user;
            std::string password;
            std::string database;
            std::string host;
            std::string port_or_socket;
            std::string ssl;
        */
        if (dbString.empty())
        {
            TC_LOG_ERROR(_logger, "Database {} not specified in configuration file!", name);
            return false;
        }
        // 检查数据库异步连接线程数（处理哪些不在意顺序的操作）
        uint8 const asyncThreads = uint8(sConfigMgr->GetIntDefault(name + "Database.WorkerThreads", 1));
        if (asyncThreads < 1 || asyncThreads > 32)
        {
            TC_LOG_ERROR(_logger, "{} database: invalid number of worker threads specified. "
                "Please pick a value between 1 and 32.", name);
            return false;
        }
        // 检查数据库同步线程数（处理比如像事务等有顺序的操作）
        uint8 const synchThreads = uint8(sConfigMgr->GetIntDefault(name + "Database.SynchThreads", 1));
        // 设置数据库连接信息（）
        pool.SetConnectionInfo(dbString, asyncThreads, synchThreads);
        if (uint32 error = pool.Open()) // 正式打开数据库连接
        {
            // Database does not exist
            if ((error == ER_BAD_DB_ERROR) && updatesEnabledForThis && _autoSetup)
            {
                // Try to create the database and connect again if auto setup is enabled
                if (DBUpdater<T>::Create(pool) && (!pool.Open()))
                    error = 0;
            }

            // If the error wasn't handled quit
            if (error)
            {
                TC_LOG_ERROR("sql.driver", "\nDatabasePool {} NOT opened. There were errors opening the MySQL connections. Check your SQLDriverLogFile "
                    "for specific errors. Read wiki at http://www.trinitycore.info/display/tc/TrinityCore+Home", name);

                return false;
            }
        }
        // Add the close operation
        _close.push([&pool]
        {
            pool.Close(); // 每次调用添加数据库时，都是先添加close操作，再添加open操作
        });
        return true;
    });

    // Populate and update only if updates are enabled for this pool
    if (updatesEnabledForThis)
    {
        _populate.push([this, name, &pool]() -> bool
        {
            if (!DBUpdater<T>::Populate(pool))
            {
                TC_LOG_ERROR(_logger, "Could not populate the {} database, see log for details.", name);
                return false;
            }
            return true;
        });

        _update.push([this, name, &pool]() -> bool
        {
            if (!DBUpdater<T>::Update(pool))
            {
                TC_LOG_ERROR(_logger, "Could not update the {} database, see log for details.", name);
                return false;
            }
            return true;
        });
    }

    _prepare.push([this, name, &pool]() -> bool
    {
        if (!pool.PrepareStatements())
        {
            TC_LOG_ERROR(_logger, "Could not prepare statements of the {} database, see log for details.", name);
            return false;
        }
        return true;
    });

    return *this;
}

bool DatabaseLoader::Load()
{
    if (!_updateFlags)
        TC_LOG_INFO("sql.updates", "Automatic database updates are disabled for all databases!");

    if (!OpenDatabases())
        return false;

    if (!PopulateDatabases())
        return false;

    if (!UpdateDatabases())
        return false;

    if (!PrepareStatements())
        return false;

    return true;
}

bool DatabaseLoader::OpenDatabases()
{
    return Process(_open);
}

bool DatabaseLoader::PopulateDatabases()
{
    return Process(_populate);
}

bool DatabaseLoader::UpdateDatabases()
{
    return Process(_update);
}

bool DatabaseLoader::PrepareStatements()
{
    return Process(_prepare);
}

bool DatabaseLoader::Process(std::queue<Predicate>& queue)
{
    while (!queue.empty())
    {
        // 失败了就关闭所有的数据库连接池
        if (!queue.front()()) // 调用队列里面的lambda如果返回false，则表示失败
        {
            // 关闭所有已注册关闭操作的打开数据库
            while (!_close.empty())
            {
                _close.top()();
                _close.pop();
            }

            return false;
        }
        // 成功了就弹出队列
        queue.pop();
    }
    return true;
}

// 这三个就是数据库连接的具体实例，登录数据库服务，角色数据库服务，世界数据库服务
template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<LoginDatabaseConnection>(DatabaseWorkerPool<LoginDatabaseConnection>&, std::string const&);
template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<CharacterDatabaseConnection>(DatabaseWorkerPool<CharacterDatabaseConnection>&, std::string const&);
template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<WorldDatabaseConnection>(DatabaseWorkerPool<WorldDatabaseConnection>&, std::string const&);
