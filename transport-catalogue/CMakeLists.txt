cmake_minimum_required(VERSION 2.4)

project(sprint9)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17")
set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(sprint9
    main.cpp
    input_reader.cpp
    stat_reader.cpp
    transport_catalogue.cpp
)
