package cn.koistudio.hitomi.activity.OrbSlam;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.Image;
import android.media.ImageReader;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import android.Manifest;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.os.Build;
import android.content.Intent;
import android.provider.Settings;
import android.net.Uri;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Collections;

import cn.koistudio.hitomi.R;
import cn.koistudio.hitomi.module.OrbSlam.SystemMono;
import cn.koistudio.hitomi.util.uIMU;
import cn.koistudio.hitomi.util.uAssets;
import cn.koistudio.hitomi.util.uCamera;
import android.graphics.PixelFormat;

public class MonoActivity extends AppCompatActivity {

    static private final String TAG = "MonoActivity" ;
    private AppCompatActivity mContext = this;
    private GLSurfaceView glSurfaceView ;
    private MapRender mMapRender = new MapRender();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_mono);
        getSupportActionBar().hide();

        glSurfaceView = (GLSurfaceView)findViewById(R.id.SLAM_MAP);
        // 1. 配置 EGL 以支持透明度 (RGBA 8888, Depth 16)
        glSurfaceView.setEGLConfigChooser(8, 8, 8, 8, 16, 0);

        // 2. 设置 SurfaceView 的格式为透明
        glSurfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);

        // 3. 将 SurfaceView 置于媒体层顶层 (在相机画面之上，但 UI 之下)
        glSurfaceView.setZOrderOnTop(true);

        glSurfaceView.setEGLContextClientVersion(2);
        glSurfaceView.setRenderer(mMapRender);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        
    }

    //
    private uIMU muIMU = null;

    // IMG Buffer
    private FpsCounter mFpsCounter = new FpsCounter();
    private uCamera mCamera = null;
    private Bitmap tmBitmap = null;
    private double tmTimestamp = 0;
    private AtomicBoolean bSystemMut = new AtomicBoolean(false);

    // 数据集读取线程
    private Thread mDatasetThread = null;
    // 控制标志位
    private boolean mIsRunningDataset = false;
    // 定义权限请求码
    private static final int PERMISSION_REQUEST_CODE = 100;

    @Override
    protected void onStart() {
        super.onStart();
        // 1. 移除原来的直接调用
        // startDatasetLoop();

        // 2. 检查并申请权限
        checkAndRequestPermissions();
    }
    private void checkAndRequestPermissions() {
        // Android 11 (R) 及以上，使用 MANAGE_EXTERNAL_STORAGE 以获得最广泛的访问权限（测试用）
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!android.os.Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, PERMISSION_REQUEST_CODE);
                return;
            }
        }

        // Android 6.0 到 Android 10
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(this,
                    new String[]{
                            Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE
                    },
                    PERMISSION_REQUEST_CODE);
        } else {
            // 已经有权限了，启动数据集
            startDatasetLoop();
        }
    }

    // 3. 处理权限申请结果
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // 用户同意了权限，启动数据集
                startDatasetLoop();
            } else {
                Toast.makeText(this, "需要存储权限才能读取数据集", Toast.LENGTH_LONG).show();
            }
        }
    }

    // 处理 Android 11 跳转设置页面的返回结果
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                if (android.os.Environment.isExternalStorageManager()) {
                    startDatasetLoop();
                } else {
                    Toast.makeText(this, "请在设置中授予所有文件访问权限", Toast.LENGTH_LONG).show();
                }
            }
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        mIsRunningDataset = false; // 停止循环
        // 停止 Handler 的循环调用，防止它在对象销毁后继续执行
        mHandler.removeCallbacks(mRunTrack);

        if (mCamera != null) {
            mCamera.close();
            mCamera = null;
        }
        if (muIMU != null) {
            muIMU.close();
            muIMU = null;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        glSurfaceView.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        //glSurfaceView.onResume();
    }

    private ImageReader.OnImageAvailableListener onImageAvailableListener = new ImageReader.OnImageAvailableListener()
    {

        @Override
        public void onImageAvailable(ImageReader reader) {

            Image image = reader.acquireLatestImage();
            if(image==null)
                return;

            // Get Bitmap
            byte[] bytes = new byte[image.getPlanes()[0].getBuffer().limit()] ;
            image.getPlanes()[0].getBuffer().get(bytes);
            Bitmap bitmap = BitmapFactory.decodeByteArray( bytes,0,bytes.length);

            // double timestampSec = 1.0*image.getTimestamp()/1000/1000 ;
            double timestampSec = 1.0 * System.currentTimeMillis() / 1000 ;

            // Buffer Bitmap
            if(bSystemMut.compareAndSet(false,true)) {
                tmBitmap = bitmap;
                tmTimestamp = timestampSec;
                // Log.v(TAG,"Camera Update IMG ");
                bSystemMut.set(false);
            }
            else {
                Log.w(TAG,"Camera Skip IMG");
            }

            image.close();
        }

    };


    /**
     * 处理照相机数据的线程
     */
    private Handler mHandler = new Handler();
    private Runnable mRunTrack = new Runnable() {
        @Override
        public void run() {

            // TODO: 姿态数据
            List<double[]> _imu = muIMU.ppStream(-1,0,0,0);

            // TODO: 等原子变量
            if(bSystemMut.compareAndSet(false,true))
            {

                if(tmBitmap!=null) {

                    if(mSystem!=null)
                    {
                        if(mSystemStage=="run") {

                            // TODO: 输入图像
                            // mSystem.TrackingMono(tmBitmap,tmTimestamp);
                            mSystem.TrackingMonoIMU(tmBitmap, tmTimestamp, _imu);

                            // TODO: set 现在照相机位置
                            mMapRender.setCameraMatrix(mSystem.mPose);

                            // TODO: set 需要绘制的点
                            mMapRender.setCoords(mSystem.mMapPoints);

                            // TODO: 重新渲染
                            glSurfaceView.requestRender();
                        }

                        mSystemTrackState = mSystem.getTrackingStateInt();
                        ((TextView)findViewById(R.id.SLAM_MESSAGE)).setText(""+mSystem.getTrackingStateStringCN());

                    }
                    else
                    {
                        ((TextView)findViewById(R.id.SLAM_MESSAGE)).setText("未启动");
                    }

                    // TODO: 左边窗口画图
                    ImageView imageView = findViewById(R.id.SLAM_IMG_CAM);
                    imageView.setImageBitmap(tmBitmap);
                    tmBitmap = null;

                    // TODO: 提示处理的帧率
                    ((TextView)findViewById(R.id.SLAM_STATE)).setText(""+mFpsCounter.update()+"fps");



                }

                bSystemMut.set(false);


            }

            // TODO: 初始化时间较长 按钮加点动态效果
            if(mSystemStage=="init")
            {
                Button _btn = findViewById(R.id.SLAM_START);
                long dots = (System.currentTimeMillis() % 2000) / 500;
                switch ((int) dots) {
                    case 1:
                        _btn.setText(".启动中.");
                        break;
                    case 2:
                        _btn.setText("..启动中..");
                        break;
                    case 3:
                        _btn.setText("...启动中...");
                        break;
                    default:
                        _btn.setText("启动中");
                }

            }

            if(mCamera!=null)
            {
                mHandler.postDelayed(mRunTrack,50);
            }


        }
    };

    private SystemMono mSystem    = null   ;
    private String mSystemStage   = "null" ;
    private int mSystemTrackState = 0;

    private Runnable mTaskInitSystem = new Runnable() {

        @Override
        public void run() {

            // Prepare File
            String filenameVoc = uAssets.prepareAsset(mContext,"ORBvoc.txt");
            // 根据是否运行数据集，加载不同的参数文件
            String yamlFileName = "CameraParam.yaml"; // 默认用手机相机的

            // 这里加一个简单的判断逻辑，或者由 UI 传入
            // 如果主要在测试数据集，建议先强制改为数据集的配置
            boolean isDatasetMode = true;
            if (isDatasetMode) {
                yamlFileName = "PARAconfig.yaml"; // 请确保 assets 里有这个文件
            }

            String filenameParam = uAssets.prepareAsset(mContext, yamlFileName);

            if(filenameVoc!=null||filenameParam!=null)
            {
                mSystem = new SystemMono(mCamera,filenameVoc,filenameParam);
                mSystemStage = "run";

                // TODO: 通知用户
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        Toast hint = Toast.makeText(mContext,"初始化完成",Toast.LENGTH_SHORT);
                        hint.show();
                        ((Button)(mContext.findViewById(R.id.SLAM_START))).setText("停止");
                    }
                });

            }
            else
            {
                Log.e(TAG,"资源文件加载失败");
                mSystemStage = "null";
            }


        }
    };

    private void mSystemReset()
    {
        while(!bSystemMut.compareAndSet(false,true)) {

            try {
                Thread.sleep(2);
            }
            catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        if(mSystem!=null){
            mSystem.reset();
        }


        bSystemMut.set(false);
    }



    public void onClickStart(View view)
    {

        if(mSystemStage=="null")
        {
            // TODO: 启动新的线程资源来初始化SLAM
            Thread thread = new Thread(mTaskInitSystem);
            thread.start();

            mSystemStage = "init";
            Toast hint = Toast.makeText(this,"开始初始化",Toast.LENGTH_LONG);
            hint.show();
        }
        else if(mSystemStage=="init")
        {
            Toast hint = Toast.makeText(this,"请等待初始化完成",Toast.LENGTH_LONG);
            hint.show();
        }
        else if(mSystemStage=="run")
        {
            mSystemStage = "stop";
            ((Button)findViewById(R.id.SLAM_START)).setText("继续");
        }
        else if(mSystemStage=="stop")
        {
            mSystemStage = "run";
            ((Button)findViewById(R.id.SLAM_START)).setText("停止");
            if(mSystemStage=="run"||mSystemStage=="stop") {
                mSystemReset();
            }
        }

    }

    public void onClickReset(View view)
    {
        if(mSystemStage=="run"||mSystemStage=="stop") {
            mSystemReset();
        }
    }

    public void onClickSight(View view)
    {
        if(view.getId()==R.id.SLAM_VIEW_FOLLOW)
            mMapRender.switchFollow(0);

        if(view.getId()==R.id.SLAM_VIEW_TOP)
            mMapRender.switchTop(0);

        if(view.getId()==R.id.SLAM_VIEW_EX)
            mMapRender.switchDist(1);
        if(view.getId()==R.id.SLAM_VIEW_TR)
            mMapRender.switchDist(-1);
    }

    // 保存解析后的所有帧
    private List<FrameData> mDatasetFrames = new java.util.ArrayList<>();

    // 读取数据集索引文件 (例如 data.csv)
    private void loadDataset(String indexFilePath) {
        try {
            File file = new File(indexFilePath);
            BufferedReader br = new BufferedReader(new FileReader(file));
            String line;
            // 数据集根目录，用于拼接图片完整路径
            String datasetDir = file.getParent();

            while ((line = br.readLine()) != null) {
                if (line.startsWith("#")) continue;

                // 修复1: 支持逗号或空格/Tab分割
                String[] parts = line.split("[,\\s]+");

                if (parts.length >= 2) {
                    String timeStr = parts[0].trim();
                    String imgFileName = parts[1].trim();

                    double t = Double.parseDouble(timeStr);
                    // 修复2: 自动判断单位。如果时间戳数值很大（例如大于 1e12），通常是纳秒，否则认为是秒
                    double timestamp = (t > 1000000000000.0) ? t / 1e9 : t;

                    String fullPath = datasetDir + "/data/" + imgFileName;
                    // 注意：TUM数据集的图片路径通常就在 datasetDir/rgb/ 下，不需要再加 /data/
                    // 建议检查文件是否存在，或者根据数据集类型动态调整路径拼接逻辑
                    File imgFile = new File(datasetDir, imgFileName); // 尝试直接拼接
                    if(!imgFile.exists()) {
                        // 尝试 EuRoC 结构
                        imgFile = new File(datasetDir + "/data/" + imgFileName);
                    }

                    if (imgFile.exists()) {
                        mDatasetFrames.add(new FrameData(imgFile.getAbsolutePath(), timestamp));
                    }
                }
            }
            br.close();

            // 确保按时间顺序排序
            Collections.sort(mDatasetFrames, (o1, o2) -> Double.compare(o1.timestamp, o2.timestamp));

            Log.i(TAG, "Dataset loaded: " + mDatasetFrames.size() + " frames.");

        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, "Failed to load dataset: " + e.getMessage());
        }
    }

    private void startDatasetLoop() {
        // [新增修复]：防止多个线程同时启动导致 C++ 底层内存被多线程踩踏
        if (mIsRunningDataset) {
            Log.w(TAG, "警告：数据集线程已在运行，拦截重复启动！");
            return;
        }

        // 先设置标志位为 true，再启动线程，确保只进一次
        mIsRunningDataset = true;

        // [新增修复]：强制停掉实况相机 Handler，防止相机数据和数据集数据打架
        mHandler.removeCallbacks(mRunTrack);

        new Thread(() -> {
            // 1. 加载数据集路径
            File sdcard = android.os.Environment.getExternalStorageDirectory();
            File datasetIndexFile = new File(sdcard, "SLAM/dataset/cam0/data.csv");
            loadDataset(datasetIndexFile.getAbsolutePath());

            if (mDatasetFrames.isEmpty()) {
                runOnUiThread(() -> Toast.makeText(mContext, "未找到数据集", Toast.LENGTH_LONG).show());
                return;
            }
            Log.i(TAG, "Dataset loaded. Frames: " + mDatasetFrames.size());

            // 2. 触发初始化 (如果你还没点按钮，这里帮你点)
            if (mSystem == null) {
                Log.i(TAG, "Auto-triggering system initialization...");
                runOnUiThread(() -> new Thread(mTaskInitSystem).start());
            }

            // 3. 先设置标志位，再等待！
            mIsRunningDataset = true;

            Log.i(TAG, "Waiting for SLAM system ready...");
            // 循环等待，直到 mSystem 初始化完毕
            while (mIsRunningDataset) {
                // 检查 mSystem 是否可用，且状态为 run
                if (mSystem != null && "run".equals(mSystemStage)) {
                    Log.i(TAG, "SLAM system is ready! GO!");
                    break; // 初始化完成，跳出等待，开始跑图
                }
                try { Thread.sleep(500); } catch (Exception e) {}
            }

            // 开始遍历帧
            for (int i = 0; i < mDatasetFrames.size(); i++) {
                if (!mIsRunningDataset) break;

                FrameData frame = mDatasetFrames.get(i);

                // 等待系统初始化
                // 如果 mSystem 还没初始化好 (mSystemStage)，可能需要等待或跳过
                if (mSystem != null && "run".equals(mSystemStage)) {
                    // 读取图片
                    Bitmap bitmap = BitmapFactory.decodeFile(frame.imagePath);
                    if (bitmap == null) {
                        Log.e(TAG, "Decode image failed: " + frame.imagePath);
                        continue;
                    }
                    // 调用核心算法 (这里使用纯视觉接口，如果需要IMU需要另外处理)
                    // [注意] 使用数据集的时间戳 frame.timestamp
                    mSystem.TrackingMono(bitmap, frame.timestamp);

                    // 4. 更新UI (必须回到主线程)
                    final Bitmap drawBmp = bitmap; // 指向被C++画过特征点的图(如果C++里修改了)或者原图
                    runOnUiThread(() -> {
                        // 更新 GLSurfaceView (地图)
                        mMapRender.setCameraMatrix(mSystem.mPose);
                        mMapRender.setCoords(mSystem.mMapPoints);
                        glSurfaceView.requestRender();

                        // 更新左下角相机预览
                        ImageView imageView = findViewById(R.id.SLAM_IMG_CAM);
                        imageView.setImageBitmap(drawBmp);

                        ((TextView)findViewById(R.id.SLAM_MESSAGE)).setText(mSystem.getTrackingStateStringCN());
                        ((TextView)findViewById(R.id.SLAM_STATE)).setText("Frame: " + frame.timestamp);
                    });
                }

                // 5. 控制播放速度 (可选)
                // 如果跑得太快，可以加一点 sleep
                try {
                    Thread.sleep(30); // 约30fps
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }

}

class FpsCounter
{

    long mStartSec = 0;
    int mCount = 0;
    int mFpsLast = 0;

    int update()
    {
        long _sec = System.currentTimeMillis()/1000;
        mCount ++ ;

        if(_sec!=mStartSec)
        {
            mStartSec = _sec;
            mFpsLast = mCount;
            mCount = 0;
        }

        return  mFpsLast;

    }


}

// 用于保存每一帧的数据结构
class FrameData {
    String imagePath;
    double timestamp;

    public FrameData(String path, double time) {
        this.imagePath = path;
        this.timestamp = time;
    }
}