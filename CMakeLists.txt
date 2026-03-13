cmake_minimum_required(VERSION 3.16)
project(PieTodo LANGUAGES CXX)

# MinGW: use static winpthread so no libwinpthread-1.dll is needed. Remove from
# implicit libs so we can link it statically explicitly; otherwise the linker may pull the .dll.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "pthread")
endif()

set(CMAKE_CXX_STANDARD 17)

# ImGui: must match Nexus loader version (1.80) so ImGuiContext ABI is compatible
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.80
)
FetchContent_MakeAvailable(imgui)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

set(IMGUI_SRC_DIR ${imgui_SOURCE_DIR})


add_library(${PROJECT_NAME} SHARED
  src/entry.cpp
  src/Shared.cpp
  src/PieTodo.def
  ${IMGUI_SRC_DIR}/imgui.cpp
  ${IMGUI_SRC_DIR}/imgui_draw.cpp
  ${IMGUI_SRC_DIR}/imgui_tables.cpp
  ${IMGUI_SRC_DIR}/imgui_widgets.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include/nexus
  ${IMGUI_SRC_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Smallest binary: size optimizations, section GC, strip, and LTO.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
      -Os
      -ffunction-sections
      -fdata-sections
    )
    target_link_options(${PROJECT_NAME} PRIVATE
      "LINKER:-s"
      "LINKER:--gc-sections"
    )
  endif()
  # LTO disabled: triggers ICE in GCC 15 with ImGui (binds_to_current_def_p in symtab.cc).
  # set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    PREFIX ""
    SUFFIX ".dll"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
  )
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WINDOWS
    UNICODE
    _UNICODE
  )
  set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/PieTodo.def")
  if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /DEF:${DEF_FILE})
  elseif(MINGW)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " ${DEF_FILE}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # Static-link winpthread into the DLL so no libwinpthread-1.dll is needed at load.
      if(CMAKE_CROSSCOMPILING AND CMAKE_FIND_ROOT_PATH)
        list(GET CMAKE_FIND_ROOT_PATH 0 _mingw_root)
        target_link_options(${PROJECT_NAME} PRIVATE
          "-static-libgcc" "-static-libstdc++"
          "-Wl,-Bstatic" "-Wl,--whole-archive" "${_mingw_root}/lib/libwinpthread.a" "-Wl,--no-whole-archive" "-Wl,-Bdynamic")
      else()
        target_link_options(${PROJECT_NAME} PRIVATE
          "-static-libgcc" "-static-libstdc++"
          "LINKER:-Bstatic" "LINKER:--whole-archive" "LINKER:-l:libwinpthread.a" "LINKER:--no-whole-archive" "LINKER:-Bdynamic")
      endif()
    endif()
  endif()
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/PieTodo"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/PieTodo/icon"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/icon/pie-todo-paper-icon.png"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/PieTodo/icon/pie-todo-paper-icon.png"
  COMMENT "Create PieTodo addon folder and copy Quick Access icon"
)
