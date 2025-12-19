//
// Created by xiaow on 2025/9/7.
//

#include <iostream>
#include <Location.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>



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

    m_markerPose_Cockpit = glm::make_mat4(new float[16]{
            1.0,0,0,0,
            0,0,1.0,0,
            0,-1.0,0,0,
            0.278194919,-0.117810422,0.515670925,1.00000000});

    // 旋转角度
    float angleX = glm::radians(-90.0f);
    float angleY = glm::radians(180.0f);
    float angleZ = glm::radians(0.0f);

    // 分别绕各轴旋转
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), angleZ, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 rotationMatrix = rotationX * rotationY * rotationZ;
    //rotationMatrix作用得更早，因此应该放在右边
    m_markerPose_Cockpit = m_markerPose_Cockpit * rotationMatrix;
    alignTransMap2CockpitFile = appData.dataDir + "CameraTracking/anchorMap2Cockpit.txt";
    m_alignTrajectoryCache.clear();
    if(!appData.isLoadMap)
        m_lastAlignMatrix = glm::mat4(1.0f);
    else {
        std::ifstream in(alignTransMap2CockpitFile);
        float* pMat = glm::value_ptr(m_lastAlignMatrix);
        for (int i = 0; i < 16; ++i) {
            if (!(in >> pMat[i])) {
                break; // 读取错误或文件结束
            }
        }
        in.close();
    }
    return STATE_OK;
}

int Location::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){

    if(!appData.isLoadMap) {
        auto frame_data = std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
        // 相机(Head/Eye) 在 OpenXR 世界坐标系下的位姿
        // frame_data.cameraMat是视图矩阵，inv(frame_data.cameraMat)才是相机位姿矩阵
        glm::mat4 cameraPose_World = glm::inverse(frame_data.cameraMat);

        if (!frameDataPtr->image.empty()) {
            cv::Mat img = frameDataPtr->image.front(); //相机图像
            const cv::Matx33f &cameraIntrinsics = frameDataPtr->colorCameraMatrix; //相机内参
            cv::Vec3d rvec, tvec;
            if (m_arucoDetector.detect(img, cameraIntrinsics, rvec, tvec)) {
                // 获取 Marker 在相机坐标系下的位姿 (OpenCV 格式)
                cv::Mat markerPose_Camera_CV = RT2Matrix(rvec, tvec);
                // 转换为 GLM 格式 (同时修正坐标轴方向)
                glm::mat4 markerPose_Camera_GLM = CVPose2GLMPoseMat(markerPose_Camera_CV);
                // 计算 Marker 在 OpenXR 世界坐标系下的实际位姿
                glm::mat4 markerPose_World = cameraPose_World * markerPose_Camera_GLM;
                if (glm::determinant(markerPose_World) != 0.0f) {
                    m_worldAlignMatrix = m_markerPose_Cockpit * glm::inverse(markerPose_World);
                }
            }

        }

        std::shared_lock<std::shared_mutex> _lock(m_dataMutex);
        static bool isFirstUpdate = true;
        if (isFirstUpdate) {
            m_lastAlignMatrix = m_worldAlignMatrix;
            isFirstUpdate = false;
        }
        if (m_alignTrajectoryCache.empty()) {
            m_alignTrajectoryCache = GenerateInterpolatedPath(m_lastAlignMatrix, m_worldAlignMatrix,
                                                              30);
            m_lastAlignMatrix = m_worldAlignMatrix; // 更新基准
        }

        glm::mat4 currentSmoothedAlign = m_alignTrajectoryCache.back();
        m_alignTrajectoryCache.pop_back();
        frameDataPtr->alignTransMap2Cockpit = currentSmoothedAlign;
    }
    else {
        frameDataPtr->alignTransMap2Cockpit = m_lastAlignMatrix;
    }

    return STATE_OK;
}

int Location::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){

    return STATE_OK;
}

int Location::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int Location::ShutDown(AppData &appData,SceneData &sceneData){

    if(!appData.isLoadMap && !appData.isOnlyUseMarkerLocation) {
        std::string outfile = appData.dataDir + "CameraTracking/alignTransMap2CockpitFile.txt";
        std::ofstream out(outfile);

        if (out.is_open()) {
            const float *pSource = glm::value_ptr(m_lastAlignMatrix);
            for (int i = 0; i < 16; ++i) {
                out << pSource[i] << " ";
            }
            out.close();
        }
    }
    return STATE_OK;
}