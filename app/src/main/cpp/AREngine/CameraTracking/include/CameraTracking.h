//
// Created by Zhang Yidi on 2023/9/14.
//

#ifndef ARENGINE_CAMERATRACKING_H
#define ARENGINE_CAMERATRACKING_H
#include "opencv3_definition.h"
#include "opencv2/opencv.hpp"
#include "ARModule.h"
#include "BasicData.h"
#include "ConfigLoader.h"
#include "App.h"

#include <memory>

/**
 * CameraTracking：相机跟踪类，继承于算法基类
 * 功能：对每一帧相机进行跟踪
 * 输入：RGBD图像及IMU
 * 输出：每帧相机的位姿，存放到SceneData::cameraPose
 */

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <opencv2/opencv.hpp>
#include "glm/glm.hpp"


class CameraTracking: public ARModule {
public:
     CameraTracking();
     ~CameraTracking() override;

    void PreCompute(std::string configPath) override;

    int Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) override;

    int Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) override;
    
    int CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr) override;

    int ProRemoteReturn(RemoteProcPtr proc) override;

    int ShutDown(AppData& appData,  SceneData& sceneData) override;
private:


    std::string alignTransformLastFile;

    cv::Mat alignTransform; // SLAM -> Reloc
    cv::Mat alignTransformLast; // SLAM -> Reloc
    cv::Mat selfSlamPose;


    std::string offline_data_dir;
    glm::mat4 m_worldAlignMatrix;
    glm::mat4 m_lastAlignMatrix;
    std::vector<glm::mat4> m_alignTrajectoryCache;
    std::shared_mutex m_dataMutex;
};

// #include "CameraTracking.cpp"
#endif //ARENGINE_CAMERATRACKING_H
