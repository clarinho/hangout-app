cmake_minimum_required(VERSION 3.21)
project(hangout_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_executable(hangout_backend
    src/main.cpp
    src/application/auth_service.cpp
    src/application/chat_service.cpp
    src/application/social_service.cpp
    src/storage/database.cpp
    src/storage/repositories.cpp
    src/transport/http_server.cpp
)

target_include_directories(hangout_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(hangout_backend PRIVATE
    Threads::Threads
    PkgConfig::SQLITE3
)

if(WIN32)
    target_link_libraries(hangout_backend PRIVATE ws2_32)
endif()

target_compile_definitions(hangout_backend PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
