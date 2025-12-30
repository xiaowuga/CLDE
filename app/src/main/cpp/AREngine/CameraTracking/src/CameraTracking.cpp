#include "CameraTracking.h"
#include "debug_utils.hpp"
#include "switch_input_pose.hpp"
//using namespace RvgCamTr;

#include <string>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <opencv2/core/eigen.hpp>
#include <unistd.h>
#include <thread>
#include "ARInput.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>  // for memcpy

#include <android/log.h>
#define LOG_TAG "CameraTracking.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)


void LogMatrixFull(const char* label, const glm::mat4& m) {
    const float* p = glm::value_ptr(m);
    LOGI("%s:\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]",
         label,
         p[0], p[4], p[8], p[12],
         p[1], p[5], p[9], p[13],
         p[2], p[6], p[10], p[14],
         p[3], p[7], p[11], p[15]);
}


namespace fs = std::filesystem;

CameraTracking::CameraTracking() = default;

CameraTracking::~CameraTracking() = default;

void clearDirectory(const std::string& dirPath) {
    // 检查目录是否存在
    if (fs::exists(dirPath)) {
        // 遍历目录中的所有文件和子目录
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            fs::remove_all(entry.path());  // 删除文件或目录
        }
    } else {
        // 如果目录不存在，可以选择创建目录（可选）
        fs::create_directories(dirPath);
    }
}



cv::Mat GLMPose2CVPoseMat(const glm::mat4& glmPose) {
    // 1. 构造 180度绕X轴的旋转矩阵 (坐标系翻转矩阵)
    // 此矩阵作用于世界坐标轴，用于将 OpenGL 空间映射回 OpenCV 空间
    // [ 1  0  0  0 ]
    // [ 0 -1  0  0 ]
    // [ 0  0 -1  0 ]
    // [ 0  0  0  1 ]
    glm::mat4 r_flip = glm::mat4(1.0f);
    r_flip[1][1] = -1.0f;
    r_flip[2][2] = -1.0f;

    // 2. 轴向修正
    // 逻辑：T_cv = r_flip * T_gl * r_flip
    // 左乘 r_flip 修正世界空间的 Y/Z 轴方向（影响平移）
    // 右乘 r_flip 修正相机局部坐标系的观察轴方向（旋转相机朝向）
    glm::mat4 cvPoseGLM = r_flip * glmPose * r_flip;

    // 3. 将 GLM (Column-Major) 转换为 OpenCV (Row-Major)
    cv::Mat cvMat(4, 4, CV_32F);

    // glm::value_ptr 返回列主序内存地址
    // 如果我们按顺序 memcpy 到 cv::Mat，它会被视为行主序，导致产生转置矩阵
    std::memcpy(cvMat.data, glm::value_ptr(cvPoseGLM), 16 * sizeof(float));

    // 显式转置，恢复成正确的行主序 4x4 矩阵
    // 使用 .clone() 确保数据内存连续
    return cvMat.t();
}


//glm::mat4 CVPose2GLMPoseMat(cv::Mat cvMat) {
//    // 1. 确保输入是 float 类型，因为 GLM 默认是 float
//    cv::Mat cvMatFloat;
//    cvMat.convertTo(cvMatFloat, CV_32F);
//
//    // 2. 基础转换：OpenCV数据 -> GLM数据 (此时产生转置)
//    glm::mat4 glmMat = glm::make_mat4(cvMatFloat.ptr<float>());
//    glmMat = glm::transpose(glmMat);
//
//    // 3. 坐标系修正：OpenCV (Y-Down) -> OpenGL (Y-Up)
//    // 修正矩阵：绕X轴旋转180度 (Scale 1, -1, -1)
//    glm::mat4 yz_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
//
//    // 4. 应用修正 (左乘)
//    return yz_flip * glmMat;
//}


