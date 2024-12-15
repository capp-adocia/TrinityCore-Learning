// 将日志信息保存起来，方便调试
// 使用asio的多线程模型，即将一个iocontext跑在多个线程中，每个线程有一个logger
// 利用strand来保证线程安全和顺序性
#ifndef TRINITYCORE_LOG_H
#define TRINITYCORE_LOG_H

#include "Define.h"
#include "AsioHacksFwd.h"
#include "LogCommon.h"
#include "StringFormat.h"

#include <memory>
#include <unordered_map>
#include <vector>

class Appender;
class Logger;
struct LogMessage;

namespace Trinity
{
    namespace Asio
    {
        class IoContext;
    }
}

#define LOGGER_ROOT "root"

typedef Appender*(*AppenderCreatorFn)(uint8 id, std::string const& name, LogLevel level, AppenderFlags flags, std::vector<std::string_view> const& extraArgs);

template <class AppenderImpl>
Appender* CreateAppender(uint8 id, std::string const& name, LogLevel level, AppenderFlags flags, std::vector<std::string_view> const& extraArgs)
{
    return new AppenderImpl(id, name, level, flags, extraArgs);
}

class TC_COMMON_API Log
{
    typedef std::unordered_map<std::string, Logger> LoggerMap;

    private:
        Log();
        ~Log();
        Log(Log const&) = delete;
        Log(Log&&) = delete;
        Log& operator=(Log const&) = delete;
        Log& operator=(Log&&) = delete;

    public:
        static Log* instance();

        void Initialize(Trinity::Asio::IoContext* ioContext);
        void SetSynchronous();  // Not threadsafe - should only be called from main() after all threads are joined
        void LoadFromConfig();
        void Close();
        bool ShouldLog(std::string const& type, LogLevel level) const;
        bool SetLogLevel(std::string const& name, int32 level, bool isLogger = true);

        template<typename... Args>
        void OutMessage(std::string_view filter, LogLevel const level, Trinity::FormatString<Args...> fmt, Args&&... args)
        {
            this->OutMessageImpl(filter, level, fmt, Trinity::MakeFormatArgs(args...));
        }

        template<typename... Args>
        void OutCommand(uint32 account, Trinity::FormatString<Args...> fmt, Args&&... args)
        {
            if (!ShouldLog("commands.gm", LOG_LEVEL_INFO))
                return;

            this->OutCommandImpl(account, fmt, Trinity::MakeFormatArgs(args...));
        }

        void OutCharDump(char const* str, uint32 account_id, uint64 guid, char const* name);

        void SetRealmId(uint32 id);

        template<class AppenderImpl>
        void RegisterAppender()
        {
            this->RegisterAppender(AppenderImpl::type, &CreateAppender<AppenderImpl>);
        }

        std::string const& GetLogsDir() const { return m_logsDir; }
        std::string const& GetLogsTimestamp() const { return m_logsTimestamp; }

    private:
        static std::string GetTimestampStr();
        void write(std::unique_ptr<LogMessage> msg) const;
        
        Logger const* GetLoggerByType(std::string const& type) const;
        Appender* GetAppenderByName(std::string_view name);
        uint8 NextAppenderId();
        void CreateAppenderFromConfig(std::string const& name);
        void CreateLoggerFromConfig(std::string const& name);
        void ReadAppendersFromConfig();
        void ReadLoggersFromConfig();
        void RegisterAppender(uint8 index, AppenderCreatorFn appenderCreateFn);
        void OutMessageImpl(std::string_view filter, LogLevel level, Trinity::FormatStringView messageFormat, Trinity::FormatArgs messageFormatArgs);
        void OutCommandImpl(uint32 account, Trinity::FormatStringView messageFormat, Trinity::FormatArgs messageFormatArgs);

        std::unordered_map<uint8, AppenderCreatorFn> appenderFactory;
        std::unordered_map<uint8, std::unique_ptr<Appender>> appenders;
        std::unordered_map<std::string, std::unique_ptr<Logger>> loggers;
        uint8 AppenderId;
        LogLevel lowestLogLevel;

        std::string m_logsDir;
        std::string m_logsTimestamp;

        Trinity::Asio::IoContext* _ioContext;
        Trinity::Asio::Strand* _strand;
};

#define sLog Log::instance()

#ifdef PERFORMANCE_PROFILING
#define TC_LOG_MESSAGE_BODY(filterType__, level__, ...) ((void)0)
#elif TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS

// This will catch format errors on build time
#define TC_LOG_MESSAGE_BODY(filterType__, level__, ...)                 \
        do {                                                            \
            if (sLog->ShouldLog(filterType__, level__))                 \
                sLog->OutMessage(filterType__, level__, __VA_ARGS__);   \
        } while (0)
#else
#define TC_LOG_MESSAGE_BODY(filterType__, level__, ...)                 \
        __pragma(warning(push))                                         \
        __pragma(warning(disable:4127))                                 \
        do {                                                            \
            if (sLog->ShouldLog(filterType__, level__))                 \
                sLog->OutMessage(filterType__, level__, __VA_ARGS__);   \
        } while (0)                                                     \
        __pragma(warning(pop))
#endif

 // 判断当前日志级别是否大于等于最低日志级别
 // 判断appender日志级别
 
#define TC_LOG_TRACE(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_TRACE, __VA_ARGS__)

#define TC_LOG_DEBUG(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_DEBUG, __VA_ARGS__)

#define TC_LOG_INFO(filterType__, ...)  \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_INFO, __VA_ARGS__)

#define TC_LOG_WARN(filterType__, ...)  \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_WARN, __VA_ARGS__)

#define TC_LOG_ERROR(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_ERROR, __VA_ARGS__)

#define TC_LOG_FATAL(filterType__, ...) \
    TC_LOG_MESSAGE_BODY(filterType__, LOG_LEVEL_FATAL, __VA_ARGS__)

#endif
