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
    int height = 1080;

public:
    std::vector<float> boundingBoxArray;
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

    // 导出渲染结果的功能
    bool exportRenderResult(const std::string& outputPath);
    
    // 设置是否启用自动导出（每N帧导出一次）
    void enableAutoExport(bool enable, int intervalFrames = 30);
    
    // 离屏渲染并导出（不影响当前渲染，支持自定义分辨率）
    bool exportOffscreenRender(const std::string& outputPath, 
                               int targetWidth, 
                               int targetHeight,
                               const glm::mat4& project,
                               const glm::mat4& view);

private:
    bool autoExportEnabled = false;
    int exportInterval = 30;
    int framesSinceLastExport = 0;
    
    // 离屏渲染相关
    bool createOffscreenFBO(int width, int height);
    void destroyOffscreenFBO();
    GLuint offscreenFBO = 0;
    GLuint offscreenColorTexture = 0;
    GLuint offscreenDepthRBO = 0;
    int offscreenWidth = 0;
    int offscreenHeight = 0;

};


#endif //ROKIDOPENXRANDROIDDEMO_RENDERCLIENT_H