glm::mat4 CVPose2GLMPoseMat(const cv::Mat& cvMat) {
    if (cvMat.empty() || cvMat.cols != 4 || cvMat.rows != 4) {
        return glm::mat4(1.0f);
    }

    cv::Mat cvMatFloat;
    cvMat.convertTo(cvMatFloat, CV_32F);

    // 1. 处理内存布局 (Row-Major -> Column-Major)
    // 直接读取并转置，得到 OpenCV 坐标系下的 GLM 矩阵
    glm::mat4 glmMat = glm::transpose(glm::make_mat4(cvMatFloat.ptr<float>()));

    // 2. 构造 180度绕X轴的旋转矩阵 (坐标系翻转矩阵)
    // 该矩阵是自逆的 (Self-inverse)，且行列式严格为 1
    // [ 1  0  0  0 ]
    // [ 0 -1  0  0 ]
    // [ 0  0 -1  0 ]
    // [ 0  0  0  1 ]
    glm::mat4 r_flip = glm::mat4(1.0f);
    r_flip[1][1] = -1.0f;
    r_flip[2][2] = -1.0f;

    // 3. 应用变换
    // 左乘 r_flip: 翻转世界坐标系的 Y 和 Z 轴 (改变平移方向)
    // 右乘 r_flip: 翻转相机自身的观察轴基向量 (改变旋转朝向)
    glm::mat4 result = r_flip * glmMat * r_flip;

    // 4. 正交规范化 (Gram-Schmidt)
    // 显式提取前三列（基向量），确保旋转矩阵是严格正交的
    glm::vec3 right = glm::normalize(glm::vec3(result[0]));
    glm::vec3 up    = glm::normalize(glm::vec3(result[1]));
    glm::vec3 forward = glm::cross(right, up); // 重新计算第三轴保证垂直
    up = glm::cross(forward, right);          // 再次微调上轴保证严格正交

    result[0] = glm::vec4(right,   0.0f);
    result[1] = glm::vec4(up,      0.0f);
    result[2] = glm::vec4(forward, 0.0f);

    return result;
}



cv::Mat RT2Matrix(const cv::Vec3d &rvec, const cv::Vec3d &tvec) {
    if (std::isinf(tvec[0]) || std::isnan(tvec[0]) || std::isinf(rvec[0]) || std::isnan(rvec[0]))
        return cv::Mat::eye(4, 4, CV_64F);

    cv::Matx33d R;
    cv::Rodrigues(rvec, R);
    cv::Mat T = cv::Mat::eye(4, 4, CV_64F);
    cv::Mat(R).copyTo(T(cv::Rect(0, 0, 3, 3)));
    T.at<double>(0, 3) = tvec[0];
    T.at<double>(1, 3) = tvec[1];
    T.at<double>(2, 3) = tvec[2];
    return T;
}


void saveFrameDataWithPose(const std::string& offlineDataDir,
                           double timestamp,
                           const cv::Mat& imgColorBuffer,
                           cv::Mat pose) {

    std::string camDir = offlineDataDir + "/cam0/";
    std::string rgbDir = camDir + "data/";
    // 如果目录不存在则创建
    fs::create_directories(camDir);
    fs::create_directories(rgbDir);

    // 格式化时间戳为字符串
    std::stringstream ss;
    // timestamp
    ss << std::fixed << std::setprecision(6) << timestamp;
    std::string timestampStr = ss.str();

    std::string rgbFile = timestampStr + ".png";

    // 构造文件路径
    std::string rgbFilePath = rgbDir + rgbFile;
    // 保存RGB图像
    if (!cv::imwrite(rgbFilePath, imgColorBuffer)) {
        return;
    }


    // 保存文件路径到文本文件
    std::ofstream posesFile1(camDir + "/cam_timestamp.txt", std::ios::app);
    if (posesFile1.is_open()) {
        posesFile1 << timestampStr << "\n";
        posesFile1.close();
    }

    std::ofstream posesFile2(camDir + "/data.csv", std::ios::app);
    if (posesFile2.is_open()) {
        posesFile2 << timestampStr << "," << rgbFile << "\n";
        posesFile2.close();
    }

    std::ofstream posesFile3(camDir + "/pose.txt", std::ios::app);
    if (posesFile3.is_open()) {
        std::string res = get_tum_string(timestamp, pose);
        posesFile3 << res;
        posesFile3.close();
    }
}



