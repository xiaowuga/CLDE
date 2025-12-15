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

// Eigen::Matrix<double, 7, 1> create_tum_pose(const cv::Mat& pose) {
//     // Create the TUM pose matrix
//     Eigen::Matrix<double, 7, 1> tum_pose;

//     // Translation (x, y, z)
//     tum_pose(0) = pose.at<double>(0, 3);
//     tum_pose(1) = pose.at<double>(1, 3);
//     tum_pose(2) = pose.at<double>(2, 3);

//     // Rotation matrix (3x3) and quaternion
//     Eigen::Matrix3d R;
//     cv::cv2eigen(pose(cv::Rect(0, 0, 3, 3)), R);
//     Eigen::Quaterniond q(R);

//     // Quaternion (qx, qy, qz, qw)
//     tum_pose(3) = q.x();
//     tum_pose(4) = q.y();
//     tum_pose(5) = q.z();
//     tum_pose(6) = q.w();

//     return tum_pose;
// }
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
private:
    
    std::string configPath;
    std::string configName;
    std::string alignTransformLastFile;

    int indexImu = 0;
    
    bool hasImage = false;
    bool useDepth = true;

    cv::Mat imgColor;
    cv::Mat imgColorBuffer; // input rgb clone
    cv::Mat imgDepth; // input depth clone
    cv::Mat imgDepthBuffer;
    double time;
    double timeBuffer;
    int fps;

    cv::Mat alignTransform; // SLAM -> Reloc
    cv::Mat alignTransformLast; // SLAM -> Reloc

    std::vector<cv::Mat> vAlignTransform;

    bool has_shutdown = false;

    bool debugging = false;
    std::string debug_output_path = "./workspace";
    std::map<int, cv::Mat> frameID2RelocPose;
    const bool use_online_pose{true}; // true: 使用眼睛的输入位姿; false（仅用于测试）: 使用离线位姿文件输入位姿
    std::shared_ptr<PoseSwicher> pose_swicher_ptr;
    cv::Mat T_wc;
    bool capture_offline_data;
};

// #include "CameraTracking.cpp"
#endif //ARENGINE_CAMERATRACKING_H
