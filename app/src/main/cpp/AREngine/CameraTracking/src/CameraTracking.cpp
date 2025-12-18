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
<<<<<<< Updated upstream
=======
#include "ARInput.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <android/log.h>
#define LOG_TAG "CameraTracking.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
>>>>>>> Stashed changes


#include <cstring>  // for memcpy

cv::Matx44f convertMatToMatx(const cv::Mat& mat) {
    // 检查 cv::Mat 是否是 4x4 并且类型为 CV_32F
    if (mat.rows != 4 || mat.cols != 4 || mat.type() != CV_32F) {
        throw std::invalid_argument("Input Mat must be 4x4 with type CV_32F.");
    }

    // 创建一个 Matx44f 对象
    cv::Matx44f matx;

    // 使用 memcpy 快速复制数据
    std::memcpy(matx.val, mat.ptr<float>(), 16 * sizeof(float));

    return matx;
}


inline glm::mat4 CV_Matx44f_to_GLM_Mat4(const cv::Matx44f &mat) {  // 将 cv::Matx44f 转换为 glm::mat4
    return glm::mat4(
            mat(0, 0), mat(0, 1), mat(0, 2), mat(0, 3),  // 第一行
            mat(1, 0), mat(1, 1), mat(1, 2), mat(1, 3),  // 第二行
            mat(2, 0), mat(2, 1), mat(2, 2), mat(2, 3),  // 第三行
            mat(3, 0), mat(3, 1), mat(3, 2), mat(3, 3)   // 第四行
    );
}




std::vector<glm::mat4> interpolatePose(const glm::mat4& startMat, const glm::mat4& endMat, int steps) {
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


bool checkConstant(std::vector<cv::Mat> vAlignTransform){
    // std:: cout << "into checkConstant..." << std::endl;
    cv::Mat v_tmp = cv::Mat_<float>(vAlignTransform.size(), 1);
    for (int pos_index = 0 ;pos_index < 3; pos_index ++){
        for (int i = 0; i < 30 * 5; i++)
        {
            // if conclude nan, return false
            if (std::isnan(vAlignTransform[i].at<float>(pos_index, 3)))
            {
                return false;
            }

            v_tmp.at<float>(i, 0) = vAlignTransform[i].at<float>(pos_index, 3);
            // debug
            // std::cout << "v_tmp.at<float>(i, 0): " << v_tmp.at<float>(i, 0) << std::endl;
        }
        float var = cv::sum((v_tmp - cv::mean(v_tmp)).mul(v_tmp - cv::mean(v_tmp)))[0] / (30 * 5);
        // debug
        std::cout << "var: " << var << std::endl;
        if (var > 0.01){
            return false;
        }
    }
    return true;
}

<<<<<<< Updated upstream
int CameraTracking::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {

    // 服务器初始化参数传递
    std::string engineDir;
    try{
        engineDir=appData.dataDir;
    }catch(const std::bad_any_cast& e){
        std::cout << e.what() << std::endl;
    }
    std::string trackingConfigPath = engineDir + "CameraTracking/config.json";

    ConfigLoader crdr(trackingConfigPath);
    // 根据新增的 sensorType 配置决定是否使用深度
    {
        uint sensorType = crdr.getValue<int>("sensorType");  // 0=MONOCULAR, 2=RGBD
        SerilizedObjs initCmdSend = {
            {"cmd", std::string("init")},
            {"isLoadMap", int(appData.isLoadMap)},
            {"isSaveMap", int(appData.isSaveMap)},
            {"sensorType", int(sensorType)}
        };
        app->postRemoteCall(this, nullptr, initCmdSend);
    }

    hasImage = false;
    alignTransform = cv::Mat::eye(4, 4, CV_32F);
    alignTransformLast = cv::Mat::eye(4, 4, CV_32F);

    T_wc = cv::Mat::eye(4, 4, CV_32F);
=======

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




void saveFrameDataWithPose(const std::string& offline_output_path,
                           double timestamp,
                           const cv::Mat& imgColorBuffer,
                           cv::Mat pose) {

    std::string camDir = offline_output_path + "/cam0/";
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

    cv::Mat tmp = inv_T(pose);
    std::ofstream posesFile3(camDir + "/pose.txt", std::ios::app);
    if (posesFile3.is_open()) {
        std::string res = get_tum_string(timestamp, tmp);
        posesFile3 << res;
        posesFile3.close();
    }
}



int CameraTracking::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {

    alignTransformLastFile = appData.dataDir + "CameraTracking/alignTransformLastFile.txt";
    SerilizedObjs initCmdSend = {
        {"cmd", std::string("init")},
        {"isLoadMap", int(appData.isLoadMap)},
        {"isSaveMap", int(appData.isSaveMap)},
        {"sensorType", 0} // 0=MONOCULAR, 2=RGBD
    };
    app->postRemoteCall(this, nullptr, initCmdSend);

    alignTransform = cv::Mat::eye(4, 4, CV_32F);
    alignTransformLast = cv::Mat::eye(4, 4, CV_32F);

    if(appData.isLoadMap){
        std::ifstream in1(alignTransformLastFile);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                in1 >> alignTransformLast.at<float>(i, j);
            }
        }
        in1.close();

        std::ifstream in2(transformCGFile);
        for (int i = 0; i < 4; i++) {
            for(int j = 0; j <4; j++) {
                in2 >> transformCG[i][j];
            }
        }
        transformGC = glm::inverse(transformCG);
        in2.close();
    }


    selfSlamPose = cv::Mat::eye(4, 4, CV_32F);
    offline_output_path = appData.offlineDataDir;
    clearDirectory(offline_output_path);
    isInitDone = false;
>>>>>>> Stashed changes
    return STATE_OK;
}


