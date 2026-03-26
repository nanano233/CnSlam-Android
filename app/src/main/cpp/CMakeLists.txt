cmake_minimum_required(VERSION 3.4.1)
project("hitomi")

add_definitions(-DEIGEN_DONT_ALIGN_STATICALLY)
add_definitions(-DEIGEN_DONT_VECTORIZE)
add_definitions(-DDISABLE_SMI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DEIGEN_DONT_ALIGN_STATICALLY -DEIGEN_DONT_VECTORIZE")

# Cpp Debug
add_library(SampleCpp SHARED
             SampleCpp.cpp)

find_library(log-lib
              log )

target_link_libraries( SampleCpp
                       ${log-lib} )

# Root
set(EXIMPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/import)

# import Util
add_subdirectory(${EXIMPORT_DIR}/jniUtil)

# import OpenCV
include_directories(${EXIMPORT_DIR}/OpenCV/)
include_directories(${EXIMPORT_DIR}/OpenCV/include)
add_subdirectory(${EXIMPORT_DIR}/OpenCV)

# import ORBSLAM
# add_subdirectory(${EXIMPORT_DIR}/ORBSLAM)

# import debug
add_subdirectory(${EXIMPORT_DIR}/hitomiDebug)

# import ORBSLAM
add_subdirectory(${EXIMPORT_DIR}/ORBSLAM)
