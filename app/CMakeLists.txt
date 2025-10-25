cmake_minimum_required(VERSION 3.10)
project(selfmapsreader-native)

add_library(native-lib SHARED native-lib.c)

find_library( log-lib log )

# Try to find crypto. If OpenSSL is packaged as libcrypto, try to link it.
# You may need to point to OpenSSL include & lib paths if it's not provided by the NDK.
find_library(CRYPTO_LIB crypto)

if(CRYPTO_LIB)
    message(STATUS "Found crypto lib: ${CRYPTO_LIB}")
    target_link_libraries(native-lib ${CRYPTO_LIB} ${log-lib})
else()
    message(WARNING "crypto lib not found. SHA256 will fail unless you provide OpenSSL/BoringSSL or implement SHA256.")
    target_link_libraries(native-lib ${log-lib})
endif()

target_include_directories(native-lib PRIVATE ${CMAKE_SOURCE_DIR})
