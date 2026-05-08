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

#include "net.h"
#include <opencv2/imgproc.hpp>
#include <android/asset_manager_jni.h>
#include <opencv2/dnn.hpp>

#include <exception>
#include <chrono>

static const char* TAG = "ORBSLAM";

// 定义检测结果结构
struct Object {
    cv::Rect_<float> rect;
    int label;
    float prob;
};
// 定义 YOLO 网络
static ncnn::Net yolo_net;
static bool bYoloInitialized = false;

// YOLO 统计计数器
static int gYoloTotalFrames = 0;
static int gYoloDetectFrames = 0;
static int gYoloTotalPersons = 0;
static float gYoloTotalConf = 0.0f;
static double gYoloTotalTimeMs = 0.0;

cv::Mat detect_dynamic_mask(const cv::Mat& bgr_img) {
    if (!bYoloInitialized) return cv::Mat();

    gYoloTotalFrames++;
    auto t1 = std::chrono::steady_clock::now();

    int img_w = bgr_img.cols;
    int img_h = bgr_img.rows;
    const int target_size = 256;

    // ==================== 1. OpenCV 安全预处理 ====================
    float scale = std::min((float)target_size / img_w, (float)target_size / img_h);
    int new_w = img_w * scale;
    int new_h = img_h * scale;

    cv::Mat resized_img;
    cv::resize(bgr_img, resized_img, cv::Size(new_w, new_h));

    int wpad = target_size - new_w;
    int hpad = target_size - new_h;

    // 创建纯净的 320x320 灰色背景 (强制内存连续)
    cv::Mat padded_img(target_size, target_size, CV_8UC3, cv::Scalar(114, 114, 114));
    resized_img.copyTo(padded_img(cv::Rect(wpad / 2, hpad / 2, new_w, new_h)));

    ncnn::Mat in = ncnn::Mat::from_pixels(padded_img.data, ncnn::Mat::PIXEL_BGR2RGB, target_size, target_size);

    const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
    in.substract_mean_normalize(0, norm_vals);

    // ==================== 2. YOLO 推理 ====================
    ncnn::Extractor ex = yolo_net.create_extractor();
    ex.input("images", in);
    ncnn::Mat out;
    ex.extract("output", out);

    cv::Mat mask(img_h, img_w, CV_8UC1, cv::Scalar(255)); // 默认全白
    if (out.empty() || out.h != 1344 || out.w != 144) return mask;

    // ==================== 3. 解析与 NMS ====================
    int strides[3] = {8, 16, 32};
    int grids[3] = {32, 16, 8};
    int anchor_idx = 0;

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;

    for (int s = 0; s < 3; s++) {
        int stride = strides[s];
        int grid_size = grids[s];
        for (int i = 0; i < grid_size; i++) {
            for (int j = 0; j < grid_size; j++) {
                const float* ptr = out.row(anchor_idx);

                float score = ptr[64]; // 人类类别
                if (score > 1.0f || score < 0.0f) {
                    score = 1.0f / (1.0f + exp(-score));
                }

                if (score > 0.65f) {
                    float dfl[4];
                    for (int k = 0; k < 4; k++) {
                        float sum = 0.f, exp_sum = 0.f;
                        // 寻找最大值防止 exp 溢出
                        float max_val = ptr[k * 16];
                        for (int v = 1; v < 16; v++) {
                            max_val = std::max(max_val, ptr[k * 16 + v]);
                        }

                        float exp_vals[16];
                        for (int v = 0; v < 16; v++) {
                            exp_vals[v] = exp(ptr[k * 16 + v] - max_val);
                            exp_sum += exp_vals[v];
                        }
                        for (int v = 0; v < 16; v++) {
                            sum += (exp_vals[v] / exp_sum) * v;
                        }
                        dfl[k] = sum;
                    }

                    float px1 = (j + 0.5f - dfl[0]) * stride;
                    float py1 = (i + 0.5f - dfl[1]) * stride;
                    float px2 = (j + 0.5f + dfl[2]) * stride;
                    float py2 = (i + 0.5f + dfl[3]) * stride;

                    // 映射回原图 (640x480)
                    float rx1 = (px1 - (wpad / 2.0f)) / scale;
                    float ry1 = (py1 - (hpad / 2.0f)) / scale;
                    float rx2 = (px2 - (wpad / 2.0f)) / scale;
                    float ry2 = (py2 - (hpad / 2.0f)) / scale;

                    // 强制保证 x_min 永远小于 x_max，避免出现负数宽高的“幽灵框”逃过 NMS
                    float x_min = std::max(0.0f, std::min(rx1, rx2));
                    float y_min = std::max(0.0f, std::min(ry1, ry2));
                    float x_max = std::min((float)img_w - 1.0f, std::max(rx1, rx2));
                    float y_max = std::min((float)img_h - 1.0f, std::max(ry1, ry2));

                    // 只有宽度和高度大于 0 的有效框才允许进入 NMS 候选队列
                    if (x_max > x_min && y_max > y_min) {
                        boxes.push_back(cv::Rect(x_min, y_min, x_max - x_min, y_max - y_min));
                        scores.push_back(score);
                    }
                }
                anchor_idx++;
            }
        }
    }

    // ==================== 4. 消除所有重叠框 ====================
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, 0.45f, 0.45f, indices);

    for (int idx : indices) {
        cv::rectangle(mask, boxes[idx], cv::Scalar(0), -1);
    }

    // 更新 YOLO 统计
    if (!indices.empty()) gYoloDetectFrames++;
    gYoloTotalPersons += (int)indices.size();
    for (int idx : indices) gYoloTotalConf += scores[idx];

    // 轻微膨胀掩码静态区域，缩小动态遮罩，保留行人边缘附近的静态特征
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::dilate(mask, mask, kernel);

    // 计时与周期日志
    auto t2 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    gYoloTotalTimeMs += ms;

    if (gYoloTotalFrames % 100 == 0) {
        float avgDetPerFrame = (float)gYoloTotalPersons / gYoloTotalFrames;
        float avgConf = gYoloTotalPersons > 0 ? gYoloTotalConf / gYoloTotalPersons : 0;
        float avgTime = (float)(gYoloTotalTimeMs / gYoloTotalFrames);
        __android_log_print(ANDROID_LOG_INFO, "YOLO_STATS",
            "Frames=%d DetectFrames=%d TotalPersons=%d AvgDet=%.2f AvgConf=%.3f AvgTime=%.1fms",
            gYoloTotalFrames, gYoloDetectFrames, gYoloTotalPersons,
            avgDetPerFrame, avgConf, avgTime);
    }

    return mask;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nCreateVocabulary(
        JNIEnv *env, jclass clazz,jstring txt_file) {

    // TODO: implement nCreateVocabulary()

    const char* filename_str;
    filename_str = env->GetStringUTFChars(txt_file, 0);
    if(!filename_str) {
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
                                                                     jstring run_type,
                                                                     jstring server_ip,
                                                                     jstring server_port,
                                                                     jint sensor_type) {
    // 启用 OpenCV 的 SIMD/NEON 优化（提高精度一致性）
    cv::setUseOptimized(true);
    // TODO: implement nCreateSystemMono()
    __android_log_print(ANDROID_LOG_INFO, TAG, "Starting Good Luck!");

    const char* filename_setting_str;
    filename_setting_str = env->GetStringUTFChars(file_setting, 0);
    if(!filename_setting_str) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Setting Filename Error");
        return 0;
    }

    const char* filename_voctxt_str;
    filename_voctxt_str = env->GetStringUTFChars(file_orb_voc, 0);
    if(!filename_voctxt_str) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Vocabulary Filename Error");
        return 0;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "Camera %s",filename_setting_str);
    __android_log_print(ANDROID_LOG_INFO, TAG, "Voc %s",filename_voctxt_str);

    // 字符串转换代码（防御 NULL）
    const char* run_type_str = env->GetStringUTFChars(run_type, 0);
    const char* server_ip_str = env->GetStringUTFChars(server_ip, 0);
    const char* server_port_str = env->GetStringUTFChars(server_port, 0);
    std::string runTypeStr = run_type_str ? run_type_str : "";
    std::string serverIpStr = server_ip_str ? server_ip_str : "";
    std::string serverPortStr = server_port_str ? server_port_str : "";

    ORB_SLAM3::System* system = 0;
    try
    {
        ORB_SLAM3::System::eSensor eSensorType = static_cast<ORB_SLAM3::System::eSensor>(sensor_type);
        system = new ORB_SLAM3::System(filename_voctxt_str, filename_setting_str,
                                       eSensorType, false, 0, "Android",
                                       runTypeStr, serverIpStr, serverPortStr);
    }
    catch(const std::exception& e) {
        // 把 OpenCV 的格式错误打印在 Logcat 里
        __android_log_print(ANDROID_LOG_ERROR, TAG, "C++ FATAL EXCEPTION: %s", e.what());
    }
    catch(int err) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "catch error %d", err);
    }
    catch(...) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Unknown C++ crash");
    }


    env->ReleaseStringUTFChars(file_orb_voc, filename_voctxt_str);
    env->ReleaseStringUTFChars(file_setting, filename_setting_str);

    __android_log_print(ANDROID_LOG_INFO, TAG, "Start Success! %X",system);

    // ReleaseStringUTFChars
    if (run_type_str) env->ReleaseStringUTFChars(run_type, run_type_str);
    if (server_ip_str) env->ReleaseStringUTFChars(server_ip, server_ip_str);
    if (server_port_str) env->ReleaseStringUTFChars(server_port, server_port_str);

    return (long)system;

}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nShutdown(JNIEnv *env, jclass clazz,
                                                             jlong p_system) {
    // TODO: implement nShutdown()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    if (!system) return 0;
    system->Shutdown();
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nDeleteSystemMono(JNIEnv *env, jclass clazz,  jlong p_system) {
    // TODO: implement nDeleteSystemMono()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    if (!system) return;
    delete system;
}


extern "C"
JNIEXPORT jint JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemGetTrackingState(JNIEnv *env,
                                                                           jclass clazz,
                                                                           jlong p_system) {

    // TODO: implement nSystemGetTrackingState()

    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    if (!system) return -1;
    return system->GetTrackingState();


}

// 模块耗时统计计数器
static int gPerfFrameCount = 0;
static double gTotalBmp2MatMs = 0, gTotalMaskVisMs = 0, gTotalTrackMs = 0, gTotalDrawMs = 0;
static int gTotalKeypoints = 0, gTotalTrackOk = 0;

extern "C"
JNIEXPORT jlong JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nSystemTrackingMono(JNIEnv *env, jclass clazz,
                                                                       jlong p_system,
                                                                       jobject bitmap,
                                                                       jdouble second) {
    auto t0 = std::chrono::steady_clock::now();

    // 1. 获取 Bitmap（RGB 图像）
    cv::Mat input = cv::bitmap2Mat(env, bitmap);
    auto t1 = std::chrono::steady_clock::now();

    // 2. 转为 BGR（匹配电脑端 cv::imread 的输出格式）
    cv::Mat bgrImg;
    if (input.channels() == 4) {
        cv::cvtColor(input, bgrImg, cv::COLOR_RGBA2BGR);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, bgrImg, cv::COLOR_RGB2BGR);
    } else {
        bgrImg = input.clone();
    }

    // 3. YOLO 动态掩码提取
    cv::Mat dynamicMask = detect_dynamic_mask(bgrImg);

    // 4. 半透明掩码可视化（在 RGB input 上，只影响显示）
    if (!dynamicMask.empty()) {
        cv::Mat maskHighlight;
        cv::bitwise_not(dynamicMask, maskHighlight);
        const float alpha = 0.4f;
        if (input.channels() == 4) {
            cv::Mat overlay(input.size(), input.type(), cv::Scalar(0, 0, 255, 0));
            cv::Mat blended;
            cv::addWeighted(input, 1.0f - alpha, overlay, alpha, 0, blended);
            blended.copyTo(input, maskHighlight);
        } else {
            cv::Mat overlay(input.size(), input.type(), cv::Scalar(0, 0, 255));
            cv::Mat blended;
            cv::addWeighted(input, 1.0f - alpha, overlay, alpha, 0, blended);
            blended.copyTo(input, maskHighlight);
        }
    }
    auto t2 = std::chrono::steady_clock::now();

    // 5. 追踪：传入 BGR 原图 + 动态掩码
    ORB_SLAM3::System* system = (ORB_SLAM3::System*) p_system;
    if (!system) return 0;

    Sophus::SE3f Tcw = system->TrackMonocular(
            bgrImg,
            second,
            -1,
            std::vector<ORB_SLAM3::IMU::Point>(),
            "",
            dynamicMask
    );
    auto t3 = std::chrono::steady_clock::now();

    // 6. 在显示图像上绘制特征点
    if (system->mpTracker) {
        std::vector<cv::KeyPoint> rawKeypoints = system->mpTracker->mCurrentFrame.mvKeys;
        input = frame_draw_fast(&input, rawKeypoints, cv::Scalar(0, 255, 0), 1.0f);
    }
    cv::mat2Bitmap(env, bitmap, input);
    auto t4 = std::chrono::steady_clock::now();

    // 累计统计
    double bmpMatMs = std::chrono::duration<double,std::milli>(t1 - t0).count();
    double maskVisMs = std::chrono::duration<double,std::milli>(t2 - t1).count();
    double trackMs = std::chrono::duration<double,std::milli>(t3 - t2).count();
    double drawMs = std::chrono::duration<double,std::milli>(t4 - t3).count();

    gPerfFrameCount++;
    gTotalBmp2MatMs += bmpMatMs;
    gTotalMaskVisMs += maskVisMs;
    gTotalTrackMs += trackMs;
    gTotalDrawMs += drawMs;

    if (system->mpTracker) {
        gTotalKeypoints += (int)system->mpTracker->mCurrentFrame.mvKeys.size();
        if (system->GetTrackingState() == 2) gTotalTrackOk++;
    }

    if (gPerfFrameCount % 100 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "PERF_STATS",
            "Frames=%d Bmp2Mat=%.1fms MaskVis=%.1fms Track=%.1fms Draw=%.1fms Total=%.1fms AvgKP=%d TrackOK=%d",
            gPerfFrameCount,
            gTotalBmp2MatMs/gPerfFrameCount, gTotalMaskVisMs/gPerfFrameCount,
            gTotalTrackMs/gPerfFrameCount, gTotalDrawMs/gPerfFrameCount,
            (gTotalBmp2MatMs+gTotalMaskVisMs+gTotalTrackMs+gTotalDrawMs)/gPerfFrameCount,
            (int)(gTotalKeypoints/gPerfFrameCount), gTotalTrackOk);
    }

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
    if (!system) return nullptr;
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
    if (!system || !system->mpTracker) return nullptr;
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
    // 1. 获取 Bitmap（RGB 图像）
    cv::Mat input = cv::bitmap2Mat(env, bitmap);

    // 2. 转为 BGR（匹配电脑端 cv::imread 的输出格式）
    cv::Mat bgrImg;
    if (input.channels() == 4) {
        cv::cvtColor(input, bgrImg, cv::COLOR_RGBA2BGR);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, bgrImg, cv::COLOR_RGB2BGR);
    } else {
        bgrImg = input.clone();
    }

    // 3. YOLO 动态掩码提取
    cv::Mat dynamicMask = detect_dynamic_mask(bgrImg);

    // 4. 半透明掩码可视化（只影响显示，不影响追踪）
    if (!dynamicMask.empty()) {
        cv::Mat maskHighlight;
        cv::bitwise_not(dynamicMask, maskHighlight);
        const float alpha = 0.4f;
        if (input.channels() == 4) {
            cv::Mat overlay(input.size(), input.type(), cv::Scalar(0, 0, 255, 0));
            cv::Mat blended;
            cv::addWeighted(input, 1.0f - alpha, overlay, alpha, 0, blended);
            blended.copyTo(input, maskHighlight);
        } else {
            cv::Mat overlay(input.size(), input.type(), cv::Scalar(0, 0, 255));
            cv::Mat blended;
            cv::addWeighted(input, 1.0f - alpha, overlay, alpha, 0, blended);
            blended.copyTo(input, maskHighlight);
        }
    }

    // 5. 获取 IMU 数据
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
    if (!system) return 0;

    try {
        // 6. 传入 BGR 原图、IMU 数据以及 dynamicMask（让 Tracking 内部做灰度转换）
        Sophus::SE3f Tcw = system->TrackMonocular(
                bgrImg,
                second,
                -1,
                imupoints,
                "",
                dynamicMask
        );

        // 7. 画特征点
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
    if (!system) return;
    system->Reset();

}

extern "C" JNIEXPORT jboolean JNICALL
Java_cn_koistudio_hitomi_module_OrbSlam_SystemMono_nInitYOLO(JNIEnv *env, jobject thiz, jobject asset_manager) {
    if (bYoloInitialized) return JNI_TRUE;

    AAssetManager* mgr = AAssetManager_fromJava(env, asset_manager);

    yolo_net.opt.use_vulkan_compute = false; // GPU 加速
    yolo_net.opt.num_threads = 2;

    // 请确保 assets 目录下有这两个文件
    int ret1 = yolo_net.load_param(mgr, "yolov8n.param");
    int ret2 = yolo_net.load_model(mgr, "yolov8n.bin");

    if (ret1 == 0 && ret2 == 0) {
        bYoloInitialized = true;
        __android_log_print(ANDROID_LOG_INFO, "ORBSLAM", "YOLOv8 Initialized Successfully");
        return JNI_TRUE;
    }
    return JNI_FALSE;
}
