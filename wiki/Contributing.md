# Contributing to FCE360 Enhanced

Thank you for your interest in contributing to FCE360 Enhanced! This guide will help you understand the codebase structure and how to add or modify Lua API functions.

## Codebase Structure

The Lua API bindings are organized into modular C++ files, making it easier to locate, understand, and modify specific functionality.

### Core Files

**Main Integration:**
- `fceux/fceux/fceulua.cpp` – Main Lua integration, script loading, lifecycle management, and frame callbacks
- `fceux/fceux/lua_bindings.h` – Consolidated header that includes all module headers

**Shared Infrastructure:**
- `fceux/fceux/lua_helpers.h/.cpp` – Centralized helper utilities:
  - Argument validation (`LuaCheckInt`, `LuaCheckString`, `LuaCheckRange`, etc.)
  - Error reporting (`LuaArgError`, `LuaBoundsError`, etc.)
  - Data conversion (NES color ↔ Lua number, `std::vector` ↔ Lua table, path normalization)
- `fceux/fceux/lua_shared_state.h` – Shared state structures and constants:
  - `LuaInputState` namespace: Button callbacks, rumble state, virtual input mappings
  - `LuaProfilerState` namespace: Timing constants (NTSC frame rate, ideal frame time)

### API Module Files

Each API category is implemented in its own C++ file:

| API Category | C++ File | Header File |
|------------|----------|-------------|
| **Drawing Functions** | `lua_video.cpp` | `lua_video.h` |
| **Memory Functions** | `lua_memory.cpp` | `lua_memory.h` |
| **Audio Functions** | `lua_audio.cpp` | `lua_audio.h` |
| **File I/O Functions** | `lua_fileio.cpp` | `lua_fileio.h` |
| **Input Functions** | `lua_input.cpp` | `lua_input.h` |
| **Input Recording Functions** | `lua_movie.cpp` | `lua_movie.h` |
| **State Management Functions** | `lua_movie.cpp` | `lua_movie.h` |
| **Monitoring Functions** | `lua_profiler.cpp`, `lua_emulator.cpp` | `lua_profiler.h`, `lua_emulator.h` |
| **ROM Information Functions** | `lua_rom.cpp` | `lua_rom.h` |
| **Game Genie Functions** | `lua_gamegenie.cpp` | `lua_gamegenie.h` |
| **Color/Palette Functions** | `lua_palette.cpp` | `lua_palette.h` |
| **Utility Functions** | `lua_runtime.cpp` | `lua_runtime.h` |

## Module Structure

Each module follows a consistent pattern:

### 1. Table-Driven Registration

Functions are registered using a `static const luaL_Reg` array:

```cpp
static const luaL_Reg kVideoFuncs[] = {
    {"drawtext", lua_drawtext},
    {"fillrect", lua_fillrect},
    // ... more functions
    {NULL, NULL}  // Terminator
};

void Lua_RegisterVideo(lua_State* L) {
    // Manual iteration and registration
    for (const luaL_Reg* reg = kVideoFuncs; reg->name; ++reg) {
        lua_register(L, reg->name, reg->func);
    }
}
```

### 2. Helper Function Usage

All modules use centralized helpers from `lua_helpers.h`:

```cpp
// Instead of manual validation:
int value = luaL_checkinteger(L, 1);

// Use helpers:
int value = LuaCheckInt(L, 1, "functionname", "paramname");
int color = LuaCheckNESColor(L, 2, "functionname", "color");
```

### 3. Shared State

When modules need shared structures, use `lua_shared_state.h`:

```cpp
#include "lua_shared_state.h"

// Use namespace-qualified types:
static LuaInputState::RumbleState s_rumbleState[4];
static LuaInputState::ButtonCallbackInfo callback;
```

## Adding a New API Function

### Step 1: Choose the Correct Module

Determine which module your function belongs to based on its functionality:

- **Drawing/rendering** → `lua_video.cpp`
- **Memory operations** → `lua_memory.cpp`
- **Audio processing** → `lua_audio.cpp`
- **File operations** → `lua_fileio.cpp`
- **Input handling** → `lua_input.cpp`
- **State/save management** → `lua_movie.cpp`
- **Performance/timing** → `lua_profiler.cpp` or `lua_emulator.cpp`
- **ROM information** → `lua_rom.cpp`
- **Color/palette** → `lua_palette.cpp`
- **Game Genie** → `lua_gamegenie.cpp`
- **Runtime utilities** → `lua_runtime.cpp`

