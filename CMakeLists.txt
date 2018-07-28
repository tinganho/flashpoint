cmake_minimum_required(VERSION 3.9)
project(FlashPoint)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include(ExternalProject)
include(cmake/modules/find_glib.cmake)
include(cmake/lib/utils.cmake)

set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
find_package(OpenSSL REQUIRED)
if (OpenSSL_FOUND)
    include_directories(${OpenSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION} ${OPENSSL_INCLUDE_DIRS}")
else()
    message(STATUS "Unable to find openssl")
endif()

# See: https://stackoverflow.com/questions/6468681/failed-linking-to-boost-library-with-cmake-generated-project-file-for-msvc9
add_definitions(-DBOOST_ALL_NO_LIB)
set(Boost_USE_STATIC_LIBS ON)

file(GLOB_RECURSE LIB_SRC ${CMAKE_SOURCE_DIR}/src/lib/*)
file(GLOB_RECURSE PROGRAM_SRC ${CMAKE_SOURCE_DIR}/src/program/*)
file(GLOB_RECURSE TEST_SRC ${CMAKE_SOURCE_DIR}/src/test/*)

set(Boost_DEBUG on)
find_package(Boost 1.66 COMPONENTS system filesystem regex REQUIRED)
find_package(CURL)

add_subdirectory(third_party/glob)
add_subdirectory(third_party/jsoncpp EXCLUDE_FROM_ALL)

include_directories(
    src
    src/lib
    third_party/glob
    third_party/diff_match_patch
    ${GLIB_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

add_executable(flash
    src/program.cpp
    ${LIB_SRC}
    ${PROGRAM_SRC})

add_executable(flash-test
    src/test.cpp
    ${TEST_SRC}
    ${PROGRAM_SRC}
    ${LIB_SRC})

add_executable(generate-diagnostics
    tasks/generate_diagnostics.cpp
    ${LIB_SRC})

target_link_libraries(flash jsoncpp_lib_static ${GLIB_LIBRARIES} ${Boost_LIBRARIES} glob libuv.a OpenSSL::SSL)
target_link_libraries(flash-test jsoncpp_lib_static ${GLIB_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES} glob libuv.a OpenSSL::SSL)
target_link_libraries(generate-diagnostics jsoncpp_lib_static ${Boost_LIBRARIES} glob)
