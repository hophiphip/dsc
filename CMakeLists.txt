cmake_minimum_required(VERSION 3.10)

project(dsc VERSION 0.1)

include_directories(include)

if(WIN32)
    # for Windows operating system in general
    message(FATAL_ERROR "WIN32 not supported" )

elseif(MSVC OR MSYS OR MINGW)
    # for detecting Windows compilers
    message(FATAL_ERROR "WIN32 not supported" )

elseif(APPLE)
    # for MacOS X or iOS, watchOS, tvOS (since 3.10.3)
    message(FATAL_ERROR "APPLE not supported" )

elseif(UNIX AND NOT APPLE)
    # for Linux, BSD, Solaris, Minix
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb -lX11 -lXext -pedantic -Wall")
endif()

add_executable(dsc
    # Networking library
    include/mongoose/mongoose.c
    # Screen image manager for x11
    include/img/x11lib.c
    main.c
)
