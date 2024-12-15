/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information

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

#include "Locales.h"
#include <boost/locale/generator.hpp>

namespace
{
std::locale _global; // 全局区域设置
std::locale _calendar; // 日历区域设置
}
// 初始化和设置程序的区域设置，目的是用来处理国际化和本地化的问题，如字符编码、日期时间格式、数字格式等
void Trinity::Locale::Init()
{
    // 改变全局区域设置，从 "C" 改为 UTF-8，以供 C 运行时函数使用
    std::locale utf8("");
    _global = utf8;
    _global = std::locale(_global, std::locale::classic(), std::locale::numeric);
    std::locale::global(_global);

    std::setlocale(LC_ALL, "");
    std::setlocale(LC_NUMERIC, "C");

    boost::locale::generator g;
    _calendar = g.generate(utf8, "");
}

std::locale const& Trinity::Locale::GetGlobalLocale()
{
    return _global;
}

std::locale const& Trinity::Locale::GetCalendarLocale()
{
    return _calendar;
}
