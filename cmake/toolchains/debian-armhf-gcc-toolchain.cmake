set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armhf)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_FIND_ROOT_PATH  /usr/arm-linux-gnueabihf/)
list(APPEND CMAKE_FIND_ROOT_PATH /usr/lib/arm-linux-gnueabihf/)
set(CMAKE_CROSSCOMPILING ON)