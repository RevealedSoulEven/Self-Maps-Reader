cmake_minimum_required(VERSION 3.10.2)
project("selfmapsreader")

# Set the native library name
add_library(
    native-lib
    SHARED
    src/main/cpp/native-lib.c
)

# Include Android native log if needed
find_library(log-lib log)

# Link against the log library (optional)
target_link_libraries(
    native-lib
    ${log-lib}
)
