cmake_minimum_required(VERSION 4.3)

project(elmo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER
    ${CMAKE_COMMAND} -E env
    CCACHE_SLOPPINESS=pch_defines,time_macros
    "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
    "${CCACHE_PROGRAM}")
endif()

find_package(Drogon CONFIG REQUIRED)

add_executable(
  elmo
  src/elmo.cc
  src/elmo/core/conf.cpp
  src/elmo/core/db.cpp
  src/elmo/handling/auth/middlewares.cpp
  src/elmo/handling/auth/pbkdf2.cpp
  src/elmo/handling/auth/routes.cpp
  src/elmo/handling/common.cpp
  src/elmo/handling/home/routes.cpp
  src/elmo/handling/index/routes.cpp
  src/elmo/handling/party/routes.cpp
  src/elmo/handling/productionorder/routes.cpp
  src/elmo/handling/rentorder/routes.cpp)

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/auth/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/home/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/order/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/party/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/productionorder/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/rentorder/template
  ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(elmo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  elmo SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(elmo PRIVATE Drogon::Drogon sqlite3)
