cmake_minimum_required(VERSION 3.6)
set(CMAKE_CXX_STANDARD 23)
project(MHWildsHighQualityPhoto)

option(MHWILDS_PLUGIN_LOG_DEBUG "Log out crucial debug information" ON)

if (MHWILDS_PLUGIN_LOG_DEBUG)
    message(STATUS "Debug logging is enabled")
    add_definitions(-DLOG_DEBUG_STEP)
else()
    message(STATUS "Debug logging is disabled")
endif()

add_subdirectory(external)
add_subdirectory(source)