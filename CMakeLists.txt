cmake_minimum_required(VERSION 3.20)
project(BlobArena VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Общий код между клиентом и сервером
add_library(blob_common
    src/common/Protocol.cpp
)
target_include_directories(blob_common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
if(WIN32)
    target_link_libraries(blob_common PUBLIC ws2_32)
endif()

# Серверная логика
add_library(blob_server
    src/server/GameState.cpp
    src/server/Server.cpp
)
target_link_libraries(blob_server PUBLIC blob_common)
target_include_directories(blob_server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Клиентская логика (использует SFML)
find_package(SFML COMPONENTS System Window Graphics CONFIG REQUIRED)
add_library(blob_client
    src/client/NetworkClient.cpp
    src/client/Interpolator.cpp
    src/client/Renderer.cpp
    src/client/ClientGame.cpp
)
target_link_libraries(blob_client PUBLIC
    blob_common
    SFML::Graphics
    SFML::Window
    SFML::System
)
target_include_directories(blob_client PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Серверный exe
add_executable(blob_server_app apps/server_main.cpp)
target_link_libraries(blob_server_app PRIVATE blob_server)

# Клиентский exe
add_executable(blob_client_app apps/client_main.cpp)
target_link_libraries(blob_client_app PRIVATE blob_client)

# Тесты
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(blob_tests tests/test_common.cpp)
    target_link_libraries(blob_tests PRIVATE blob_server blob_client GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(blob_tests)
endif()

set_target_properties(blob_server_app blob_client_app blob_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)