#ifndef _SHION_H
#define _SHION_H

#ifdef _WIN32
#  ifdef SHION_BUILD
#    define SHION_EXPORT __declspec(dllexport)
#  else
#    define SHION_EXPORT __declspec(dllimport)
#  endif
#endif

#ifndef SHION_BUILD
#  define SHION_INCLUDE
#endif

#include "types.h"

#endif
