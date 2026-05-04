cmake_minimum_required(VERSION 3.16)
project(broker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем корень проекта в пути поиска заголовков
include_directories(${CMAKE_SOURCE_DIR})

# Все исходники
set(SOURCES
    main.cpp
    server/server.cpp
    storage/storage.cpp
    storage/index.cpp
)

add_executable(broker ${SOURCES})
target_link_libraries(broker pthread)
