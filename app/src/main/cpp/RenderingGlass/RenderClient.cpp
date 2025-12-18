//
// Created by shutiao on 2025/9/13.
//

#include "RenderClient.h"
#include <android/log.h>
#include <fstream>
#include <jni.h>
#include"opencv2/core.hpp"
#include "app/utilsmym.hpp"
#include <opencv2/opencv.hpp>
#include <utility>

#define LOG_TAG "RenderClient.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define HAND_JOINT_COUNT 16

RenderClient::RenderClient() = default;

RenderClient::~RenderClient() = default;

//TODO：现在没有传进来的关节位姿，后续需要添加

int RenderClient::Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
    LOGI("RenderClient init");


    mModel  = std::make_shared<renderModel>("test");
    mModel->shaderInit();

    auto scene_virtualObjects = sceneData.getAllObjectsOfType<SceneModel>();
    for(int i = 0; i < scene_virtualObjects.size(); i ++){
        mModel->loadFbModel(scene_virtualObjects[i]->name, scene_virtualObjects[i]->filePath);
    }

    //加载模型的动画数据：Action + State
    cadDataManager::DataInterface::loadAnimationActionData(appData.animationActionConfigFile);
    cadDataManager::DataInterface::loadAnimationStateData(appData.animationStateConfigFile);

//    mModel->loadModel("model/backpack/backpack.obj");
    // 获取RenderPassManager单例
    auto& passManager = RenderPassManager::getInstance();
    // 初始化渲染通道、注册渲染通道
    mEquirectangularToCubemapPass = std::make_shared<EquirectangularToCubemapPass>();
    mEquirectangularToCubemapPass->initialize(mModel->getMMeshes());
    passManager.registerPass("equirectangularToCubemap", mEquirectangularToCubemapPass);

    mIrradiancePass = std::make_shared<IrradiancePass>();
    mIrradiancePass->initialize(mModel->getMMeshes());
    passManager.registerPass("irradiance", mIrradiancePass);

    mPrefilterPass = std::make_shared<PrefilterPass>();
    mPrefilterPass->initialize(mModel->getMMeshes());
    passManager.registerPass("prefilter", mPrefilterPass);

    mBrdfPass = std::make_shared<BrdfPass>();
    mBrdfPass->initialize(mModel->getMMeshes());
    passManager.registerPass("brdf", mBrdfPass);

    mShadowMappingDepthPass = std::make_shared<ShadowMappingDepthPass>();
    mShadowMappingDepthPass->initialize(mModel->getMMeshes());
    passManager.registerPass("shadowMappingDepth", mShadowMappingDepthPass);

    mPbrPass = std::make_shared<PbrPass>();
    mPbrPass->initialize(mModel->getMMeshes());
    passManager.registerPass("pbr", mPbrPass);

    mBackgroundPass = std::make_shared<BackgroundPass>();
    mBackgroundPass->initialize(mModel->getMMeshes());
    passManager.registerPass("background", mBackgroundPass);

    mSSAOGeometryPass = std::make_shared<SSAOGeometryPass>();
    mSSAOGeometryPass->initialize(mModel->getMMeshes());
    passManager.registerPass("SSAOGeometry", mSSAOGeometryPass);

    mSSAOPass = std::make_shared<SSAOPass>();
    mSSAOPass->initialize(mModel->getMMeshes());
    passManager.registerPass("SSAO", mSSAOPass);


    // 设置渲染顺序（先环境贴图转换，后PBR渲染）
//    std::vector<std::string> passOrder = {
//            "equirectangularToCubemap",
//            "irradiance",
//            "prefilter",
//            "brdf",
//            "SSAOGeometry",
//            "SSAO",
//            "shadowMappingDepth",
//            "pbr",
//            "background"};
    std::vector<std::string> passOrder = {"equirectangularToCubemap", "irradiance","prefilter","brdf","shadowMappingDepth","pbr","background"};
    passManager.setPassOrder(passOrder);

    startTime = std::chrono::high_resolution_clock::now();

    mGizmoPass = std::make_shared<GizmoPass>();
    mGizmoPass->initBoundingBoxMap(boundingBoxMap);
    return STATE_OK;
}

int RenderClient::Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
//    LOGI("RenderClient update");;
    glm::mat4 mProject = project;
    glm::mat4 mView = view;

    glm::mat4 model_trans_mat = glm::mat4(1.0);

    //交互需要的接口
    if(!sceneData.actionPassage.isEmpty()){
        modelName    = sceneData.actionPassage.modelName;
        instanceName = sceneData.actionPassage.instanceName;
        originState  = sceneData.actionPassage.originState;
        targetState  = sceneData.actionPassage.targetState;
        instanceId = sceneData.actionPassage.instanceId;
        //设置活跃模型
        cadDataManager::DataInterface::setActiveDocumentData(modelName);
        auto animationState = cadDataManager::DataInterface::getAnimationStateByName(modelName, instanceName);
        std::vector<cadDataManager::AnKeyframe> &anKeyframes = animationState->keyframes;
        for (auto& anKeyframe : anKeyframes) {
            if (anKeyframe.originState == originState && anKeyframe.targetState == targetState) {
                positionArray   = anKeyframe.positionArray;
                quaternionArray = anKeyframe.quaternionArray;
            }
        }
        //高亮模型
//        highlightInstance(modelName, instanceId);
    }

