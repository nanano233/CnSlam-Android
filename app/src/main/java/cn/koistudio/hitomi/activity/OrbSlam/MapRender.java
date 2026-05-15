package cn.koistudio.hitomi.activity.OrbSlam;

import static java.lang.Math.atan2;
import static java.lang.Math.cos;
import static java.lang.Math.sin;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import cn.koistudio.hitomi.util.uGLES;

public class MapRender implements GLSurfaceView.Renderer{

    static final String TAG = "MapRender";

    // GLES Entry
    private int mHandler_vertex   = 0;
    private int mHandler_fragment = 0;
    private int mHandler_program  = 0;

    private int degree = 0;

    // GLES Handler
    private int hPosition = 0;
    private int hMatrix   = 0;
    private int hColor    = 0;

    // Matrix
    private float[] mMatrix_Camera     = new float[16];
    private float[] mMatrix_Projection = new float[16];
    private float[] mMatrix_Vertex     = new float[16];
    private double  mRy = 0 ;
    private double  mMx = 0 ;
    private double  mMy = 0 ;
    private double  mMz = 0 ;

    public int sightDist = 4 ;
    public void switchDist(int param)
    {
        if(param>0) {
            sightDist = sightDist * 2;
            if(sightDist>64)
                sightDist = 64;
        }
        else if(param<0) {
            sightDist = sightDist / 2;
            if(sightDist<1)
                sightDist = 1;
        }
        else
            sightDist = 4;

    }

    //
    public boolean sightFollow = true ;
    public void switchFollow(int param)
    {
        if(param>0) {
            sightFollow = true;
        }
        else if(param<0) {
            sightFollow = false;
        }
        else
            sightFollow = !sightFollow;
    }

    // 尾随视角切换
    public boolean sightTop = true ;
    public void switchTop(int param)
    {
        if(param>0) {
            sightTop = true;
        }
        else if(param<0) {
            sightTop = false;
        }
        else
            sightTop = !sightTop;

        if(!sightTop)
            sightDist = 1;
    }


    public void setCameraMatrix(float[] matrix)
    {



        mMx = matrix[3];
        mMz = matrix[11];
        mMy = matrix[7];

        // 相机前向在 XZ 平面的 yaw 角：atan2(forward_x, forward_z)
        mRy = atan2(matrix[2], matrix[10]);

        float zoom = 1.0f / sightDist;

        if(sightTop)
        {

            // TODO: 头顶视角（sightDist缩放视锥体，不移动相机）
            Matrix.frustumM(mMatrix_Projection,0,
                -scaleRate * 0.5f * zoom, scaleRate * 0.5f * zoom,
                -scaleRate * 0.5f * zoom, scaleRate * 0.5f * zoom,
                (float) 0.1, 500);


            // TODO: 设置观察原点
            if (sightFollow) {

                // TODO: up 向量 XZ 分量 = 相机前向，屏幕 +Y 跟随相机朝向
                Matrix.setLookAtM(
                        mMatrix_Camera, 0,
                        matrix[3], -3f, matrix[11],
                        matrix[3], 0.0001f, matrix[11],
                        0.001f * (float) sin(mRy), 2.0f, 0.001f * (float) cos(mRy));
            } else {
                Matrix.setLookAtM(
                        mMatrix_Camera, 0,
                        matrix[3], -3f, matrix[11],
                        matrix[3], 0.0001f, matrix[11],
                        0.0000f, 2.0f, 0.0001f);
            }
        }
        else
        {
            // TODO: 尾行视角
            Matrix.frustumM(mMatrix_Projection,0,
                -scaleRate * 0.5f * zoom, scaleRate * 0.5f * zoom,
                -scaleRate * 0.5f * zoom, scaleRate * 0.5f * zoom,
                (float) 0.1f, 500f);

            Matrix.setLookAtM(
                mMatrix_Camera, 0,
                matrix[3] - 0.1f * (float)sin(mRy), matrix[7] + 0.5f, matrix[11] - 0.1f * (float)cos(mRy),
                matrix[3], matrix[7], matrix[11],
                0f, 1.0f, 0f);


        }

    }


