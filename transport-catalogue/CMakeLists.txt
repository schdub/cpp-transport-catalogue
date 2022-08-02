cmake_minimum_required(VERSION 2.4)

project(sprint10)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17")
set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(sprint10
    main.cpp
    input_reader.cpp
    stat_reader.cpp
    transport_catalogue.cpp
    geo.cpp
    domain.cpp
    map_renderer.cpp
    request_handler.cpp
    svg.cpp
    json.cpp
)
