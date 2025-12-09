#ifndef ARENGINE_HDRSWITCH_H
#define ARENGINE_HDRSWITCH_H
#include "opencv3_definition.h"
#include "opencv2/opencv.hpp"
#include "ARModule.h"
#include "BasicData.h"
#include "ConfigLoader.h"
#include "App.h"

#include <memory>

class HDRSwitch: public ARModule {
public:
    int Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) override;

    int Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) override;

    int CollectRemoteProcs(
            SerilizedFrame &serilizedFrame,
            std::vector<RemoteProcPtr> &procs,
            FrameDataPtr frameDataPtr) override;
    int ProRemoteReturn(RemoteProcPtr proc) override;


    int ShutDown(AppData& appData,  SceneData& sceneData) override;
private:
    int environmentalState
};

#endif 