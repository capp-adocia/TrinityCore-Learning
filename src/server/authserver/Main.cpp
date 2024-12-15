/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
* @file main.cpp
* @brief Authentication Server main program
*
* This file contains the main program for the
* authentication server
*/

#include "AppenderDB.h"
#include "AuthSocketMgr.h"
#include "Banner.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "DeadlineTimer.h"
#include "IoContext.h"
#include "IPLocation.h"
#include "GitRevision.h"
#include "Locales.h"
#include "MySQLThreading.h"
#include "OpenSSLCrypto.h"
#include "ProcessPriority.h"
#include "RealmList.h"
#include "SecretMgr.h"
#include "SharedDefines.h"
#include "Util.h"
#include <boost/asio/signal_set.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>
#include <iostream>
#include <csignal>

using boost::asio::ip::tcp;
using namespace boost::program_options;
namespace fs = boost::filesystem;

#ifndef _TRINITY_REALM_CONFIG
# define _TRINITY_REALM_CONFIG  "authserver.conf"
#endif
#ifndef _TRINITY_REALM_CONFIG_DIR
    #define _TRINITY_REALM_CONFIG_DIR "authserver.conf.d"
#endif

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
#include "ServiceWin32.h"
char serviceName[] = "authserver";
char serviceLongName[] = "TrinityCore auth service";
char serviceDescription[] = "TrinityCore World of Warcraft emulator auth service";
/*
* -1 - not in service mode
*  0 - stopped
*  1 - running
*  2 - paused
*/
int m_ServiceStatus = -1;

void ServiceStatusWatcher(std::weak_ptr<Trinity::Asio::DeadlineTimer> serviceStatusWatchTimerRef, std::weak_ptr<Trinity::Asio::IoContext> ioContextRef, boost::system::error_code const& error);
#endif

bool StartDB();
void StopDB();
void SignalHandler(std::weak_ptr<Trinity::Asio::IoContext> ioContextRef, boost::system::error_code const& error, int signalNumber);
void KeepDatabaseAliveHandler(std::weak_ptr<Trinity::Asio::DeadlineTimer> dbPingTimerRef, int32 dbPingInterval, boost::system::error_code const& error);
void BanExpiryHandler(std::weak_ptr<Trinity::Asio::DeadlineTimer> banExpiryCheckTimerRef, int32 banExpiryCheckInterval, boost::system::error_code const& error);
variables_map GetConsoleArguments(int argc, char** argv, fs::path& configFile, fs::path& configDir, std::string& winServiceAction);

