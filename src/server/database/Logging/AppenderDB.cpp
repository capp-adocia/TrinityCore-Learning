#include "AppenderDB.h"
#include "DatabaseEnv.h"
#include "LogMessage.h"
#include "PreparedStatement.h"

AppenderDB::AppenderDB(uint8 id, std::string const& name, LogLevel level, AppenderFlags /*flags*/, std::vector<std::string_view> const& /*args*/)
    : Appender(id, name, level), realmId(0), enabled(false) { }

AppenderDB::~AppenderDB() { }

void AppenderDB::_write(LogMessage const* message)
{
    // 避免无限循环，PExecute 会触发 "sql.sql" 类型的 Logging
    if (!enabled || (message->type.find("sql") != std::string::npos))
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_LOG);
    stmt->setUInt64(0, message->mtime);
    stmt->setUInt32(1, realmId);
    stmt->setString(2, message->type);
    stmt->setUInt8(3, uint8(message->level));
    stmt->setString(4, message->text);
    LoginDatabase.Execute(stmt);
}

void AppenderDB::setRealmId(uint32 _realmId)
{
    enabled = true;
    realmId = _realmId;
}
