cmake_minimum_required(VERSION 3.20)
project(PieTodo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release build for optimized, stripped output
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

# Cross-compilation setup for Windows on Linux
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Static linking to avoid Windows DLL dependencies
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -DWIN32_LEAN_AND_MEAN")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui)
include_directories(${CMAKE_SOURCE_DIR}/src)

# ImGui source files
set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
)

# Source files
set(SOURCES
    src/dllmain.cpp
    src/TodoManager.cpp
    src/Shared.cpp
    ${IMGUI_SOURCES}
)

# Header files
set(HEADERS
    include/nexus/Nexus.h
    src/TodoManager.h
    src/Shared.h
)

# Create shared library (DLL)
add_library(PieTodo SHARED ${SOURCES} ${HEADERS})

# Force Windows DLL extension
set_target_properties(PieTodo PROPERTIES
    SUFFIX ".dll"
    PREFIX ""
)

# Module definition file for clean DLL export
target_link_options(PieTodo PRIVATE -Wl,--out-implib,PieTodo.lib -Wl,--strip-all)
target_sources(PieTodo PRIVATE ${CMAKE_SOURCE_DIR}/PieTodo.def)

# Output directory
set_target_properties(PieTodo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Post-build: copy icon into addon folder
add_custom_command(TARGET PieTodo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PieTodo>/PieTodo"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PieTodo>/PieTodo/icon"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icon/pie-todo-paper-icon.png"
        "$<TARGET_FILE_DIR:PieTodo>/PieTodo/icon/pie-todo-paper-icon.png"
    COMMENT "Create PieTodo addon folder and copy Quick Access icon"
)
