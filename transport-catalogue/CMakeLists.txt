cmake_minimum_required(VERSION 2.4)

set(PROJECT_NAME "transport_catalogue")

project(${PROJECT_NAME})

set(TRANSPORT_DB_FILES
    main.cpp
    transport_catalogue.cpp
    geo.cpp
    domain.cpp
    map_renderer.cpp
    request_handler.cpp
    svg.cpp
    json.cpp
    json_reader.cpp
    json_builder.cpp
    transport_router.cpp
    serialization.cpp
    transport_catalogue.proto
    transport_catalogue.h
    geo.h
    graph.h
    domain.h
    map_renderer.h
    request_handler.h
    svg.h
    json.h
    json_reader.h
    json_builder.h
    transport_router.h
    serialization.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17")
set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS transport_catalogue.proto)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${TRANSPORT_DB_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

string(REPLACE "protobuf.lib" "protobufd.lib" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")
string(REPLACE "protobuf.a" "protobufd.a" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")

target_link_libraries(${PROJECT_NAME} "$<IF:$<CONFIG:Debug>,${Protobuf_LIBRARY_DEBUG},${Protobuf_LIBRARY}>" Threads::Threads)
