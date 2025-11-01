// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define NOMINMAX  // Prevent Windows min/max macros from conflicting with std::min/max

#include <xtl.h>
#include <xboxmath.h>

// Enable Lua support
#ifndef USE_LUA
#define USE_LUA 1
#endif
