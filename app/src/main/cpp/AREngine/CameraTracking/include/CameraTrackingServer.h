#pragma once

#include "ConfigLoader.h"
#include"RPCServer.h"
// #include"Basic/include/RPC.h"
#include "Relocalization.h"
#include <queue>

class CameraTrackingServer
    :public ARModuleServer
{
public:
    virtual ARModuleServerPtr create() {
        return std::make_shared<CameraTrackingServer>();
    }

    virtual int init(RPCServerConnection& con) override;

    struct DetectedObj
    {
        std::string objName;

        cv::Rect     roi;
        cv::Matx34f  pose;

        DEFINE_BFS_IO_3(DetectedObj, objName, roi, pose);
    };

    virtual int call(RemoteProcPtr proc, FrameDataPtr frameDataPtr, RPCServerConnection& con) override;

    std::shared_ptr<Reloc::Relocalization> sys;
    std::string configPath;
    std::string configName;

    // int indexImu = 0;
    
    // bool hasImage = false;
    // bool useDepth = true;

    // cv::Mat imgColor;
    // cv::Mat imgColorBuffer;
    // cv::Mat imgDepth;
    // cv::Mat imgDepthBuffer;
    // double time;
    // double timeBuffer;
    int fps;

    struct input_data{
        cv::Mat rgb;
        cv::Mat depth;
        cv::Mat slam_pose;
        double tframe;
        input_data(cv::Mat rgb, cv::Mat depth, cv::Mat slam_pose, double tframe):
            rgb(rgb), depth(depth), slam_pose(slam_pose), tframe(tframe){};
        bool operator < (const input_data& other) const{ // for priority_queue, the smaller the higher priority
            return tframe > other.tframe;
        }
    };
    std::priority_queue<input_data> input_queue;
    bool initialized = false;

    bool debugging = false;
    std::string debug_output_path = "./workspace";

    int sensorType = 2; // 0=MONOCULAR, 2=RGBD
};

