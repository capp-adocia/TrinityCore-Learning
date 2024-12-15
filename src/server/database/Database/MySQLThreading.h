#ifndef _MYSQLTHREADING_H
#define _MYSQLTHREADING_H

#include "Define.h"

namespace MySQL
{
    TC_DATABASE_API void Library_Init(); // 初始化MySQL库
    TC_DATABASE_API void Library_End(); // 结束MySQL库
    TC_DATABASE_API uint32 GetLibraryVersion(); // 获取MySQL库的版本
}

#endif
