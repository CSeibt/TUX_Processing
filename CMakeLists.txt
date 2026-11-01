# Define the minimum CMake version and project name.
cmake_minimum_required(VERSION 3.10)
project(TUX_Processing)

# Require C++17 for all targets in this project.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate ROOT and load its CMake configuration.
find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

# Collect all C/C++ source files under src/, including files added later.
file(GLOB_RECURSE BIRO_PROC_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE BIRO_PROC_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

add_executable(biro_proc biro_proc.cpp ${BIRO_PROC_SOURCES} ${BIRO_PROC_HEADERS})

# Make the project headers and ROOT headers available to the executable.
target_include_directories(biro_proc PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${ROOT_INCLUDE_DIRS}
)

# Link the executable against the ROOT libraries.
target_link_libraries(biro_proc PRIVATE ${ROOT_LIBRARIES})


