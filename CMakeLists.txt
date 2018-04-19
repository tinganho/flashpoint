cmake_minimum_required(VERSION 3.9)
project(FlashPoint)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include(ExternalProject)
include(cmake/modules/find_glib.cmake)
include(cmake/lib/utils.cmake)


# See: https://stackoverflow.com/questions/6468681/failed-linking-to-boost-library-with-cmake-generated-project-file-for-msvc9
add_definitions(-DBOOST_ALL_NO_LIB)
set(Boost_USE_STATIC_LIBS ON)

file(GLOB LIB_SRC ${CMAKE_SOURCE_DIR}/src/lib/*)
file(GLOB PROGRAM_SRC ${CMAKE_SOURCE_DIR}/src/program/*)
file(GLOB TEST_SRC ${CMAKE_SOURCE_DIR}/src/test/*)

set(Boost_DEBUG on)
find_package(Boost 1.66 COMPONENTS system filesystem regex REQUIRED)

add_subdirectory(third_party/glob)
add_subdirectory(third_party/jsoncpp EXCLUDE_FROM_ALL)

include_directories(
    src
    src/lib
    third_party/glob
    ${GLIB_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
    ${JSONCPP_INCLUDE_DIRS}
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

target_link_libraries(flash jsoncpp ${GLIB_LIBRARIES} ${Boost_LIBRARIES} glob libuv.a)
target_link_libraries(flash-test jsoncpp ${GLIB_LIBRARIES} ${Boost_LIBRARIES} glob libuv.a)
