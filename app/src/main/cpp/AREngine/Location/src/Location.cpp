//
// Created by xiaow on 2025/9/7.
//

#include <iostream>
#include <Location.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <android/log.h>
#define LOG_TAG "Location.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)


#include <glm/gtc/quaternion.hpp>

void SaveTransformToFile(const std::string& filename, const glm::mat4& m) {
    // 1. 直接提取位移 (Column 3, Elements 0,1,2)
    glm::vec3 translation = glm::vec3(m[3]);

    // 2. 直接转换为四元数 (从旋转子阵提取)
    // 因为没有 scale，quat_cast 的结果是精准的单位四元数
    glm::quat rotation = glm::quat_cast(m);

    // 3. 写入文件
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << translation.x << " " << translation.y << " " << translation.z << " "
        << rotation.x << " " << rotation.y << " " << rotation.z << " " << rotation.w << "\n";

        outFile.close();
        printf("Saved to %s\n", filename.c_str());
    }
}


glm::mat4 LoadTransformFromFile(const std::string& filename) {
    glm::vec3 translation;
    glm::quat rotation;
    std::string dummy; // 用于跳过文件中的标签文字

    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        return glm::mat4(1.0f); // 失败则返回单位阵
    }

    inFile >> translation.x >> translation.y >> translation.z >> rotation.x >> rotation.y >> rotation.z >> rotation.w;

    inFile.close();

    // --- 重新合成矩阵 ---
    // 注意顺序：先旋转再平移（在矩阵乘法中，平移矩阵要在旋转矩阵左边）
    // M = T * R
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::mat4_cast(rotation);

    return T * R;
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




// 辅助函数：将矩阵转为一行字符串，方便在 Logcat 中查看
std::string MatrixToString(const glm::mat4& m) {
    std::stringstream ss;
    const float* p = glm::value_ptr(m);
    ss << std::fixed << std::setprecision(3);
    // 只打印平移部分 (last column) 和 旋转的迹，减少日志长度
    ss << "[T:(" << p[12] << "," << p[13] << "," << p[14] << ") ";
    ss << "R_row0:(" << p[0] << "," << p[4] << "," << p[8] << ")]";
    return ss.str();
}

// 如果你想看完整 4x4
void LogMatrixFull(const char* label, const glm::mat4& m) {
    const float* p = glm::value_ptr(m);
    LOGI("%s:\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]\n[%.3f %.3f %.3f %.3f]",
         label,
         p[0], p[4], p[8], p[12],
         p[1], p[5], p[9], p[13],
         p[2], p[6], p[10], p[14],
         p[3], p[7], p[11], p[15]);
}

// rvec/tvec 转 cv::Mat (4x4)
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



int Location::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    std::string dataDir = appData.dataDir;
    m_arucoDetector.loadTemplate(dataDir + "templ_2.json");
    m_arucoDetector2.loadTemplate(dataDir + "templ_3.json");

    m_markerPose_Cockpit = glm::mat4(1.0f);

    // 2. 手动设置旋转部分 (3x3) - 确保正交
    // 注意：glm[列][行]
    m_markerPose_Cockpit[0] = glm::vec4(1.0f,  0.0f,  0.0f, 0.0f);
    m_markerPose_Cockpit[1] = glm::vec4(0.0f,  0.0f, -1.0f, 0.0f); // 对应原数据 y,z 交换
    m_markerPose_Cockpit[2] = glm::vec4(0.0f,  1.0f,  0.0f, 0.0f);

    // 3. 设置位移部分 (第4列)
    m_markerPose_Cockpit[3] = glm::vec4(0.278194919f, -0.117810422f, 0.515670925f, 1.0f);

    // 4. 应用你的旋转修正 (保持正交性)
    float angleX = glm::radians(-90.0f);
    float angleY = glm::radians(180.0f);
    float angleZ = glm::radians(0.0f);
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1,0,0)) *
                               glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0,1,0)) *
                               glm::rotate(glm::mat4(1.0f), angleZ, glm::vec3(0,0,1));

    m_markerPose_Cockpit = m_markerPose_Cockpit * rotationMatrix;


    makerPoseInMapFile = appData.dataDir + "CameraTracking/makerPoseInMapFile.txt";
    markerPoseInGlass = glm::mat4(1.0);
    m_alignTrajectoryCache.clear();
    m_worldAlignMatrix = glm::mat4(1.0);
    markerPoseInMap = glm::mat4(1.0f);
    markerPoseInMap = glm::mat4(
            glm::vec4(-0.844f,  0.011f, -0.536f,  0.000f), // 第一列 (Column 0)
            glm::vec4(-0.531f, -0.163f,  0.832f,  0.000f), // 第二列 (Column 1)
            glm::vec4(-0.078f,  0.987f,  0.144f,  0.000f), // 第三列 (Column 2)
            glm::vec4(-0.146f, -0.702f, -0.760f,  1.000f)  // 第四列 (Column 3 - 平移)
    );
    if(!appData.isLoadMap)
        markerPoseInMap = glm::mat4(1.0f);
    else {
//        std::ifstream in(makerPoseInMapFile);
        markerPoseInMap = LoadTransformFromFile(makerPoseInMapFile);
    }

    float det3 = glm::determinant(markerPoseInMap);
    LOGI("markerPoseInMap_init=%.3f", det3);

    return STATE_OK;
}

int Location::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    static int logCounter = 0;
    bool shouldLog = (logCounter++ % 30 == 0);
//    if(!appData.isLoadMap) {
    auto frame_data = std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
    // 相机(Head/Eye) 在 OpenXR 世界坐标系下的位姿
    // frame_data.cameraMat是视图矩阵，inv(frame_data.cameraMat)才是相机位姿矩阵
    glm::mat4 cameraPoseInGlass = glm::inverse(frame_data.cameraMat);
    glm::mat4 alignTransG2M = frameDataPtr->alignTransG2M;
    bool isAlign = frameDataPtr->isAlign;


    if (!frameDataPtr->image.empty()) {
        cv::Mat img = frameDataPtr->image.front(); //相机图像
        const cv::Matx33f &cameraIntrinsics = frameDataPtr->colorCameraMatrix; //相机内参
        cv::Vec3d rvec, tvec;
        if (m_arucoDetector.detect(img, cameraIntrinsics, rvec, tvec)) {

            glm::mat4 markerPoseInCam = CVPose2GLMPoseMat(RT2Matrix(rvec, tvec));
            markerPoseInGlass = cameraPoseInGlass * markerPoseInCam;
            m_worldAlignMatrix = m_markerPose_Cockpit * glm::inverse(markerPoseInGlass);
            if(appData.isLoadMap && appData.updateMarkerPoseInMap && isAlign) {
                markerPoseInMap =  alignTransG2M * markerPoseInGlass;
                SaveTransformToFile(makerPoseInMapFile ,markerPoseInMap);
//                LogMatrixFull("Update markerPoseInMap=:",markerPoseInMap);
            }
        }
        else if (appData.isLoadMap && isAlign) {
            markerPoseInGlass = glm::inverse(alignTransG2M) * markerPoseInMap;
            m_worldAlignMatrix = m_markerPose_Cockpit * glm::inverse(markerPoseInGlass);
        }
        else {
            m_worldAlignMatrix = m_markerPose_Cockpit * glm::inverse(markerPoseInGlass);
        }
    }

    frameDataPtr->alignTrans = m_worldAlignMatrix;

    return STATE_OK;
}

int Location::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){

    return STATE_OK;
}

int Location::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int Location::ShutDown(AppData &appData,SceneData &sceneData){

    return STATE_OK;
}