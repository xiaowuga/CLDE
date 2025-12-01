//
// Created by shutiao on 2025/9/13.
//

#include"AREngine/Basic/include/RPC.h"
#include"AREngine/Basic/include/ARModule.h"
#include"AREngine/Basic/include/App.h"
#include"opencv2/core.hpp"
#include"renderModel.h"
#include"openxr_loader/include/common/xr_linear.h"
#include"utils.h"

#ifndef ROKIDOPENXRANDROIDDEMO_RENDERCLIENT_H
#define ROKIDOPENXRANDROIDDEMO_RENDERCLIENT_H

#include "RenderingGlass/pbrPass.h"
#include "RenderingGlass/equirectangularToCubemapPass.h"
#include "RenderingGlass/renderPassManager.h"
#include "RenderingGlass/irradiancePass.h"
#include "RenderingGlass/prefilterPass.h"
#include "RenderingGlass/brdfPass.h"
#include "RenderingGlass/backgroundPass.h"
#include "RenderingGlass/shadowMappingDepthPass.h"
#include "RenderingGlass/SSAOGeometryPass.h"
#include "RenderingGlass/SSAOPass.h"
#include "RenderingGlass/GizmoPass.h"

class RenderClient : public ARModule {
private:

    std::shared_ptr<EquirectangularToCubemapPass> mEquirectangularToCubemapPass;
    std::shared_ptr<IrradiancePass> mIrradiancePass;
    std::shared_ptr<PrefilterPass> mPrefilterPass;
    std::shared_ptr<BrdfPass> mBrdfPass;
    std::shared_ptr<ShadowMappingDepthPass> mShadowMappingDepthPass;
    std::shared_ptr<PbrPass> mPbrPass;
    std::shared_ptr<BackgroundPass> mBackgroundPass;
    std::shared_ptr<SSAOGeometryPass> mSSAOGeometryPass;
    std::shared_ptr<SSAOPass> mSSAOPass;
    std::shared_ptr<renderModel> mModel; // for test
    std::shared_ptr<GizmoPass> mGizmoPass;
    int actionFrame = -1;
    std::vector<float> positionArray;
    std::vector<float> quaternionArray;
    int frameCount = 0;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<float>> startTime;
    float fps;
    int width = 1920;
    int height = 1200;

    std::string modelName    = "";
    std::string instanceName = "";
    std::string instanceId   = "";
    std::string originState  = "";
    std::string targetState  = "";

public:
    std::vector<float> boundingBoxArray;
    std::unordered_map<std::string, std::vector<float>> boundingBoxMap;
    std::vector<glm::mat4> joc;
    glm::mat4 project;
    glm::mat4 view;

    RenderClient();

    ~RenderClient();

    void PreCompute(std::string configPath) override;

    int Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;

    int CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr) override;

    int ProRemoteReturn(RemoteProcPtr proc) override;

    int Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;

    int ShutDown(AppData& appData, SceneData& sceneData) override;

    void updateFrameCount();

    float getFps(){
        return fps;
    }

    int getWidth(){
        return width;
    }

    int getHeight(){
        return height;
    }

    int getIndiceSum(){
        return mModel->getIndiceSum();
    }

    void highlightInstance(std::string modelName, std::string instanceId);
};


#endif //ROKIDOPENXRANDROIDDEMO_RENDERCLIENT_H
