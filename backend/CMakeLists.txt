cmake_minimum_required(VERSION 3.16)
project(resource-monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
find_package(OpenSSL REQUIRED)

FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cpp-httplib json)

add_executable(sysmon
    src/main.cpp
    src/metrics.cpp
)

target_include_directories(sysmon PRIVATE src)
target_link_libraries(sysmon PRIVATE httplib::httplib nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto)

add_custom_command(TARGET sysmon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/static ${CMAKE_BINARY_DIR}/static
    COMMENT "Copying static web files to build directory"
)
