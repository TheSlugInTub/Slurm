# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.16)

# Set the project name
project(Slurm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_C_STANDARD 17)

include_directories(
    include/
)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)
file(GLOB_RECURSE INCLUDESOURCES 
    include/*.cpp 
    include/*.c
)

if (MSVC)
    add_compile_options(
        /Zc:__cplusplus
        /Zc:preprocessor
    )
endif()

if(WIN32)
    # For Windows, we'll use PDCurses (which is ncurses-compatible)
    include(D:/Apps/vcpkg/scripts/buildsystems/vcpkg.cmake)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PDCURSES QUIET pdcurses)
    endif()
    
    if(NOT PDCURSES_FOUND)
        # Try to find PDCurses manually
        find_path(PDCURSES_INCLUDE_DIR 
            NAMES curses.h ncurses.h pdcurses.h
            PATHS 
                ${CMAKE_SOURCE_DIR}/third_party/pdcurses
                ${CMAKE_SOURCE_DIR}/deps/pdcurses
                $ENV{PDCURSES_ROOT}/include
                $ENV{PDCURSES_ROOT}
                C:/pdcurses
                C:/vcpkg/installed/x64-windows/include
                C:/vcpkg/installed/x86-windows/include
        )
        
        find_library(PDCURSES_LIBRARY
            NAMES pdcurses libpdcurses curses libcurses
            PATHS
                ${CMAKE_SOURCE_DIR}/third_party/pdcurses
                ${CMAKE_SOURCE_DIR}/deps/pdcurses
                $ENV{PDCURSES_ROOT}/lib
                $ENV{PDCURSES_ROOT}
                C:/pdcurses
                C:/vcpkg/installed/x64-windows/lib
                C:/vcpkg/installed/x86-windows/lib
        )
        
        if(PDCURSES_INCLUDE_DIR AND PDCURSES_LIBRARY)
            set(CURSES_FOUND TRUE)
            set(CURSES_INCLUDE_DIRS ${PDCURSES_INCLUDE_DIR})
            set(CURSES_LIBRARIES ${PDCURSES_LIBRARY})
            message(STATUS "Found PDCurses: ${PDCURSES_LIBRARY}")
        else()
            message(WARNING "PDCurses not found. Please install PDCurses or use vcpkg: vcpkg install pdcurses")
        endif()
    else()
        set(CURSES_FOUND TRUE)
        set(CURSES_INCLUDE_DIRS ${PDCURSES_INCLUDE_DIRS})
        set(CURSES_LIBRARIES ${PDCURSES_LIBRARIES})
    endif()
else()
    # For UNIX systems (Linux, macOS, etc.)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NCURSES QUIET ncurses)
    endif()
    
    if(NOT NCURSES_FOUND)
        # Fallback to FindCurses module
        find_package(Curses REQUIRED)
        if(CURSES_FOUND)
            set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
            set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
        endif()
    else()
        set(CURSES_FOUND TRUE)
        set(CURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIRS})
        set(CURSES_LIBRARIES ${NCURSES_LIBRARIES})
    endif()
endif()

# Check if we found curses/ncurses
if(NOT CURSES_FOUND)
    message(FATAL_ERROR "ncurses/curses library not found!")
endif()

# Add the executable
add_executable(${PROJECT_NAME} 
    ${CMAKE_SOURCE_DIR}/src/console.cpp
    ${CMAKE_SOURCE_DIR}/src/pseudoconsole.cpp
    ${INCLUDESOURCES}
)

# Link ncurses/pdcurses
target_include_directories(${PROJECT_NAME} PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${CURSES_LIBRARIES}
    ntdll.lib)

# Add compile definitions for easier cross-platform usage
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PDCURSES_WINCON)
endif()

# Print information about what was found
message(STATUS "Curses include directories: ${CURSES_INCLUDE_DIRS}")
message(STATUS "Curses libraries: ${CURSES_LIBRARIES}")
