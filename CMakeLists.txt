cmake_minimum_required(VERSION 3.16)
project(ZeroClaw++ LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add all C++ sources
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# Define our library
add_library(zeroclaw_lib
    ${SRC_FILES}
    include/sqlite-3.51.2/sqlite3.c
)

# OpenSSL definitions (cpp-httplib uses CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(zeroclaw_lib PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
if(MSVC)
    target_compile_options(zeroclaw_lib PRIVATE /utf-8)
endif()

# Include directories
target_include_directories(zeroclaw_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/openssl-3.5.5/x64/include # or root include if organized that way
)

# Add OS link libraries for Windows + OpenSSL
if(WIN32)
    target_link_libraries(zeroclaw_lib PUBLIC
        ws2_32
        crypt32
        bcrypt
        secur32
        user32
        shell32
        advapi32
        ole32
        oleaut32
        ${CMAKE_SOURCE_DIR}/include/openssl-3.5.5/x64/lib/libssl.lib
        ${CMAKE_SOURCE_DIR}/include/openssl-3.5.5/x64/lib/libcrypto.lib
    )
endif()

# Define the executable
add_executable(zeroclaw
    src/main.cpp
)

# Link the executable with our library
target_link_libraries(zeroclaw PRIVATE zeroclaw_lib)

if(MSVC)
    target_compile_options(zeroclaw PRIVATE /utf-8)
endif()
