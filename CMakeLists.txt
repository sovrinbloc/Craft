cmake_minimum_required(VERSION 2.8)

project(craft)

add_executable(craft main.c db.c map.c noise.c util.c deps/sqlite/sqlite3.c)

add_definitions(-std=c99 -O3)

#include_directories(/usr/local/include)
include_directories(deps/GLFW)
add_subdirectory(deps/glfw)
include_directories(deps/glfw/include)
include_directories(deps/sqlite)
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

target_link_libraries(craft glfw ${GLFW_LIBRARIES} ${PNG_LIBRARY})