int CameraTracking::Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {
    if(!isInitDone){
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        isInitDone = true;
    }

<<<<<<< Updated upstream
    time = frameDataPtr->timestamp;
//    std::cout << "time: " << time << std::endl;
    timeBuffer = time;

    if (true)
    {
        //CameraPose cp;
        auto cp = sceneData.getMainCamera();
        cp->timestamp = timeBuffer;


        T_wc = cv::Mat(sceneData.getMainCamera()->ARSelfPose);
        T_wc.convertTo(T_wc, CV_32F); // 确保 T_wc 是 CV_32F 类型
        T_wc = pose_swicher_ptr->getPose(use_online_pose, T_wc, time);


        cv::Mat T_wc_aligned; //T_wc 具有坐标变换意义的位姿
        if (!appData.isLoadMap) // 建图模式下，直接使用T_wc
        {
            T_wc_aligned = T_wc;
        }
        else // 运行模式下，需要进行对齐
        {
            while(true){            
                if (frameID2RelocPose.find(frameDataPtr->frameID) == frameID2RelocPose.end()){ 
                    usleep(1000*33); // wait for one frame
                }
                else{
                    // 一直等到到有值，所以不会为空
                    if (!appData.bUseRelocPose)
                        T_wc_aligned = alignTransformLast.inv() * alignTransform * T_wc;
                    else{
                        T_wc_aligned = alignTransformLast.inv() * frameID2RelocPose[frameDataPtr->frameID];
                    }
                    break;
                }
            }
        }
        
        auto T_cw = inv_T(T_wc_aligned);
        cp->transform.setPose(T_cw);
        glm::mat4 glm_alignTransform = CV_Matx44f_to_GLM_Mat4(alignTransform);
        frameDataPtr->viewRelocMatrix = glm_alignTransform;
        frameDataPtr->jointRelocMatrix = glm_alignTransform;
        frameDataPtr->modelRelocMatrix =  glm::mat4(1.0);

    }  //hasImage

=======
    auto cp = sceneData.getMainCamera();
    cv::Matx44d selfPose = cp->ARSelfPose;
    selfSlamPose = cv::Mat(selfPose);

    if(!appData.isLoadMap) {
        imgColor = frameDataPtr->image.front();
        saveFrameDataWithPose(offline_output_path, time, imgColor, selfSlamPose);
        transformCG = frameDataPtr->transformCG;
    }
    else {
        frameDataPtr->transformCG = transformCG;
        frameDataPtr->transformGC = transformGC;
        cv::Mat tmp = alignTransformLast.inv() * alignTransform;
        cv::Mat tmpT = tmp.t();
        frameDataPtr->relocMatrix = CV_Matx44f_to_GLM_Mat4(tmpT);
    }


>>>>>>> Stashed changes
    return STATE_OK;
}

