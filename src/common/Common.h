#ifndef TRINITYCORE_COMMON_H
#define TRINITYCORE_COMMON_H

#include "Define.h"
#include <array>
#include <string>

#define STRINGIZE(a) #a // 定义宏STRINGIZE，用于将参数a转换为字符串

enum TimeConstants // 定义了不同时间单位的常量
{
    MINUTE          = 60,
    HOUR            = MINUTE*60,
    DAY             = HOUR*24,
    WEEK            = DAY*7,
    MONTH           = DAY*30,
    YEAR            = MONTH*12,
    IN_MILLISECONDS = 1000 // 毫秒
};

// 数字越大代表权限越高
enum AccountTypes // 定义了不同类型的账户权限
{
    SEC_PLAYER         = 0, // 普通玩家
    SEC_MODERATOR      = 1, // 版主，能进行基本的管理操作。
    SEC_GAMEMASTER     = 2, // 游戏管理员，拥有更高的权限来管理游戏内容。GM可以踢人、封禁玩家等。
    SEC_ADMINISTRATOR  = 3, // 管理员，拥有最高的权限，可以管理服务器设置、玩家账户等。
    SEC_CONSOLE        = 4 // must be always last in list, accounts must have less security level always also
    // SEC_CONSOLE 是必须始终是最后一个的，账户必须总是具有较低的权限级别
};

enum LocaleConstant : uint8 // 定义了不同语言的常量
{
    LOCALE_enUS = 0, // 英语（美国）
    LOCALE_koKR = 1, // 韩语
    LOCALE_frFR = 2, // 法语（法国）
    LOCALE_deDE = 3, // 德语（德国）
    LOCALE_zhCN = 4, // 中文（中国）
    LOCALE_zhTW = 5, // 中文（台湾）
    LOCALE_esES = 6, // 西班牙语（西班牙）
    LOCALE_esMX = 7, // 西班牙语（墨西哥）
    LOCALE_ruRU = 8, // 俄语

    TOTAL_LOCALES // 必须始终是最后一个
};

#define DEFAULT_LOCALE LOCALE_enUS

#define MAX_LOCALES 8
#define MAX_ACCOUNT_TUTORIAL_VALUES 8

TC_COMMON_API extern char const* localeNames[TOTAL_LOCALES];

TC_COMMON_API LocaleConstant GetLocaleByName(std::string const& name); // 根据名称获取语言常量

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_4
#define M_PI_4 0.785398163397448309616
#endif

#define MAX_QUERY_LEN 32*1024

#endif
