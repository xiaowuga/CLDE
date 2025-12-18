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


class PoseSwicher;

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

public:
    bool isInitDone;
private:
    std::string alignTransformLastFile;
    std::string transformCGFile;



    cv::Mat imgColor;
    cv::Mat imgColorBuffer; // input rgb clone
    double time;

    cv::Mat alignTransform; // SLAM -> Reloc
    cv::Mat alignTransformLast; // SLAM -> Reloc

    glm::mat4 transformCG;
    glm::mat4 transformGC;


<<<<<<< Updated upstream
    bool debugging = true;
    std::string debug_output_path = "./workspace";
    std::map<int, cv::Mat> frameID2RelocPose;
    const bool use_online_pose{true}; // true: 使用眼睛的输入位姿; false（仅用于测试）: 使用离线位姿文件输入位姿
    std::shared_ptr<PoseSwicher> pose_swicher_ptr;
    cv::Mat T_wc;
=======
    std::string offline_output_path;
    cv::Mat selfSlamPose;
    bool capture_offline_data;
>>>>>>> Stashed changes
};

// #include "CameraTracking.cpp"
#endif //ARENGINE_CAMERATRACKING_H