std::vector<glm::mat4> GenerateInterpolatedPath(const glm::mat4& startMat, const glm::mat4& endMat, int steps) {
    std::vector<glm::mat4> path;

    // 保护措施：如果步数太少，直接返回终点或起点
    if (steps <= 0) return path;
    if (steps == 1) {
        path.push_back(startMat);
        return path;
    }

    // 预分配内存，防止频繁 realloc
    path.reserve(steps);

    // -------------------------------------------------
    // 1. 提取基础数据 (分解)
    // -------------------------------------------------
    // 提取位移 (GLM 第4列对应内存最后4个数，即你的最后一行)
    glm::vec3 p0 = glm::vec3(startMat[3]);
    glm::vec3 p1 = glm::vec3(endMat[3]);

    // 提取旋转 (转为四元数)
    glm::quat q0 = glm::quat_cast(startMat);
    glm::quat q1 = glm::quat_cast(endMat);

    // -------------------------------------------------
    // 2. 循环生成插值
    // -------------------------------------------------
    for (int i = 0; i < steps; ++i) {
        // 计算当前进度 t (从 0.0 到 1.0)
        // i=0 时 t=0 (起点)
        // i=steps-1 时 t=1 (终点)
        float t = (float)i / (float)(steps - 1);

        // A. 位置线性插值 (LERP)
        glm::vec3 pt = glm::mix(p0, p1, t);

        // B. 旋转球面插值 (SLERP)
        // GLM 的 slerp 会自动处理最短路径
        glm::quat qt = glm::slerp(q0, q1, t);

        // C. 重组矩阵
        glm::mat4 mat = glm::mat4_cast(qt); // 旋转部分
        mat[3] = glm::vec4(pt, 1.0f);       // 位移部分 (填入最后4个float)

        path.push_back(mat);
    }
    std::reverse(path.begin(), path.end());

    return path;
}



int CameraTracking::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {

    SerilizedObjs initCmdSend = {
        {"cmd", std::string("init")},
        {"isLoadMap", int(appData.isLoadMap)},
        {"isSaveMap", int(appData.isSaveMap)},
        {"sensorType", 0} // 0=MONOCULAR, 2=RGBD
    };
    app->postRemoteCall(this, nullptr, initCmdSend);
//    arucoDetector.loadTemplate(appData.dataDir + "templ_2.json");

    worldAlignMatrix = glm::mat4(1.0);
    alignTrajectoryCache.clear();

    offlineDataDir = appData.offlineDataDir;
    trnasMap2MapRefFile = appData.dataDir + "CameraTracking/trnasMap2MapRefFile.txt";
    markerPoseInMapFile = appData.dataDir + "CameraTracking/markerPoseInMapFile.txt";
    transLocal2MapRef = glm::mat4(1.0);
    if(appData.isCaptureOfflineData) {
        clearDirectory(offlineDataDir);
    }

    if(appData.isLoadMap) {
        //读取锚点相机的位姿(文件是OpenCV格式的位姿）
        std::ifstream in1(trnasMap2MapRefFile);
        cv::Mat tmp = cv::Mat::eye(4, 4, CV_32F);
        for (int i = 0; i < 4; i++){
            for (int j = 0; j < 4; j++) {
                in1 >> tmp.at<float>(i, j);
            }
        }
        in1.close();
        trnasMapRef2Map = glm::inverse(CVPose2GLMPoseMat(tmp));

        // 读取marker的地图位姿(文件是glm格式的位姿）
        std::ifstream in2(markerPoseInMapFile);
        bool isLoaded = false;

        if (in2.is_open()) {
            float* pMat = glm::value_ptr(markerPoseInMap);
            int count = 0;
            for (; count < 16; ++count) {
                if (!(in2 >> pMat[count])) {
                    break; // 读取中断（文件结束或格式错误）
                }
            }
            in2.close();
            if (count == 16) {
                isLoaded = true;
            }
        }
        if (!isLoaded) {
            markerPoseInMap = glm::mat4(1.0f); // GLM 创建单位阵的方法
        }

    }

    isAlign = false;

    markerPoseInCockpit = glm::make_mat4(new float[16]{
            1.0,0,0,0,
            0,0,1.0,0,
            0,-1.0,0,0,
            0.278194919,-0.117810422,0.515670925,1.0});

    float angleX = glm::radians(-90.0f);
    float angleY = glm::radians(180.0f);
    float angleZ = glm::radians(0.0f);
    // 分别绕各轴旋转
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), angleZ, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 rotationMatrix = rotationX * rotationY * rotationZ;

    markerPoseInCockpit = markerPoseInCockpit * rotationMatrix;


    return STATE_OK;
}


