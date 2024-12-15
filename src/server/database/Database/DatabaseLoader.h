#ifndef DatabaseLoader_h__
#define DatabaseLoader_h__

#include "Define.h"

#include <functional>
#include <queue>
#include <stack>
#include <string>

template <class T>
class DatabaseWorkerPool;

// 数据库加载类，用于启动所有数据库工作池，
// 处理更新、延迟准备报表并清理失败。
class TC_DATABASE_API DatabaseLoader
{
public:
    DatabaseLoader(std::string const& logger, uint32 const defaultUpdateMask);
    
    // 注册要加载的数据库（延迟实现）
    // 这里将所有需要执行的函数存储到队列中，以便稍后调用
    template <class T>
    DatabaseLoader& AddDatabase(DatabaseWorkerPool<T>& pool, std::string const& name);

    // Load all databases
    bool Load(); // 调用OpenDatabases()、PopulateDatabases()、UpdateDatabases()和PrepareStatements()

    enum DatabaseTypeFlags
    {
        DATABASE_NONE       = 0, // No database
        DATABASE_LOGIN      = 1, // 登录数据库
        DATABASE_CHARACTER  = 2, // 角色数据库
        DATABASE_WORLD      = 4, // 世界数据库
        DATABASE_MASK_ALL   = DATABASE_LOGIN | DATABASE_CHARACTER | DATABASE_WORLD
    };

private:
    bool OpenDatabases();
    bool PopulateDatabases();
    bool UpdateDatabases();
    bool PrepareStatements();

    using Predicate = std::function<bool()>;
    using Closer = std::function<void()>;

    // 调用给定队列中的所有函数，并在出现错误时关闭数据库
    // 出现错误时返回false。
    bool Process(std::queue<Predicate>& queue);

    std::string const _logger;
    bool const _autoSetup;
    uint32 const _updateFlags;
    // 使用队列存储要调用的函数，以便更加模块化地加载过程
    std::queue<Predicate> _open, _populate, _update, _prepare;
    std::stack<Closer> _close; // 这里使用栈的原因是AddDatabase中的_open被调用后会将关闭操作入栈，一旦出错了就会出栈
};

#endif // DatabaseLoader_h__
