
#ifndef TRINITY_DEFINE_H
#define TRINITY_DEFINE_H

#include "CompilerDefs.h"

#if TRINITY_COMPILER == TRINITY_COMPILER_GNU
#  if !defined(__STDC_FORMAT_MACROS)
#    define __STDC_FORMAT_MACROS
#  endif
#  if !defined(__STDC_CONSTANT_MACROS)
#    define __STDC_CONSTANT_MACROS
#  endif
#  if !defined(_GLIBCXX_USE_NANOSLEEP)
#    define _GLIBCXX_USE_NANOSLEEP
#  endif
#  if defined(HELGRIND)
#    include <valgrind/helgrind.h>
#    undef _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE
#    undef _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER
#    define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A) ANNOTATE_HAPPENS_BEFORE(A)
#    define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A)  ANNOTATE_HAPPENS_AFTER(A)
#  endif
#endif

#include <cstddef>
#include <cinttypes>
#include <climits>

#define TRINITY_LITTLEENDIAN 0
#define TRINITY_BIGENDIAN    1

#if !defined(TRINITY_ENDIAN)
#  if defined (BOOST_BIG_ENDIAN)
#    define TRINITY_ENDIAN TRINITY_BIGENDIAN
#  else
#    define TRINITY_ENDIAN TRINITY_LITTLEENDIAN
#  endif
#endif

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
#  define TRINITY_PATH_MAX 260
#else // TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS
#  define TRINITY_PATH_MAX PATH_MAX
#endif // TRINITY_PLATFORM

#if !defined(COREDEBUG)
#  define TRINITY_INLINE inline
#else //COREDEBUG
#  if !defined(TRINITY_DEBUG)
#    define TRINITY_DEBUG
#  endif //TRINITY_DEBUG
#  define TRINITY_INLINE
#endif //!COREDEBUG

#if TRINITY_COMPILER == TRINITY_COMPILER_GNU
#  define ATTR_PRINTF(F, V) __attribute__ ((__format__ (__printf__, F, V)))
#else //TRINITY_COMPILER != TRINITY_COMPILER_GNU
#  define ATTR_PRINTF(F, V)
#endif //TRINITY_COMPILER == TRINITY_COMPILER_GNU

#ifdef TRINITY_API_USE_DYNAMIC_LINKING
#  if TRINITY_COMPILER == TRINITY_COMPILER_MICROSOFT
#    define TC_API_EXPORT __declspec(dllexport)
#    define TC_API_IMPORT __declspec(dllimport)
#  elif TRINITY_COMPILER == TRINITY_COMPILER_GNU
#    define TC_API_EXPORT __attribute__((visibility("default")))
#    define TC_API_IMPORT
#  else
#    error compiler not supported!
#  endif
#else
#  define TC_API_EXPORT
#  define TC_API_IMPORT
#endif

#ifdef TRINITY_API_EXPORT_COMMON
#  define TC_COMMON_API TC_API_EXPORT
#else
#  define TC_COMMON_API TC_API_IMPORT
#endif

#ifdef TRINITY_API_EXPORT_DATABASE
#  define TC_DATABASE_API TC_API_EXPORT
#else
#  define TC_DATABASE_API TC_API_IMPORT
#endif

#ifdef TRINITY_API_EXPORT_SHARED
#  define TC_SHARED_API TC_API_EXPORT
#else
#  define TC_SHARED_API TC_API_IMPORT
#endif

#ifdef TRINITY_API_EXPORT_GAME
#  define TC_GAME_API TC_API_EXPORT
#else
#  define TC_GAME_API TC_API_IMPORT
#endif

#define UI64FMTD "%" PRIu64
#define UI64LIT(N) UINT64_C(N)

#define SI64FMTD "%" PRId64
#define SI64LIT(N) INT64_C(N)

#define SZFMTD "%" PRIuPTR

#define STRING_VIEW_FMT "%.*s"
#define STRING_VIEW_FMT_ARG(str) static_cast<int>((str).length()), (str).data()

typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#endif //TRINITY_DEFINE_H