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
        /utf-8  # Enable UTF-8 support in MSVC
    )
endif()

if(WIN32)
    # For Windows, we'll use PDCurses with wide character support
    include(D:/Apps/vcpkg/scripts/buildsystems/vcpkg.cmake)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PDCURSES QUIET pdcurses)
    endif()
    
    if(NOT PDCURSES_FOUND)
        # Try to find PDCurses manually - look for wide character version first
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
        
        # Look for wide character libraries first
        find_library(PDCURSES_LIBRARY
            NAMES pdcursesw libpdcursesw pdcurses libpdcurses cursesw libcursesw curses libcurses
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
            message(WARNING "PDCurses not found. For Unicode support, install with: vcpkg install pdcurses[wide-character]")
        endif()
    else()
        set(CURSES_FOUND TRUE)
        set(CURSES_INCLUDE_DIRS ${PDCURSES_INCLUDE_DIRS})
        set(CURSES_LIBRARIES ${PDCURSES_LIBRARIES})
    endif()
else()
    # For UNIX systems (Linux, macOS, etc.) - prefer ncursesw
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # Try ncursesw first, then fallback to ncurses
        pkg_check_modules(NCURSESW QUIET ncursesw)
        if(NOT NCURSESW_FOUND)
            pkg_check_modules(NCURSES QUIET ncurses)
        endif()
    endif()
    
    if(NCURSESW_FOUND)
        set(CURSES_FOUND TRUE)
        set(CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
        set(CURSES_LIBRARIES ${NCURSESW_LIBRARIES})
        message(STATUS "Found ncursesw (wide character support)")
    elif(NCURSES_FOUND)
        set(CURSES_FOUND TRUE)
        set(CURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIRS})
        set(CURSES_LIBRARIES ${NCURSES_LIBRARIES})
        message(STATUS "Found ncurses (basic support)")
    else()
        # Fallback to FindCurses module
        find_package(Curses REQUIRED)
        if(CURSES_FOUND)
            set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
            set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
        endif()
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

# Add compile definitions for better Unicode support
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        PDCURSES_WINCON
        PDC_WIDE           # Enable wide character support in PDCurses
        UNICODE
        _UNICODE
    )
else()
    # For Unix systems, check if we have wide character support
    if(NCURSESW_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE NCURSES_WIDECHAR)
    endif()
endif()

# Print information about what was found
message(STATUS "Curses include directories: ${CURSES_INCLUDE_DIRS}")
message(STATUS "Curses libraries: ${CURSES_LIBRARIES}")
