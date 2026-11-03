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

option(ELMO_TEST "Build unit tests" OFF)
option(ELMO_COVERAGE "Clang source-based coverage for unit tests" OFF)

if(ELMO_COVERAGE AND NOT ELMO_TEST)
  message(FATAL_ERROR "ELMO_COVERAGE requires ELMO_TEST=ON")
endif()

add_library(
  elmo_core OBJECT
  src/elmo/core/conf.cpp
  src/elmo/core/db.cpp
  src/elmo/handling/auth/middlewares.cpp
  src/elmo/handling/auth/pbkdf2.cpp
  src/elmo/handling/auth/routes.cpp
  src/elmo/handling/common.cpp
  src/elmo/handling/form/routes.cpp
  src/elmo/handling/help/routes.cpp
  src/elmo/handling/home/routes.cpp
  src/elmo/handling/index/routes.cpp
  src/elmo/handling/party/routes.cpp
  src/elmo/handling/productionorder/routes.cpp
  src/elmo/handling/rentorder/routes.cpp)

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/auth/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/form/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/help/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/home/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/order/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/party/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/productionorder/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(elmo_core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elmo/handling/rentorder/template
  ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(elmo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  elmo_core SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(elmo_core PUBLIC Drogon::Drogon sqlite3)

add_executable(elmo src/elmo.cc)
target_link_libraries(elmo PRIVATE elmo_core)

if(ELMO_TEST)
  find_package(GTest REQUIRED)

  enable_testing()

  add_executable(
    elmo-test
    src/elmo/core/conf-test.cc
    src/elmo/core/db-test.cc
    src/elmo/handling/auth/middlewares-test.cc
    src/elmo/handling/auth/utils-test.cc
    src/elmo/handling/common-test.cc
    src/elmo/handling/help/routes-test.cc
    src/elmo/testing/db.cpp)

  target_compile_definitions(
    elmo-test PRIVATE ELMO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(elmo-test PRIVATE elmo_core GTest::gmock
    GTest::gmock_main)

  include(GoogleTest)
  gtest_discover_tests(elmo-test)
endif()

if(ELMO_COVERAGE)
  set(ELMO_COVERAGE_FLAGS -fprofile-instr-generate -fcoverage-mapping)
  target_compile_options(elmo_core PRIVATE ${ELMO_COVERAGE_FLAGS})
  target_compile_options(elmo-test PRIVATE ${ELMO_COVERAGE_FLAGS})
  target_link_options(elmo-test PRIVATE ${ELMO_COVERAGE_FLAGS})

  find_program(XCRUN xcrun)
  foreach(tool llvm-profdata llvm-cov)
    string(TOUPPER ${tool} var)
    string(REPLACE "-" "_" var ${var})
    if(XCRUN)
      execute_process(COMMAND ${XCRUN} -f ${tool}
        OUTPUT_VARIABLE ${var} OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    endif()
    if(NOT ${var})
      find_program(${var} ${tool} REQUIRED)
    endif()
  endforeach()

  set(ELMO_COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
  set(ELMO_COVERAGE_IGNORE
    "(${CMAKE_BINARY_DIR}|-test\\.cc$|/testing/|/3rdparty/|^/opt/|^/usr/)")
  add_custom_target(
    coverage
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ELMO_COVERAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ELMO_COVERAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E env
      LLVM_PROFILE_FILE=${ELMO_COVERAGE_DIR}/elmo.profraw
      $<TARGET_FILE:elmo-test> --gtest_brief=1
    COMMAND ${LLVM_PROFDATA} merge -sparse -o ${ELMO_COVERAGE_DIR}/elmo.profdata
      ${ELMO_COVERAGE_DIR}/elmo.profraw
    COMMAND ${LLVM_COV} report $<TARGET_FILE:elmo-test>
      -instr-profile=${ELMO_COVERAGE_DIR}/elmo.profdata
      -ignore-filename-regex=${ELMO_COVERAGE_IGNORE}
    COMMAND ${LLVM_COV} show $<TARGET_FILE:elmo-test>
      -instr-profile=${ELMO_COVERAGE_DIR}/elmo.profdata
      -ignore-filename-regex=${ELMO_COVERAGE_IGNORE}
      -format=html -output-dir=${ELMO_COVERAGE_DIR}/html
    COMMAND sh -c "${LLVM_COV} export $<TARGET_FILE:elmo-test> -instr-profile=${ELMO_COVERAGE_DIR}/elmo.profdata '-ignore-filename-regex=${ELMO_COVERAGE_IGNORE}' -format=lcov > ${ELMO_COVERAGE_DIR}/coverage.lcov"
    DEPENDS elmo-test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM)
endif()
