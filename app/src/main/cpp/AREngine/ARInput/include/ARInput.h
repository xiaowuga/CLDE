
#pragma once

#include"App.h"
#include "opencv2/core/core.hpp"
#include<shared_mutex>

class ARInputSources{
public:
    static ARInputSources* instance();

    enum{
        DATAF_IMAGE=0x01,
        DATAF_TIMESTAMP=0x02,
        DATAF_CAMERAMAT=0x04
    };
    struct FrameData{
        cv::Mat   img;
        uint64_t  timestamp;
        cv::Matx44f cameraMat;
    };

    void set(const FrameData &frameData, int  mask);

    void get(FrameData &frameData, int mask=-1);
private:
    FrameData  _frameData;
    std::shared_mutex  _dataMutex;
};

class ARInputs
        :public ARModule
{
    uint64_t _lastTimestamp=0;
    cv::Mat  _udistMap1, _udistMap2;
    cv::Matx33f  _camK;
public:
    virtual int Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;
    virtual int Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) override;


};

//
//class AREngine
//{
//public:
//    static AREngine* instance();
//
//    virtual void start() =0;
//
//    struct FrameData
//    {
//        cv::Mat  img;
//    };
//
//    virtual void setFrame(const FrameData &frame) =0;
//
//    virtual FrameData getFrame() =0;
//
//    virtual void grabEngineOutput(ARApp *app) =0;
//
//
//};