cmake_minimum_required(VERSION 3.20)
project(sfxbrowser)

set(BUILD_SHARED_LIBS OFF)

#justwindowsthings
if (WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Project files
file(GLOB srcfiles ./src/*.*)
list(APPEND includes PUBLIC ./src/)

# Onut
option(ONUT_USE_SDL "" OFF)
option(ASSIMP_BUILD_ASSIMP_TOOLS "" OFF)
option(ASSIMP_BUILD_TESTS "" OFF)
add_subdirectory(thirdparty/onut/)
list(APPEND libs PUBLIC libonut)
list(APPEND includes PUBLIC thirdparty/onut/include/)

# Source groups for Visual Studio
source_group("src" FILES ${srcfiles})

# .exe
add_executable(${PROJECT_NAME} WIN32 ${srcfiles})

# Work dir
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/")

# Lib/Headers
target_include_directories(${PROJECT_NAME} PUBLIC ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC ${libs})