int main(int argc, char** argv)
{
    Trinity::Impl::CurrentServerProcessHolder::_type = SERVER_PROCESS_AUTHSERVER; // 指定当前服务器类型为认证服务器
    signal(SIGABRT, &Trinity::AbortHandler); // 当程序发生abort信号时，会调用这个函数

    Trinity::VerifyOsVersion(); // 检查操作系统版本

    Trinity::Locale::Init(); // 初始化语言环境

    auto configFile = fs::absolute(_TRINITY_REALM_CONFIG); // 获取配置文件路径
    auto configDir  = fs::absolute(_TRINITY_REALM_CONFIG_DIR); // 获取配置目录路径
    std::string winServiceAction;
    auto vm = GetConsoleArguments(argc, argv, configFile, configDir, winServiceAction); // 获取命令行参数
    // exit if help or version is enabled
    if (vm.count("help") || vm.count("version")) // 如果启用了帮助或版本选项，则退出
        return 0;

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    if (winServiceAction == "install")
        return WinServiceInstall() == true ? 0 : 1;
    if (winServiceAction == "uninstall")
        return WinServiceUninstall() == true ? 0 : 1;
    if (winServiceAction == "run")
        return WinServiceRun() ? 0 : 1;
#endif
    // linux平台
    std::string configError;
    if (!sConfigMgr->LoadInitial(configFile.generic_string(),
                                 std::vector<std::string>(argv, argv + argc),
                                 configError)) // 加载初始配置文件
    {
        printf("Error in config file: %s\n", configError.c_str());
        return 1;
    }

    std::vector<std::string> loadedConfigFiles;
    std::vector<std::string> configDirErrors;
    // 加载.conf配置文件
    bool additionalConfigFileLoadSuccess = sConfigMgr->LoadAdditionalDir(configDir.generic_string(), true, loadedConfigFiles, configDirErrors);
    for (std::string const& loadedConfigFile : loadedConfigFiles)
        printf("Loaded additional config file %s\n", loadedConfigFile.c_str());

    if (!additionalConfigFileLoadSuccess) // 如果加载额外的配置文件失败
    {
        for (std::string const& configDirError : configDirErrors)
            printf("Error in additional config files: %s\n", configDirError.c_str()); // 打印错误信息

        return 1;
    }

    std::vector<std::string> overriddenKeys = sConfigMgr->OverrideWithEnvVariablesIfAny();

    sLog->RegisterAppender<AppenderDB>();
    sLog->Initialize(nullptr); // 启动同步日志系统

    // 初始化完成，展示欢迎信息
    Trinity::Banner::Show("authserver",
        [](char const* text)
        {
            TC_LOG_INFO("server.authserver", "{}", text);
        },
        []()
        {
            TC_LOG_INFO("server.authserver", "Using configuration file {}.", sConfigMgr->GetFilename());
            TC_LOG_INFO("server.authserver", "Using SSL version: {} (library: {})", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
            TC_LOG_INFO("server.authserver", "Using Boost version: {}.{}.{}", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
        }
    );
    // 打印被环境变量覆盖的配置项
    for (std::string const& key : overriddenKeys)
        TC_LOG_INFO("server.authserver", "Configuration field '{}' was overridden with environment variable.", key);
    // 初始化OpenSSL库
    OpenSSLCrypto::threadsSetup(boost::dll::program_location().remove_filename());

    std::shared_ptr<void> opensslHandle(nullptr, [](void*) { OpenSSLCrypto::threadsCleanup(); }); // OpenSSL库清理句柄

    // 创建认证服务器PID文件
    // 主要防止多个实例重复启动
    std::string pidFile = sConfigMgr->GetStringDefault("PidFile", "");
    if (!pidFile.empty())
    {
        if (uint32 pid = CreatePIDFile(pidFile))
            TC_LOG_INFO("server.authserver", "Daemon PID: {}\n", pid);
        else
        {
            TC_LOG_ERROR("server.authserver", "Cannot create PID file {}.\n", pidFile);
            return 1;
        }
    }

    // 初始化数据库连接
    if (!StartDB()) // 认证服务用单线程即可
        return 1;

    std::shared_ptr<void> dbHandle(nullptr, [](void*) { StopDB(); });

    if (vm.count("update-databases-only"))
        return 0;

    sSecretMgr->Initialize();

    // Load IP Location Database
    // IP地址库
    sIPLocation->Load();

    std::shared_ptr<Trinity::Asio::IoContext> ioContext = std::make_shared<Trinity::Asio::IoContext>();

    // Get the list of realms for the server
    // 获取服务器上的所有区域列表
    sRealmList->Initialize(*ioContext, sConfigMgr->GetIntDefault("RealmsStateUpdateDelay", 20));

    std::shared_ptr<void> sRealmListHandle(nullptr, [](void*) { sRealmList->Close(); });

    if (sRealmList->GetRealms().empty())
    {
        TC_LOG_ERROR("server.authserver", "No valid realms specified.");
        return 1;
    }

    // 启动监听端口（接受器）以接受认证连接
    int32 port = sConfigMgr->GetIntDefault("RealmServerPort", 3724);
    if (port < 0 || port > 0xFFFF)
    {
        TC_LOG_ERROR("server.authserver", "Specified port out of allowed range (1-65535)");
        return 1;
    }
    
    std::string bindIp = sConfigMgr->GetStringDefault("BindIP", "0.0.0.0"); // 监听IP，默认监听所有IP

    if (!sAuthSocketMgr.StartNetwork(*ioContext, bindIp, port))
    {
        TC_LOG_ERROR("server.authserver", "Failed to initialize network");
        return 1;
    }

    std::shared_ptr<void> sAuthSocketMgrHandle(nullptr, [](void*) { sAuthSocketMgr.StopNetwork(); });

    // Set signal handlers
    boost::asio::signal_set signals(*ioContext, SIGINT, SIGTERM);
#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    signals.add(SIGBREAK);
#endif
    // 设置信号处理程序，以便在收到信号时优雅地关闭服务器
    signals.async_wait(std::bind(&SignalHandler, std::weak_ptr<Trinity::Asio::IoContext>(ioContext), std::placeholders::_1, std::placeholders::_2));

    // 根据配置文件设置进程优先级
    SetProcessPriority("server.authserver", sConfigMgr->GetIntDefault(CONFIG_PROCESSOR_AFFINITY, 0), sConfigMgr->GetBoolDefault(CONFIG_HIGH_PRIORITY, false));

    // 使用定时器定期发送数据库保持活动ping
    int32 dbPingInterval = sConfigMgr->GetIntDefault("MaxPingTime", 30);
    std::shared_ptr<Trinity::Asio::DeadlineTimer> dbPingTimer = std::make_shared<Trinity::Asio::DeadlineTimer>(*ioContext);
    dbPingTimer->expires_from_now(boost::posix_time::minutes(dbPingInterval));
    dbPingTimer->async_wait(std::bind(&KeepDatabaseAliveHandler, std::weak_ptr<Trinity::Asio::DeadlineTimer>(dbPingTimer), dbPingInterval, std::placeholders::_1));

    int32 banExpiryCheckInterval = sConfigMgr->GetIntDefault("BanExpiryCheckInterval", 60);
    std::shared_ptr<Trinity::Asio::DeadlineTimer> banExpiryCheckTimer = std::make_shared<Trinity::Asio::DeadlineTimer>(*ioContext);
    banExpiryCheckTimer->expires_from_now(boost::posix_time::seconds(banExpiryCheckInterval));
    banExpiryCheckTimer->async_wait(std::bind(&BanExpiryHandler, std::weak_ptr<Trinity::Asio::DeadlineTimer>(banExpiryCheckTimer), banExpiryCheckInterval, std::placeholders::_1));

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    std::shared_ptr<Trinity::Asio::DeadlineTimer> serviceStatusWatchTimer;
    if (m_ServiceStatus != -1)
    {
        serviceStatusWatchTimer = std::make_shared<Trinity::Asio::DeadlineTimer>(*ioContext);
        serviceStatusWatchTimer->expires_from_now(boost::posix_time::seconds(1));
        serviceStatusWatchTimer->async_wait(std::bind(&ServiceStatusWatcher,
            std::weak_ptr<Trinity::Asio::DeadlineTimer>(serviceStatusWatchTimer),
            std::weak_ptr<Trinity::Asio::IoContext>(ioContext),
            std::placeholders::_1));
    }
#endif

    ioContext->run(); // 运行io服务循环，等待异步操作完成

    banExpiryCheckTimer->cancel();
    dbPingTimer->cancel();

    TC_LOG_INFO("server.authserver", "Halting process...");

    signals.cancel();

    return 0;
}

// 启动数据库服务
bool StartDB()
{
    MySQL::Library_Init();
    /*
        // Load databases
        // NOTE: While authserver is singlethreaded you should keep synch_threads == 1.
        // Increasing it is just silly since only 1 will be used ever.
        authserver（认证服务）是单线程设计的，因此同步线程的数量应该保持在1。
        即便尝试增加同步线程数量，由于设计为单线程，所以只有1个线程会真正被使用。
        增加线程数不会带来性能提升，反而可能导致资源浪费。
    */
    // 创建一个DatabaseLoader实例，负责管理和加载数据库连接。
    DatabaseLoader loader("server.authserver", DatabaseLoader::DATABASE_NONE);
    loader.AddDatabase(LoginDatabase, "Login");

    if (!loader.Load())
        return false;

    TC_LOG_INFO("server.authserver", "Started auth database connection pool.");
    sLog->SetRealmId(0); // 设置RealmId以启用数据库附加器，用来记录数据库操作日志。
    return true;
}

/// Close the connection to the database
void StopDB()
{
    LoginDatabase.Close();
    MySQL::Library_End();
}

void SignalHandler(std::weak_ptr<Trinity::Asio::IoContext> ioContextRef, boost::system::error_code const& error, int /*signalNumber*/)
{
    if (!error)
        if (std::shared_ptr<Trinity::Asio::IoContext> ioContext = ioContextRef.lock())
            ioContext->stop();
}

void KeepDatabaseAliveHandler(std::weak_ptr<Trinity::Asio::DeadlineTimer> dbPingTimerRef, int32 dbPingInterval, boost::system::error_code const& error)
{
    if (!error)
    {
        if (std::shared_ptr<Trinity::Asio::DeadlineTimer> dbPingTimer = dbPingTimerRef.lock())
        {
            TC_LOG_INFO("server.authserver", "Ping MySQL to keep connection alive");
            LoginDatabase.KeepAlive();

            dbPingTimer->expires_from_now(boost::posix_time::minutes(dbPingInterval));
            dbPingTimer->async_wait(std::bind(&KeepDatabaseAliveHandler, dbPingTimerRef, dbPingInterval, std::placeholders::_1));
        }
    }
}

void BanExpiryHandler(std::weak_ptr<Trinity::Asio::DeadlineTimer> banExpiryCheckTimerRef, int32 banExpiryCheckInterval, boost::system::error_code const& error)
{
    if (!error)
    {
        if (std::shared_ptr<Trinity::Asio::DeadlineTimer> banExpiryCheckTimer = banExpiryCheckTimerRef.lock())
        {
            LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_DEL_EXPIRED_IP_BANS));
            LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_UPD_EXPIRED_ACCOUNT_BANS));

            banExpiryCheckTimer->expires_from_now(boost::posix_time::seconds(banExpiryCheckInterval));
            banExpiryCheckTimer->async_wait(std::bind(&BanExpiryHandler, banExpiryCheckTimerRef, banExpiryCheckInterval, std::placeholders::_1));
        }
    }
}

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
void ServiceStatusWatcher(std::weak_ptr<Trinity::Asio::DeadlineTimer> serviceStatusWatchTimerRef, std::weak_ptr<Trinity::Asio::IoContext> ioContextRef, boost::system::error_code const& error)
{
    if (!error)
    {
        if (std::shared_ptr<Trinity::Asio::IoContext> ioContext = ioContextRef.lock())
        {
            if (m_ServiceStatus == 0)
                ioContext->stop();
            else if (std::shared_ptr<Trinity::Asio::DeadlineTimer> serviceStatusWatchTimer = serviceStatusWatchTimerRef.lock())
            {
                serviceStatusWatchTimer->expires_from_now(boost::posix_time::seconds(1));
                serviceStatusWatchTimer->async_wait(std::bind(&ServiceStatusWatcher, serviceStatusWatchTimerRef, ioContextRef, std::placeholders::_1));
            }
        }
    }
}
#endif

