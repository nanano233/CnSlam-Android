cmake_minimum_required(VERSION 3.4.1)

# 1. 包含头文件路径
# 注意：这里加入了 Sophus 和 Boost
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/CameraModels)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/Eigen3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/Sophus)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/boost)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/openssl/include)

# 2. 导入预编译库 (DBoW2 和 g2o)
add_library(lib_dbow2 SHARED IMPORTED)
set_target_properties(lib_dbow2 PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libDBoW2.so)

add_library(lib_g2o SHARED IMPORTED)
set_target_properties(lib_g2o PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libg2o.so)

# 3. 导入 Boost 和 OpenSSL
add_library(lib_boost_serialization STATIC IMPORTED)
set_target_properties(lib_boost_serialization PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libboost_serialization.a)

add_library(lib_crypto STATIC IMPORTED)
set_target_properties(lib_crypto PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libcrypto.a)

# 4. 定义 AdaptSLAM 核心库
add_library(lib_adaptslam SHARED
        src/System.cc
        src/TcpSocket.cc
        src/Tracking.cc
        src/LocalMapping.cc
        src/LoopClosing.cc
        src/ORBextractor.cc
        src/ORBmatcher.cc
        src/Converter.cc
        src/MapPoint.cc
        src/KeyFrame.cc
        src/Atlas.cc
        src/Map.cc
#        src/MapDrawer.cc
        src/Optimizer.cc
        src/Frame.cc
        src/KeyFrameDatabase.cc
        src/Sim3Solver.cc
#        src/Viewer.cc
        src/ImuTypes.cc
        src/G2oTypes.cc
        src/CameraModels/Pinhole.cpp
        src/CameraModels/KannalaBrandt8.cpp
        src/OptimizableTypes.cpp
        src/MLPnPsolver.cpp
        src/GeometricTools.cc
        src/TwoViewReconstruction.cc
        src/Config.cc
        src/Settings.cc
        src/Uncertainty.cc
)

# 5. 链接 AdaptSLAM 的依赖
target_link_libraries(lib_adaptslam
        lib_dbow2
        lib_g2o
        lib_boost_serialization
        lib_crypto
        OpenCV
        atomic
        android
        log)

# 6. 编译 JNI 接口库 (ORBSLAM)
add_library(ORBSLAM SHARED
        importORBSLAM.cpp
        nViewer.cpp
        nViewer.hpp
)

# 7. 链接 JNI 库与核心库
target_link_libraries(ORBSLAM
        lib_adaptslam
        android
        log)