int CameraTracking::Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {
    static bool firstUpdate = true;
    if(firstUpdate){
        firstUpdate = false;
        std::cout << "waiting for remote init" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        std::cout << "waiting done" << std::endl;
    }


    auto frame_data = std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
    cameraPoseInLocal = glm::inverse(frame_data.cameraMat);
    frameDataPtr->alignTransG2M = trnasMapRef2Map * transLocal2MapRef;
    frameDataPtr->isAlign = isAlign;

    if(appData.isCaptureOfflineData) {
        cv::Mat pose = GLMPose2CVPoseMat(cameraPoseInLocal);
        double timestamp = frameDataPtr->timestamp;
        cv::Mat img = frameDataPtr->image.front();
        saveFrameDataWithPose(offlineDataDir, timestamp, img, pose);
    }
    return STATE_OK;
}

void CameraTracking::PreCompute(std::string configPath) {
    return;
}

#include <string>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <opencv2/core/eigen.hpp>
#include <unistd.h>

//需要检测时，通过RPC调用服务器上的检测功能
int CameraTracking::CollectRemoteProcs(SerilizedFrame& serilizedFrame,
                                       std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr) {

    serilizedFrame.addRGBImage(*frameDataPtr);  //添加RGB图像到serilizedFrame。serilizedFrame随后将被上传到服务器。
    // add slam_pose
    cv::Mat cameraPoseInLocal_CV = GLMPose2CVPoseMat(cameraPoseInLocal);
    SerilizedObjs send = {
        {"cmd", std::string("reloc")},
        {"slam_pose", Bytes(cameraPoseInLocal_CV)}, //继续添加其它数据
        {"tframe", Bytes(frameDataPtr->timestamp)}
    };

    // test if timestamp is correct
    static double last_tframe = 0;
    if (frameDataPtr->timestamp < last_tframe) {
        return STATE_ERROR;
    }
    last_tframe = frameDataPtr->timestamp;

    procs.push_back(std::make_shared<RemoteProc>(this, frameDataPtr, send, RPCF_SKIP_BUFFERED_FRAMES)); //添加到procs，随后该命令将被发送到ObjectTrackingServer进行处理
    return STATE_OK;
}

int CameraTracking::ProRemoteReturn(RemoteProcPtr proc){
    auto& send = proc->send;
    auto& ret = proc->ret;
    auto cmd = send.getd<std::string>("cmd");
    if (cmd == "reloc"){
        if(ret.getd<bool>("align_OK")){
            glm::mat tmp = CVPose2GLMPoseMat( ret.getd<cv::Mat>("alignTransform"));
            float det = glm::determinant(tmp);
            LOGI("transLocal2MapRefasdasd=%.3f", det);
            if (!std::isnan(det)) {
//                transLocal2MapRef = tmp;
                isAlign = true;
            }
        }
        else{
            transLocal2MapRef = glm::mat4(1.0);
        }

    }
    return STATE_OK;
}

int CameraTracking::ShutDown(AppData& appData,  SceneData& sceneData){
    // remote shutdown
    SerilizedObjs cmdsend = {
        {"cmd", std::string("shutdown")}
    };
    app->postRemoteCall(this, nullptr, cmdsend);
    std::cout << "client CameraTracking send shutdown to Server" << std::endl;
    
    return STATE_OK;
}