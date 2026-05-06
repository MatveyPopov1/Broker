cmake_minimum_required(VERSION 3.10)
project(broker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(broker
    main.cpp
    index/index.cpp
    storage/storage.cpp
    server/server.cpp
)

target_include_directories(broker PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(broker PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)

add_subdirectory(tests)