### Step 2: Implement the Function

1. **Add the function implementation** in the appropriate `.cpp` file:

```cpp
static int lua_mynewfunction(lua_State* L) {
    int n = lua_gettop(L);
    if (n < 2) {
        return LuaArgCountError(L, "mynewfunction", 2, 2, n);
    }
    
    // Use helpers for validation
    int param1 = LuaCheckInt(L, 1, "mynewfunction", "param1");
    const char* param2 = LuaCheckString(L, 2, "mynewfunction", "param2");
    
    // Implementation...
    
    lua_pushinteger(L, result);
    return 1;
}
```

2. **Add to the registration table**:

```cpp
static const luaL_Reg kMyModuleFuncs[] = {
    {"existingfunction", lua_existingfunction},
    {"mynewfunction", lua_mynewfunction},  // Add here
    {NULL, NULL}
};
```

3. **Register the function** (already handled by `Lua_Register<Module>()` if using table-driven registration)

### Step 3: Add Documentation

1. **Update the wiki page** for the appropriate API category (e.g., `wiki/Drawing-Functions.md`)
2. **Add function signature** with anchor link:
   ```markdown
   ### `mynewfunction(param1, param2)`
   ```
3. **Document parameters, return values, examples, and use cases**

### Step 4: Update Module Header (if needed)

If the function needs to be accessible from other C++ files, add a declaration to the module's `.h` file:

```cpp
// In lua_mymodule.h
void Lua_MyModuleSomeHelper(lua_State* L);  // If needed by other modules
```

## Best Practices

### 1. Always Use Helper Functions

**✅ Good:**
```cpp
int value = LuaCheckRange(L, 1, 0, 255, "setcolor", "value");
const char* path = LuaCheckPath(L, 2, "loadfile", "path");
```

**❌ Avoid:**
```cpp
int value = luaL_checkinteger(L, 1);
if (value < 0 || value > 255) {
    return luaL_error(L, "value out of range");
}
```

### 2. Use Table-Driven Registration

**✅ Good:**
```cpp
static const luaL_Reg kMyModuleFuncs[] = {
    {"func1", lua_func1},
    {"func2", lua_func2},
    {NULL, NULL}
};
```

**❌ Avoid:**
```cpp
lua_register(L, "func1", lua_func1);
lua_register(L, "func2", lua_func2);
// ... scattered throughout the file
```

### 3. Use Shared State for Common Structures

**✅ Good:**
```cpp
#include "lua_shared_state.h"
static LuaInputState::RumbleState s_rumbleState[4];
```

**❌ Avoid:**
```cpp
struct RumbleState { ... };  // Duplicate definition
```

### 4. Consistent Error Messages

Use helper functions for consistent error formatting:

```cpp
return LuaArgCountError(L, "functionname", minArgs, maxArgs, actualArgs);
return LuaArgTypeError(L, "functionname", argNum, expectedType);
return LuaBoundsError(L, "functionname", paramName, value, min, max);
```

### 5. Validate All Inputs

Always validate:
- Argument count
- Argument types
- Value ranges (use `LuaCheckRange`, `LuaCheckNESColor`, etc.)
- String parameters (use `LuaCheckString`, `LuaCheckPath`)

## Testing

1. **Create a test script** in `test-lua/` directory:
   ```lua
   -- test_mynewfunction.lua
   function script()
       local result = mynewfunction(42, "test")
       print("Result: " .. result)
   end
   ```

2. **Test edge cases:**
   - Invalid argument counts
   - Out-of-range values
   - Nil parameters
   - Empty strings (where applicable)

3. **Verify error messages** are clear and helpful

## Code Review Checklist

When submitting changes, ensure:

- [ ] Function is in the correct module file
- [ ] Uses helper functions for validation
- [ ] Added to registration table
- [ ] Error messages are clear and consistent
- [ ] Documentation updated in wiki
- [ ] Test script created (if applicable)
- [ ] No duplicate code (use helpers/shared state)
- [ ] Follows existing code style and patterns

## Getting Help

- Review existing modules for examples
- Check `lua_helpers.h` for available helper functions
- See [Technical Details](Technical-Details) for implementation specifics
- Check [Troubleshooting](Troubleshooting) for common issues

## See Also

- [Home](Home) – Main API documentation index
- [Technical Details](Technical-Details) – Implementation details and codebase structure
- [Setup](Setup) – Development environment setup