    // Debug 变量


    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig eglConfig)
    {
        Log.i(TAG,"onSurfaceCreated");

        GLES20.glClearColor(0.0f,0.0f,0.0f,0.0f);
        GLES20.glEnable(GLES20.GL_DEPTH_TEST);

        // TODO: Prepare Program
        mHandler_vertex   = uGLES.loadShader(GLES20.GL_VERTEX_SHADER,uGLES.Default_VertexShaderCode_3D);
        mHandler_fragment = uGLES.loadShader(GLES20.GL_FRAGMENT_SHADER,uGLES.Default_FragmentShaderCode);
        mHandler_program  = uGLES.createProgram(mHandler_vertex,mHandler_fragment);

        // TODO: Prepare Handler
        hPosition = GLES20.glGetAttribLocation(mHandler_program, "vPosition");
        hMatrix   = GLES20.glGetUniformLocation(mHandler_program, "vMatrix");
        hColor    = GLES20.glGetUniformLocation(mHandler_program, "vColor");

    }

    private float scaleRate = 1.0f;
    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        Log.i(TAG,"onSurfaceChanged "+" width="+width + " height="+height);

        GLES20.glViewport(0,0,width,height);
        float rate= width / (float)height;
        scaleRate = rate;

        //Matrix.orthoM(mMatrix_Projection,0,-rate * 1.0f,rate * 1.0f,rate * -1.0f,rate * 1.0f,(float) 0.5f,10);
        //Matrix.orthoM(mMatrix_Projection,0,-rate *0.5f,rate * 0.5f,rate * -0.5f,rate * 0.5f,(float) 0.4,10);
        Matrix.frustumM(mMatrix_Projection,0,-rate *0.5f,rate * 0.5f,rate * -0.5f,rate * 0.5f,(float) 0.1,500);
        Matrix.setLookAtM(mMatrix_Camera,0,0.0001f,-3f,0.0001f,0.0001f,0.0001f,0.0001f,0f,2.0f,0.0001f);
        // Matrix.setLookAtM(mMatrix_Camera,0,0.0001f,0.001f,0,0.001f,0.001f,1f,0.1f,0.001f,1);
        printfMatrix44(mMatrix_Camera);

        Matrix.multiplyMM(mMatrix_Vertex,0,mMatrix_Projection,0,mMatrix_Camera,0);
    }

    public static void printfMatrix44(float[] mx)
    {
        if(mx.length!=16)
            return;

        Log.i(TAG,"Matrix44=");
        for(int r=0;r<4;r++)
        {
            Log.i(TAG," "+mx[4*r+0] + " , " + mx[4*r+1] + " , " + mx[4*r+2] + " , " + mx[4*r+3]);
        }

    }

    public void setCoords(float[] coords)
    {
        this.mCoords = coords;
    }

    // 轨迹数据 — 来自当前激活地图的全部关键帧位姿
    private volatile float[] mTrajectory = null;
    private volatile int mTrajCount = 0;

    public void setTrajectory(float[] traj) {
        // 只有新轨迹包含至少2个关键帧时才更新，防止重初始化后轨迹消失
        if (traj != null && traj.length >= 6) {
            mTrajectory = traj;
            mTrajCount = traj.length / 3;
        }
    }

    public void addTrajectoryPoint(float x, float y, float z) {
        // 保留兼容，不再使用
    }


    float mCoords[] = {
            -1.0f,   0.5f,  -1.0f,
            -1.0f,   0.5f,   1.0f,
            -1.0f,   0.5f,  -1.0f,
             1.0f,   0.5f,  -1.0f,
             1.0f,   0.5f,   1.0f,
             1.0f,   0.5f,  -1.0f,
             1.0f,   0.5f,   1.0f,
            -1.0f,   0.5f,   1.0f,
    };


    @Override
    public void onDrawFrame(GL10 gl) {



        Log.i(TAG,"onDrawFrame");
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT| GLES20.GL_DEPTH_BUFFER_BIT);
        //GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        GLES20.glUseProgram(mHandler_program);
        GLES20.glEnableVertexAttribArray(hPosition);

        // Debug Cycle
        // Matrix.setLookAtM(mMatrix_Camera,0, 0.00f,0.01f, -0.1f,0,0,1, 0,0,1);
