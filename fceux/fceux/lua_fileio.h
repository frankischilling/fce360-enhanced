#pragma once

#ifdef USE_LUA

struct lua_State;

#ifdef __cplusplus
extern "C" {
#endif

// Registrar function - registers all file I/O-related Lua bindings
void Lua_RegisterFileIO(lua_State* L);

// Helper function for creating parent directories (used by other modules)
void Lua_FileIOCreateParentDirectories(const char* filepath);

// Lifecycle hooks (if needed)
void Lua_FileIOReset(void);

#ifdef __cplusplus
}
#endif

#endif // USE_LUA

