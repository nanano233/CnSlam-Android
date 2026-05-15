package cn.koistudio.hitomi.module.OrbSlam;

import static cn.koistudio.hitomi.activity.OrbSlam.MapRender.printfMatrix44;

import android.graphics.Bitmap;
import android.util.Log;

import java.util.List;

import cn.koistudio.hitomi.util.uCamera;

/**
 * 模仿C语言语法编写的接口类
 * 基本上是为了直接调用native的C接口
 */
public class SystemMono {

    static {
        System.loadLibrary("ORBSLAM");
    }

    // 传感器类型常量，与 ORB_SLAM3::System::eSensor 一致
    public static final int MONOCULAR = 0;
    public static final int IMU_MONOCULAR = 3;


    private uCamera muCamera;
    private String mFileVoc;
    private String mFileCamParam;

    // C++ Objects
    long mpnSystem = 0;

    /**
     *
     * 初始化的时候很耗时 Handler.post调用
     * 检查资源和测试运行环境
     */
    public SystemMono(uCamera camera, String fileVoc, String fileCamParam, int sensorType)
    {
        muCamera = camera;

        // 这里设置 AdaptSLAM 的参数，后续可以改为从 UI 获取
        String runType = "client";
        String serverIp = "192.168.1.161"; // 请改为你的服务器实际IP
        String serverPort = "10001";

        mpnSystem = nCreateSystemMono(fileCamParam, fileVoc, runType, serverIp, serverPort, sensorType);

    }

    public static final int SYSTEM_NOT_READY = -1 ;
    public static final int NO_IMAGES_YET    =  0 ;
    public static final int NOT_INITIALIZED  =  1 ;
    public static final int OK               =  2 ;
    public static final int RECENTLY_LOST    =  3 ;
    public static final int LOST             =  4 ;
    public static final int OK_KLT           =  5 ;

    public int getTrackingStateInt()
    {
        return nSystemGetTrackingState(mpnSystem);
    }

    public String getTrackingStateStringCN()
    {
        int state = nSystemGetTrackingState(mpnSystem);
        switch (state)
        {
            case NO_IMAGES_YET:
                return "暂无图像";
            case NOT_INITIALIZED:
                return "姿态初始化中";
            case OK:
                return "正在建图";
            case RECENTLY_LOST:
                return "匹配异常";
            case LOST:
                return "姿态丢失";
            default:
                return "未初始化";
        }

    }



    public float[] getTrajectory()
    {
        return nSystemGetCurrentTrajectory(mpnSystem);
    }

    public float[] mPose = {
            1.0f,0.0f,0.0f,0.0f,
            0.0f,1.0f,0.0f,0.0f,
            0.0f,0.0f,1.0f,0.0f,
            0.0f,0.0f,0.0f,1.0f};

    public float[] mMapPoints = new float[0];

    public void TrackingMono(Bitmap bitmap,double second)
    {
        nSystemTrackingMono(mpnSystem,bitmap,second);

        float[] pose = nSystemGetCurrentCamPose(mpnSystem);
        mMapPoints = nSystemGetCurrentMapPoints(mpnSystem);
        if(pose!=null)
        {
            printfMatrix44(pose);
            this.mPose = pose ;
        }
        else
        {
            mPose = new float[]{
                    1.0f,0.0f,0.0f,0.0f,
                    0.0f,1.0f,0.0f,0.0f,
                    0.0f,0.0f,-1.0f,0.0f,
                    0.0f,0.0f,0.0f,1.0f};
        }

        // Log.d("TAG",""+_f[3]);
    }

    public void TrackingMonoIMU(Bitmap bitmap, double second, List<double[]> points)
    {

        double[] pointData = new double[7*points.size()];
        for(int i=0;i<points.size();i++)
        {
            double[] onePoint = points.get(i);
            pointData[i*7+0] = onePoint[0];
            pointData[i*7+1] = onePoint[1];
            pointData[i*7+2] = onePoint[2];
            pointData[i*7+3] = onePoint[3];
            pointData[i*7+4] = onePoint[4];
            pointData[i*7+5] = onePoint[5];
            pointData[i*7+6] = onePoint[6]; // timestamp
            // Log.d("TAG","time="+pointData[i*7+6]);
        }

        nSystemTrackingMonoIMU(mpnSystem,bitmap,second,pointData);

        float[] pose = nSystemGetCurrentCamPose(mpnSystem);
        mMapPoints = nSystemGetCurrentMapPoints(mpnSystem);
        if(pose!=null)
        {
            printfMatrix44(pose);
            this.mPose = pose ;
        }
        else
        {
            mPose = new float[]{
                    1.0f,0.0f,0.0f,0.0f,
                    0.0f,1.0f,0.0f,0.0f,
                    0.0f,0.0f,-1.0f,0.0f,
                    0.0f,0.0f,0.0f,1.0f};
        }

        // Log.d("TAG",""+_f[3]);
    }




    /**
     * 释放资源前必须调用
     */
    public synchronized void release() // 加上 synchronized 防止多线程双重释放
    {
        if(this.mpnSystem != 0) {
            nShutdown(this.mpnSystem);
            nDeleteSystemMono(this.mpnSystem);
            this.mpnSystem = 0; // 置零确保即使被调用两次也是安全的
        }

//        if(mpVocabulary!=0)
//        {
//            nDeleteVocabulary(mpVocabulary);
//            mpVocabulary = 0;
//        }

        //nShutdown(this.mpnSystem);
        //nDeleteSystemMono(this.mpnSystem);
    }


    /**
     * 释放资源前必须调用
     */
    public void reset()
    {
        if(this.mpnSystem!=0) {
            nReset(this.mpnSystem);
        }
    }

    /**
     * 重新设置Yaml的内容
     */
//    static native int nPrepareSettingFile(String fileSetting,int height,int weight);

    /**
     * 加载词袋模型
     * @param txtFile 文件路径
     * @return
     */
    static native long nCreateVocabulary(String txtFile);
    static native void nDeleteVocabulary(long pVocabulary);

    /**
     * 相当于 new ORBSLAM::SYSTEM 和 delete ORMSLAM::SYSTEM
     */
    static native long nCreateSystemMono(String fileSetting, String fileOrbVoc, String runType, String serverIp, String serverPort, int sensorType);
    static native void nDeleteSystemMono(long pSystem);


//    static native int nTrack(long pSystem, Bitmap bitmap,long timestamp);
    static native int nShutdown(long pSystem);
    static native void nReset(long pSystem);

    static native int nSystemGetTrackingState(long pSystem);
    static native long nSystemTrackingMono(long pSystem,Bitmap bitmap,double second);
    static native long nSystemTrackingMonoIMU(long pSystem,Bitmap bitmap,double second,double[] pointData);

    static native float[] nSystemGetCurrentMapPoints(long pSystem);
    static native float[] nSystemGetCurrentCamPose(long pSystem);
    static native float[] nSystemGetCurrentTrajectory(long pSystem);

    // ==================== 新增：YOLO 初始化接口 ====================
    public native boolean nInitYOLO(android.content.res.AssetManager mgr);
    // ===============================================================
}