variables_map GetConsoleArguments(int argc, char** argv, fs::path& configFile, fs::path& configDir, [[maybe_unused]] std::string& winServiceAction)
{
    options_description all("Allowed options");
    all.add_options()
        ("help,h", "print usage message")
        ("version,v", "print version build info")
        ("config,c", value<fs::path>(&configFile)->default_value(fs::absolute(_TRINITY_REALM_CONFIG)),
                     "use <arg> as configuration file")
        ("config-dir,cd", value<fs::path>(&configDir)->default_value(fs::absolute(_TRINITY_REALM_CONFIG_DIR)),
            "use <arg> as directory with additional config files")
        ("update-databases-only,u", "updates databases only")
        ;
#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    options_description win("Windows platform specific options");
    win.add_options()
        ("service,s", value<std::string>(&winServiceAction)->default_value(""), "Windows service options: [install | uninstall]")
        ;

    all.add(win);
#endif
    variables_map variablesMap;
    try
    {
        store(command_line_parser(argc, argv).options(all).allow_unregistered().run(), variablesMap);
        notify(variablesMap);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    if (variablesMap.count("help"))
        std::cout << all << "\n";
    else if (variablesMap.count("version"))
        std::cout << GitRevision::GetFullVersion() << "\n";

    return variablesMap;
}
