#
# Copyright 2017 Segment Inc. <friends@segment.com>
#
# This software is supplied under the terms of the MIT License, a
# copy of which should be located in the distribution where this
# file was obtained (LICENSE.txt).  A copy of the license may also be
# found online at https://opensource.org/licenses/MIT.
#

cmake_minimum_required (VERSION 3.1)
project(analytics-cpp)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(NO_DEFAULT_HTTP "Disable builtin HTTP transport (stub only)." OFF)
option(COVERALLS "Generate coveralls data" OFF)

# We require C++ 11.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIED on)
set(CMAKE_CXX_EXTENSIONS off)
find_package(Threads REQUIRED)
enable_testing()

if (COVERALLS)
    message(STATUS "Enabling coveralls.")
    include(Coveralls)
    coveralls_turn_on_coverage()
endif()

if (NO_DEFAULT_HTTP)
    set(HTTP_SOURCES http-none.hpp)

elseif (CMAKE_SYSTEM_NAME MATCHES Windows)
    add_definitions(-DSEGMENT_USE_WININET)
    set(HTTP_SOURCES http-wininet.hpp http-wininet.cpp)
    set(HTTP_LIBRARY wininet)

else()
    find_package(CURL REQUIRED)
    add_definitions(-DSEGMENT_USE_CURL)
    set(HTTP_SOURCES http-curl.hpp http-curl.cpp)
    set(HTTP_LIBRARY ${CURL_LIBRARIES})
endif()

set(SOURCES analytics.cpp analytics.hpp
    date.hpp json.hpp http.hpp
    ${HTTP_SOURCES})

# Coverage only for our own code, not the date or json library we use.
set (COVERALLS_SRCS analytics.cpp analytics.hpp http.hpp ${HTTP_SOURCES})

if (COVERALLS)
    coveralls_setup("${COVERALLS_SRCS}" ON)
endif()

# Dynamic library
add_library(${PROJECT_NAME} ${SOURCES} ${HTTP_SOURCES})
target_link_libraries(${PROJECT_NAME} ${HTTP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

# Static library
add_library(${PROJECT_NAME}_static STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME}_static ${HTTP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

include_directories(AFTER SYSTEM ${CURL_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})

add_subdirectory(tests)
