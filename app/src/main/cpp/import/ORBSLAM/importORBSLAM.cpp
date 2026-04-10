#include <jni.h>
#include <android/log.h>
#include <string>

#include <ORBmatcher.h>
#include <ORBVocabulary.h>
#include <System.h>
#include <MapPoint.h>
#include <Tracking.h>
#include <Atlas.h>
#include <ImuTypes.h>

#include <jni.h>

#include <importOpenCV.h>

#include "nViewer.hpp"

#include "Converter.h"

#include <opencv2/core.hpp>

static const char* TAG = "ORBSLAM";

extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nCreateVocabulary(
        JNIEnv *env, jclass clazz,jstring txt_file) {

    // TODO: implement nCreateVocabulary()

    const char* filename_str;
    filename_str = env->GetStringUTFChars(txt_file, 0);
    if(!filename_str) {
        env->ReleaseStringUTFChars(txt_file, filename_str);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Vocabulary Filename Error");
        return 0;
    }

    ORB_SLAM3::ORBVocabulary* mpVocabulary = new ORB_SLAM3::ORBVocabulary();
    bool bVocLoad = mpVocabulary->loadFromTextFile(filename_str);
    if(!bVocLoad) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Vocabulary File Load Error %s",filename_str);
        delete mpVocabulary;
        return 0;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,"Vocabulary File Load Success %s",filename_str);

    return (long)mpVocabulary;

}


extern "C"
JNIEXPORT void JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nDeleteVocabulary(
        JNIEnv *env, jclass clazz,jlong pVocabulary) {

    ORB_SLAM3::ORBVocabulary* mpVocabulary = (ORB_SLAM3::ORBVocabulary*)pVocabulary;
    delete mpVocabulary;
}


extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nCreateSystemMono(JNIEnv *env, jclass clazz,
                                                                     jstring file_setting,
                                                                     jstring file_orb_voc,
                                                                     // 新增 JNI 参数
                                                                     jstring run_type,
                                                                     jstring server_ip,
                                                                     jstring server_port) {
    // 强行关闭 OpenCV 的 SIMD/NEON 优化
    cv::setUseOptimized(false);
    // TODO: implement nCreateSystemMono()
    __android_log_print(ANDROID_LOG_INFO, TAG, "Starting Good Luck!");

    const char* filename_setting_str;
    filename_setting_str = env->GetStringUTFChars(file_setting, 0);
    if(!filename_setting_str) {
        env->ReleaseStringUTFChars(file_setting, filename_setting_str);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Setting Filename Error");
        return 0;
    }

    const char* filename_voctxt_str;
    filename_voctxt_str = env->GetStringUTFChars(file_orb_voc, 0);
    if(!filename_voctxt_str) {
        env->ReleaseStringUTFChars(file_orb_voc, filename_voctxt_str);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Vocabulary Filename Error");
        return 0;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "Camera %s",filename_setting_str);
    __android_log_print(ANDROID_LOG_INFO, TAG, "Voc %s",filename_voctxt_str);

    // 字符串转换代码
    const char* run_type_str = env->GetStringUTFChars(run_type, 0);
    const char* server_ip_str = env->GetStringUTFChars(server_ip, 0);
    const char* server_port_str = env->GetStringUTFChars(server_port, 0);

    ORB_SLAM3::System* system = 0;
    try
    {
        // 调用修改后的 System 构造函数
        system = new ORB_SLAM3::System(filename_voctxt_str, filename_setting_str,
                                       ORB_SLAM3::System::MONOCULAR, false, 0, "Android",
                                       string(run_type_str), string(server_ip_str), string(server_port_str));
    }
    catch(int err)
    {
        __android_log_print(ANDROID_LOG_INFO, TAG, "catch error %d",err);
    }


    env->ReleaseStringUTFChars(file_orb_voc, filename_voctxt_str);
    env->ReleaseStringUTFChars(file_setting, filename_setting_str);

    __android_log_print(ANDROID_LOG_INFO, TAG, "Start Success! %X",system);

    // ReleaseStringUTFChars
    env->ReleaseStringUTFChars(run_type, run_type_str);
    env->ReleaseStringUTFChars(server_ip, server_ip_str);
    env->ReleaseStringUTFChars(server_port, server_port_str);

    return (long)system;

}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nShutdown(JNIEnv *env, jclass clazz,
                                                             jlong p_system) {
    // TODO: implement nShutdown()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    system->Shutdown();
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nDeleteSystemMono(JNIEnv *env, jclass clazz,  jlong p_system) {
    // TODO: implement nDeleteSystemMono()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    delete system;
}


extern "C"
JNIEXPORT jint JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemGetTrackingState(JNIEnv *env,
                                                                           jclass clazz,
                                                                           jlong p_system) {

    // TODO: implement nSystemGetTrackingState()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    return system->GetTrackingState();


}

extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemTrackingMono(JNIEnv *env, jclass clazz,
                                                                       jlong p_system,
                                                                       jobject bitmap,
                                                                       jdouble second) {
    // 1. 获取 Bitmap
    cv::Mat input = cv::bitmap2Mat(env, bitmap);

    // 2. 转灰度图
    cv::Mat inputGray;
    if (input.channels() == 4) {
        cv::cvtColor(input, inputGray, cv::COLOR_RGBA2GRAY);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, inputGray, cv::COLOR_RGB2GRAY);
    } else {
        inputGray = input.clone();
    }

    // 3. 内存隔离护盾
    cv::Mat safeInputGray(inputGray.rows, inputGray.cols, inputGray.type());
    inputGray.copyTo(safeInputGray);

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;

    // 4. 传入 safeInputGray
    Sophus::SE3f Tcw = system->TrackMonocular(safeInputGray, second);

    // 5. 直接在原图绘制特征点
    std::vector<cv::KeyPoint> rawKeypoints = system->mpTracker->mCurrentFrame.mvKeys;
    input = frame_draw_fast(&input, rawKeypoints, cv::Scalar(0, 255, 0), 1.0f);
    cv::mat2Bitmap(env, bitmap, input);

    return 0;
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemGetCurrentMapPoints(JNIEnv *env,
                                                                              jclass clazz,
                                                                              jlong p_system) {
    // TODO: implement nSystemGetCurrentMapPoints()

    // Debug
    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    //cv::Mat pose = system->mpTracker->mCurrentFrame.mTcw ;
    // __android_log_print(ANDROID_LOG_INFO, TAG, "Pose %d %d",pose.rows,pose.cols);
//    if(pose.rows>3&&pose.cols>3)
//    {
//        __android_log_print(ANDROID_LOG_INFO, TAG,
//            "R0 %f %f %f %f",pose.at<float>(0,0),pose.at<float>(0,1),pose.at<float>(0,2),pose.at<float>(0,3));
//        __android_log_print(ANDROID_LOG_INFO, TAG,
//            "R1 %f %f %f %f",pose.at<float>(1,0),pose.at<float>(1,1),pose.at<float>(1,2),pose.at<float>(1,3));
//        __android_log_print(ANDROID_LOG_INFO, TAG,
//            "R2 %f %f %f %f",pose.at<float>(2,0),pose.at<float>(2,2),pose.at<float>(2,2),pose.at<float>(2,3));
//        __android_log_print(ANDROID_LOG_INFO, TAG,
//                            "R3 %f %f %f %f",pose.at<float>(3,0),pose.at<float>(3,2),pose.at<float>(3,2),pose.at<float>(3,3));
//    }
    //

    // TODO: Print Debug
    std::vector<ORB_SLAM3::MapPoint*> vpMPs = system->mpAtlas->GetAllMapPoints();
    __android_log_print(ANDROID_LOG_INFO, TAG, "MapPoints=%d",vpMPs.size());

    std::vector<float> vPoints ;
    for(size_t i=0, iend=vpMPs.size(); i<iend;i++)
    {
        if(vpMPs[i]->isBad())
            continue;
        Eigen::Vector3f pos = vpMPs[i]->GetWorldPos();
        vPoints.push_back(pos(0));
        vPoints.push_back(-pos(1));
        vPoints.push_back(pos(2));
    }


    jfloatArray resArr = env->NewFloatArray(vPoints.size());
//    float _data[6];
//    for(int i=0;i<6;i++)
//    {
//        _data[i] = 0.3 * i;
//    }

    env->SetFloatArrayRegion(resArr,0,vPoints.size(),vPoints.data());
    return resArr;

}
extern "C"
JNIEXPORT jfloatArray JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemGetCurrentCamPose(JNIEnv *env,
                                                                            jclass clazz,
                                                                            jlong p_system) {
    // TODO: implement nSystemGetCurrentCamPose()
    // Debug
    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    Sophus::SE3f Tcw = system->mpTracker->mCurrentFrame.GetPose();
    cv::Mat pose = ORB_SLAM3::Converter::toCvMat(Tcw.matrix());

    if(pose.rows==4&&pose.cols==4)
    {
        // pose = pose.t();
        jfloatArray resArr = env->NewFloatArray(16);
        float _data[16];

//        for(int r=0;r<4;r++)
//        {
//            for(int c=0;c<4;c++)
//            {
//                _data[r*4+c] = pose.at<float>(r,c);
//            }
//        }

//        cv::Mat Rwc(3,3,CV_32F);
//        cv::Mat twc(3,1,CV_32F);
//
//        Rwc = pose.rowRange(0,3).colRange(0,3).t();
//        twc = -Rwc*pose.rowRange(0,3).col(3);
        Sophus::SE3f Twc = Tcw.inverse();
        cv::Mat poseInv = ORB_SLAM3::Converter::toCvMat(Twc.matrix());

        for(int i=0; i<16; i++)
            _data[i] = poseInv.at<float>(i/4, i%4);


        env->SetFloatArrayRegion(resArr,0,16,_data);
        return resArr;
    }

    return nullptr;

}
extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemTrackingMonoIMU(JNIEnv *env, jclass clazz,
                                                                          jlong p_system,
                                                                          jobject bitmap,
                                                                          jdouble second,
                                                                          jdoubleArray point_data) {
    // 1. 获取 Bitmap
    cv::Mat input = cv::bitmap2Mat(env, bitmap);

    // 2. 灰度转换和内存隔离
    cv::Mat inputGray;
    if (input.channels() == 4) {
        cv::cvtColor(input, inputGray, cv::COLOR_RGBA2GRAY);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, inputGray, cv::COLOR_RGB2GRAY);
    } else {
        inputGray = input.clone();
    }
    cv::Mat safeInputGray(inputGray.rows, inputGray.cols, inputGray.type());
    inputGray.copyTo(safeInputGray);

    // 3. 获取 IMU 数据
    int IMUDataLen = env->GetArrayLength(point_data);
    jdouble *pIMUData = env->GetDoubleArrayElements(point_data, NULL);
    vector<ORB_SLAM3::IMU::Point> imupoints;

    for(int i=0; i<IMUDataLen/7; i++) {
        double ax = pIMUData[i*7+0];
        double ay = pIMUData[i*7+1];
        double az = pIMUData[i*7+2];
        double gx = pIMUData[i*7+3];
        double gy = pIMUData[i*7+4];
        double gz = pIMUData[i*7+5];
        double timestamp = pIMUData[i*7+6];
        ORB_SLAM3::IMU::Point _point(ax,ay,az,gx,gy,gz,timestamp);
        imupoints.push_back(_point);
    }

    // 用完 JNI 数组释放
    env->ReleaseDoubleArrayElements(point_data, pIMUData, JNI_ABORT);

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;

    try {
        // 4. 传入安全的原分辨率灰度图
        Sophus::SE3f Tcw = system->TrackMonocular(safeInputGray, second, -1, imupoints);

        // 5.画特征点
        input = frame_draw_fast(&input, system->GetTrackedKeyPointsUn(), cv::Scalar(0,255,0), 1.0f);
    }
    catch(int err) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Track Error %X",err);
    }

    cv::mat2Bitmap(env,bitmap,input);
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nReset(JNIEnv *env, jclass clazz,jlong p_system) {
    // TODO: implement nReset()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    system->Reset();

}