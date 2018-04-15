cmake_minimum_required(VERSION 3.9)
project(Flash)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include(cmake/modules/find_glib.cmake)
include(cmake/lib/utils.cmake)


# See: https://stackoverflow.com/questions/6468681/failed-linking-to-boost-library-with-cmake-generated-project-file-for-msvc9
add_definitions(-DBOOST_ALL_NO_LIB)
set(Boost_USE_STATIC_LIBS ON)

file(GLOB LIB_SRC ${CMAKE_SOURCE_DIR}/src/lib/*)

find_package(Boost 1.66 COMPONENTS system filesystem regex REQUIRED)

include_directories(
    src
    src/lib
    ${GLIB_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
)

add_executable(flash
    main.cpp
    ${LIB_SRC})

target_link_libraries(flash ${GLIB_LIBRARIES} libuv.a )
