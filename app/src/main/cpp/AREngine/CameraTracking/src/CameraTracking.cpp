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

namespace fs = std::filesystem;

CameraTracking::CameraTracking() = default;

CameraTracking::~CameraTracking() = default;


// T_ab->T_ba
cv::Mat inv_T(cv::Mat pose){
    assert(pose.rows == 4 && pose.cols == 4);
    if (pose.type() != CV_32F)
    {
        pose.convertTo(pose, CV_32F);
    }
    cv::Mat r_inv = pose(cv::Rect(0, 0, 3, 3)).clone().t();
    cv::Mat t_inv = - r_inv * pose(cv::Rect(3, 0, 1, 3)).clone();
    cv::Mat T_inv = cv::Mat::eye(4, 4, CV_32F);
    r_inv.copyTo(T_inv(cv::Rect(0, 0, 3, 3)));
    t_inv.copyTo(T_inv(cv::Rect(3, 0, 1, 3)));
    return T_inv;
}

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



cv::Mat GLMPose2CVPoseMat(const glm::mat4& glmPose) {
    // 1. 轴向修正 (右乘)
    glm::mat4 axisFix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
    glm::mat4 cvPoseGLM = glmPose * axisFix;

    // 2. 转为 OpenCV Mat (注意转置问题)
    cv::Mat cvMat = cv::Mat(4, 4, CV_32F);
    // 这里利用 memcpy + transpose 技巧
    // GLM(Col) -> memcpy -> OpenCV(Row) = 结果是转置矩阵
    std::memcpy(cvMat.data, glm::value_ptr(cvPoseGLM), 16 * sizeof(float));

    // 3. 显式转置以获得正确的 Row-Major 矩阵
    return cvMat.t();
}

glm::mat4 CVPose2GLMPoseMat(cv::Mat cvMat) {
    // 1. 确保输入是 float 类型，因为 GLM 默认是 float
    cv::Mat cvMatFloat;
    cvMat.convertTo(cvMatFloat, CV_32F);

    // 2. 基础转换：OpenCV数据 -> GLM数据 (此时产生转置)
    glm::mat4 glmMat = glm::make_mat4(cvMatFloat.ptr<float>());
    glmMat = glm::transpose(glmMat);

    // 3. 坐标系修正：OpenCV (Y-Down) -> OpenGL (Y-Up)
    // 修正矩阵：绕X轴旋转180度 (Scale 1, -1, -1)
    glm::mat4 yz_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

    // 4. 应用修正 (左乘)
    return yz_flip * glmMat;
}


int CameraTracking::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {

    SerilizedObjs initCmdSend = {
        {"cmd", std::string("init")},
        {"isLoadMap", int(appData.isLoadMap)},
        {"isSaveMap", int(appData.isSaveMap)},
        {"sensorType", 0} // 0=MONOCULAR, 2=RGBD
    };
    app->postRemoteCall(this, nullptr, initCmdSend);

    alignTransform = cv::Mat::eye(4, 4, CV_32F);
    alignTransformLast = cv::Mat::eye(4, 4, CV_32F);
    selfSlamPose = cv::Mat::eye(4, 4, CV_32F);
    offline_data_dir = appData.offlineDataDir;
    alignTransformLastFile = appData.dataDir + "CameraTracking/anchorFile.txt";
    m_alignTrajectoryCache.clear();
    if(!appData.isLoadMap) {
        clearDirectory(offline_data_dir);
    }
    else {
        std::ifstream in(alignTransformLastFile);
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                in >> alignTransformLast.at<float>(i, j);
            }
        }
        in.close();
    }
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

    double timestamp = frameDataPtr->timestamp;
    cv::Mat img=frameDataPtr->image.front();
    auto frame_data=std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
    // 相机(Head/Eye) 在 OpenXR 世界坐标系下的位姿
    // frame_data.cameraMat是视图矩阵，inv(frame_data.cameraMat)才是相机位姿矩阵
    glm::mat4 cameraPose_World_glm = glm::inverse(frame_data.cameraMat);
    selfSlamPose = GLMPose2CVPoseMat(cameraPose_World_glm);
    if(!appData.isLoadMap) {
        saveFrameDataWithPose(offline_data_dir,timestamp, img, selfSlamPose);
        m_lastAlignMatrix = glm::mat4(1.0);
        frameDataPtr->alignTransTracking2Map = m_lastAlignMatrix;
    }
    else {

        cv::Mat worldAlignMatrix = alignTransformLast.inv() * alignTransform;
        m_worldAlignMatrix = CVPose2GLMPoseMat(worldAlignMatrix);


        frameDataPtr->alignTransTracking2Map = m_worldAlignMatrix;
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
int CameraTracking::CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr)
{

    serilizedFrame.addRGBImage(*frameDataPtr);  //添加RGB图像到serilizedFrame。serilizedFrame随后将被上传到服务器。

    // add slam_pose
    SerilizedObjs send = {
        {"cmd", std::string("reloc")},
        {"slam_pose", Bytes(selfSlamPose)}, //继续添加其它数据
        {"tframe", Bytes(frameDataPtr->timestamp)}
    };

    // test if timestamp is correct
    static double last_tframe = 0;
    if (frameDataPtr->timestamp < last_tframe)
    {
        return STATE_ERROR;
    }
    last_tframe = frameDataPtr->timestamp;

    procs.push_back(std::make_shared<RemoteProc>(this, frameDataPtr, send)); //添加到procs，随后该命令将被发送到ObjectTrackingServer进行处理
    return STATE_OK;
}

int CameraTracking::ProRemoteReturn(RemoteProcPtr proc){
    auto& send = proc->send;
    auto& ret = proc->ret;
    auto cmd = send.getd<std::string>("cmd");
    
    if (cmd == "reloc"){

        if(ret.getd<bool>("align_OK")){
            alignTransform = ret.getd<cv::Mat>("alignTransform");

        }
        else{
            alignTransform = cv::Mat::eye(4, 4, CV_32F);;
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