//    {//测试接口用代码，推力杆会动
//        std::vector<cadDataManager::AnimationActionUnit::Ptr> animationActions = cadDataManager::DataInterface::getAnimationActions("EngineFireAlarm");
//        auto animationAction = animationActions[0];
//        modelName = animationAction->modelName;
//        instanceName = animationAction->instanceName;
//        instanceId = animationAction->instanceId;
//        originState = animationAction->originState;
//        targetState = "3";
//        //加载了多个模型时，需要对ModelName模型执行动画，切换该ModelName为当前活跃状态
//        cadDataManager::DataInterface::setActiveDocumentData(modelName);
//        auto instance = cadDataManager::DataInterface::getInstanceByName(instanceName);
//        instanceId = instance->getId();
//
//        cadDataManager::AnimationStateUnit::Ptr animationState = cadDataManager::DataInterface::getAnimationStateByName(modelName, instanceName);
//        std::vector<cadDataManager::AnKeyframe> anKeyframes = animationState->keyframes;
//        for (auto& anKeyframe : anKeyframes) {
//            if (anKeyframe.originState == originState && anKeyframe.targetState == targetState) {
//                positionArray = anKeyframe.positionArray;
//                quaternionArray = anKeyframe.quaternionArray;
//            }
//        }
//    }

    //调用位姿变换接口，实时更新模型位置
    if (positionArray.size() != 0) {
        LOGI("找到positionArray");
        actionFrame ++;
//        LOGI("%i", actionFrame);
        std::vector<float> position   = { positionArray[actionFrame * 3], positionArray[actionFrame * 3 + 1], positionArray[actionFrame * 3 + 2] };
        std::vector<float> quaternion = { quaternionArray[actionFrame * 4] , quaternionArray[actionFrame * 4 + 1], quaternionArray[actionFrame * 4 + 2],quaternionArray[actionFrame * 4 + 3] };
        std::vector<float> matrix     = cadDataManager::DataInterface::composeMatrix(position, quaternion);

        std::unordered_map<std::string, std::vector<cadDataManager::RenderInfo>> modifyModel;

//        //测试高亮的接口，需要把restoreInstanceColor给注释掉
//        highlightInstance(modelName, instanceId);
        cadDataManager::DataInterface::restoreInstanceColor(instanceId);//零件实例高亮的恢复
//        cadDataManager::DataInterface::modifyInstanceColor(instanceId, "#FF0000");//零件实例高亮的修改
        modifyModel = cadDataManager::DataInterface::modifyInstanceMatrix(instanceId, matrix);//零件实例位置的修改

        mModel->processMeshData(modifyModel);

        if((actionFrame * 3 + 3) == positionArray.size()){
            actionFrame = -1;
            sceneData.actionPassage.clear();
            positionArray.clear();
            quaternionArray.clear();
            sceneData.actionLock.try_lock();
            sceneData.actionLock.unlock();
        }
    }

    mModel->render(project,view,model_trans_mat);
    mGizmoPass->updateBoundingBOX(boundingBoxArray);
    mGizmoPass->render(project, view);
    mPbrPass->render(project, view, joc);

    updateFrameCount();
//    auto testNum = getFps();
//    testNum = getIndiceSum();
//    testNum = getFps();
    if(appData.environmentalState != environmentalState){
        environmentalState = appData.environmentalState;
//        auto& passManager = RenderPassManager::getInstance();
//        auto pbrPass = passManager.getPassAs<PbrPass>("pbr");
//        pbrPass->setLightChange(true);

        auto& passManager = RenderPassManager::getInstance();
        auto equiPass = passManager.getPassAs<EquirectangularToCubemapPass>("equirectangularToCubemap");
        equiPass->setEnvCubeMap(environmentalState);

        auto irradiancePass = passManager.getPassAs<IrradiancePass>("irradiance");
        irradiancePass->setIrradianceMap(environmentalState);

        auto prefilterPass = passManager.getPassAs<PrefilterPass>("prefilter");
        prefilterPass->setPrefilterMap(environmentalState);
    }

    return STATE_OK;
}

int RenderClient::CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr) {
    LOGI("Rendering CollectRemoteProcs frameDataPtr");
    return STATE_OK;
}

int RenderClient::ProRemoteReturn(RemoteProcPtr proc) {
    return STATE_OK;
}

int RenderClient::ShutDown(AppData& appData, SceneData& sceneData) {
    return STATE_OK;
}

void RenderClient::PreCompute(std::string configPath) {
}

void RenderClient::updateFrameCount() {

    // 增量帧计数器
    frameCount++;

    // 计算经过的时间
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsedTime = currentTime - startTime;

    // 检查是否已过一秒
    if (elapsedTime.count() >= 1.0f)
    {
        // 计算 FPS
        fps = frameCount / elapsedTime.count();

        // 将FPS输出到控制台
        std::cout << "FPS: " << fps << std::endl;

        // 下一秒重置
//        frameCount = 0;
        startTime = currentTime;
    }
}

void RenderClient::highlightInstance(std::string modelName, std::string instanceId){
    std::unordered_map<std::string, std::vector<cadDataManager::RenderInfo>> modifyModel;
    cadDataManager::DataInterface::setActiveDocumentData(modelName);
    modifyModel = cadDataManager::DataInterface::modifyInstanceColor(instanceId, "#FF0000");//零件实例高亮的修改
    mModel->processMeshData(modifyModel);
}

std::vector<float> countCenter(){

}