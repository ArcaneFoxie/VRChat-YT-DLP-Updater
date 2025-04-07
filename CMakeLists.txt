cmake_minimum_required(VERSION 3.15)
project(yt_dlp_updater VERSION 1.0)

# Set CMake policies
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force static linking for everything
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# CURL static settings
set(CURL_STATIC_CRT ON CACHE BOOL "" FORCE)
set(CURL_STATIC ON CACHE BOOL "" FORCE)
set(CURL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CURL_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
set(CURL_USE_ZLIB ON CACHE BOOL "" FORCE)

# OpenSSL static settings
set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(OPENSSL_MSVC_STATIC_RT ON CACHE BOOL "" FORCE)

# ZLIB static settings
set(ZLIB_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

# Find required packages
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

# Add executable
add_executable(yt_dlp_updater src/main.cpp)

# Link libraries
target_link_libraries(yt_dlp_updater PRIVATE 
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

# Set static runtime for MSVC
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif() 