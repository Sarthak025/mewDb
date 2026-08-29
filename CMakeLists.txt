cmake_minimum_required(VERSION 3.15)

project(mewDb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mewDb main.cpp src/db_engine.cpp src/wal.cpp src/ss_table.cpp)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_include_directories(mewDb PRIVATE include)
target_link_libraries(mewDb z)