void CameraTracking::PreCompute(std::string configPath) {
    return;
}


//需要检测时，通过RPC调用服务器上的检测功能
int CameraTracking::CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr)
{

<<<<<<< Updated upstream
    serilizedFrame.addRGBImage(*frameDataPtr);  //添加RGB图像到serilizedFrame。serilizedFrame随后将被上传到服务器。
    serilizedFrame.addDepthImage(*frameDataPtr);  

    // add slam_pose
    SerilizedObjs send = {
        {"cmd", std::string("reloc")},
        {"slam_pose", Bytes(T_wc)}, //继续添加其它数据
        {"tframe", Bytes(frameDataPtr->timestamp)}
    };

    // test if timestamp is correct
    static double last_tframe = 0;
    if (frameDataPtr->timestamp < last_tframe)
    {
        std::cerr << "tframe < last_tframe" << std::endl;
        return STATE_ERROR;
    }
    last_tframe = frameDataPtr->timestamp;

    procs.push_back(std::make_shared<RemoteProc>(this, frameDataPtr, send)); //添加到procs，随后该命令将被发送到ObjectTrackingServer进行处理
=======
//    serilizedFrame.addRGBImage(*frameDataPtr);
//
//    // add slam_pose
//    SerilizedObjs send = {
//        {"cmd", std::string("reloc")},
//        {"slam_pose", Bytes(selfSlamPose)}, //继续添加其它数据
//        {"tframe", Bytes(frameDataPtr->timestamp)}
//    };
//
//    // test if timestamp is correct
//    static double last_tframe = 0;
//    if (frameDataPtr->timestamp < last_tframe)
//    {
//        return STATE_ERROR;
//    }
//    last_tframe = frameDataPtr->timestamp;
//
//    procs.push_back(std::make_shared<RemoteProc>(this, frameDataPtr, send)); //添加到procs，随后该命令将被发送到ObjectTrackingServer进行处理
>>>>>>> Stashed changes
    return STATE_OK;
}

int CameraTracking::ProRemoteReturn(RemoteProcPtr proc){
    auto& send = proc->send;
    auto& ret = proc->ret;
    auto cmd = send.getd<std::string>("cmd");
    
    if (cmd == "reloc") {//处理reloc命令返回值
        if(ret.getd<bool>("align_OK")){
            alignTransform = ret.getd<cv::Mat>("alignTransform");

        }
        else{
            alignTransform = cv::Mat::eye(4, 4, CV_32F);;
        }
<<<<<<< Updated upstream
        // std::cout << "alignTransform: " << alignTransform << std::endl;
        frameID2RelocPose[ret.getd<int>("curFrameID")] = ret.getd<cv::Mat>("RelocPose");
        std::cout << "frameID : " << ret.getd<int>("curFrameID") << std::endl;
=======
>>>>>>> Stashed changes

    }
    return STATE_OK;
}

int CameraTracking::ShutDown(AppData& appData,  SceneData& sceneData) {
    if (!appData.isLoadMap) {
        std::ofstream out(transformCGFile, std::ios::out);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                out << transformCG[i][j] << " ";
            }
            out << std::endl;
        }
    }

    // remote shutdown
    SerilizedObjs cmdsend = {
        {"cmd", std::string("shutdown")}
    };
    app->postRemoteCall(this, nullptr, cmdsend);

    return STATE_OK;
}