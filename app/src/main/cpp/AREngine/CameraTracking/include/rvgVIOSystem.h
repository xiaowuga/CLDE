//
// Created by zyd on 23-9-18.
//

#ifndef ARENGINE_RVGVIOSYSTEM_H
#define ARENGINE_RVGVIOSYSTEM_H

#include <opencv2/opencv.hpp>
#include <memory>

namespace RvgVio {

class VIOSystem;

class RvgVioSystem {
public:
    bool Init(std::string& configPath, std::string& configName);
    bool isInitialized();
    void processRGBD(double timestamp, cv::Mat& frame, cv::Mat& depth_image);
    void processRGB(double timestamp, cv::Mat& frame);

    void processIMU(std::vector<double>& imuData);
    cv::Mat getPose() const; // double 


private:
    std::shared_ptr<VIOSystem> sys;
};
}

#endif //ARENGINE_RVGVIOSYSTEM_H