//        mMatrix_Camera = new float[]{
//                1.0f,0.0f,0.0f,0.0f,
//                0.0f,1.0f,0.0f,0.0f,
//                0.0f,0.0f,-1.0f,0.0f,
//                0.0f,0.0f,0.0f,1.0f};

        // printfMatrix44(mMatrix_Camera);
        Matrix.multiplyMM(mMatrix_Vertex,0,mMatrix_Projection,0,mMatrix_Camera,0);



        GLES20.glUniformMatrix4fv(hMatrix,1,false,mMatrix_Vertex,0);


        // Debug Var Start
/*
        float vColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

        // TODO: 处理缩放
        if(sightTop)
        {
            for (int j = 0; j < mCoords.length; j++) {
                mCoords[j] = mCoords[j] / sightDist;
            }
        }
        else
        {
            for (int j = 0; j < mCoords.length; j++) {
                if(j%3==0)
                    mCoords[j] = -mCoords[j];
            }
        }
*/


        float vColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

        if (mCoords.length < 3 * 3) {
            mCoords = new float[]{
                    -1.0f, 0.5f, -1.0f,
                    -1.0f, 0.5f, 1.0f,
                    -1.0f, 0.5f, -1.0f,
            };
        } else {

            if(sightTop) {
                float _sin = 0;
                float _cos = 1;

                //            if(!sightFollow) {
                //                _sin = (float) sin(-mRy);
                //                _cos = (float) cos(-mRy);
                //            }

                // 三角形在屏幕以上 1:1 速率跟随场景旋转（与相机前向反向）
                // 推导：三角形世界方向 = (cos(2*mRy), -sin(2*mRy)) → 屏幕角度 = -mRy
                _sin = (float) cos(2 * mRy);
                _cos = (float) -sin(2 * mRy);

                // TODO: 指示方向的三角形
                float ts = 0.3f / sightDist;
                float _h1 = ts;
                float _h2 = ts * 0.5f;

                mCoords[0] = (_h1 * _sin + (float) mMx);
                mCoords[1] = (0.05f);
                mCoords[2] = (_h1 * _cos + (float) mMz);

                mCoords[3] = (_h2 * _cos + (float) mMx);
                mCoords[4] = (0.05f);
                mCoords[5] = (_h2 * (-_sin) + (float) mMz);

                mCoords[6] = (_h2 * (-_cos) + (float) mMx);
                mCoords[7] = (0.05f);
                mCoords[8] = (_h2 * _sin + (float) mMz);
            }
        }


        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(mCoords.length * 4);
        byteBuffer.order(ByteOrder.nativeOrder());
        FloatBuffer vertexBuffer = byteBuffer.asFloatBuffer();
        vertexBuffer.put(mCoords);
        vertexBuffer.position(0);
        // Debug Var End

        GLES20.glUniform4fv(hColor, 1, vColor,0);

        GLES20.glVertexAttribPointer(hPosition, 3, GLES20.GL_FLOAT, false, 12, vertexBuffer);

        // 绘制方向三角形 + 地图点云
        if(sightTop)
            GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, (3));
        GLES20.glDrawArrays(GLES20.GL_POINTS, 3, (mCoords.length/3-3));

        // 绘制轨迹（绿色）— 来自当前激活地图的关键帧位姿
        float[] trajSnap = mTrajectory; // 捕获 volatile 引用
        int trajCount = mTrajCount;
        if (trajSnap != null && trajCount >= 2) {
            ByteBuffer bb = ByteBuffer.allocateDirect(trajCount * 3 * 4);
            bb.order(ByteOrder.nativeOrder());
            FloatBuffer fb = bb.asFloatBuffer();
            fb.put(trajSnap, 0, trajCount * 3);
            fb.position(0);

            float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
            GLES20.glUniform4fv(hColor, 1, green, 0);
            GLES20.glUniformMatrix4fv(hMatrix, 1, false, mMatrix_Vertex, 0);
            GLES20.glVertexAttribPointer(hPosition, 3, GLES20.GL_FLOAT, false, 12, fb);
            GLES20.glLineWidth(1.0f);
            GLES20.glDrawArrays(GLES20.GL_LINE_STRIP, 0, trajCount);
        }

        GLES20.glDisableVertexAttribArray(hPosition);



    }
}


