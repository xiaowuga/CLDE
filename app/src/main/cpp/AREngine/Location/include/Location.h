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
    ArucoPP m_arucoDetector;
    ArucoPP m_arucoDetector2;
    // T_Virtual_Marker (Target Pose)
    glm::mat4 m_markerPose_Cockpit;
    // T_Virtual_World = T_Virtual_Marker * inv(T_World_Marker)
    glm::mat4 m_worldAlignMatrix;

    std::vector<glm::mat4> m_alignTrajectoryCache;
    glm::mat4 m_lastAlignMatrix;
    std::shared_mutex m_dataMutex;

    std::string alignTransMap2CockpitFile;


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
