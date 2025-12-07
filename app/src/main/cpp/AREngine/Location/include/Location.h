//
// Created by xiaow on 2025/9/7.
//

#ifndef ROKIDOPENXRANDROIDDEMO_LOCATION_H
#define ROKIDOPENXRANDROIDDEMO_LOCATION_H

#include "BasicData.h"
#include "ARModule.h"
#include "arucopp.h"
#include "glm/glm.hpp"
#include "ARInput.h"
#include "App.h"



class Location : public ARModule {
public:
    ArucoPP  _detector;
    ArucoPP  _detector2;
    glm::mat4 markerPose;
    glm::mat4 markerPose2;
    glm::mat4 marker;
    glm::mat4 marker_inv;
    glm::mat4 trans;
    glm::mat4 trans_inv;
    glm::mat4 diff;
    std::vector<glm::mat4> cache;
    std::shared_mutex  _dataMutex;
    glm::mat4 lastTrans;
    glm::mat4 marker2;


public:

    int Init(AppData& appData, SceneData& SceneData, FrameDataPtr frameDataPtr) override;
    int Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;
    int ShutDown(AppData& appData, SceneData& sceneData) override;
    int CollectRemoteProcs(
            SerilizedFrame &serilizedFrame,
            std::vector<RemoteProcPtr> &procs,
            FrameDataPtr frameDataPtr) override;
    int ProRemoteReturn(RemoteProcPtr proc) override;



};


#endif //ROKIDOPENXRANDROIDDEMO_LOCATION_H
