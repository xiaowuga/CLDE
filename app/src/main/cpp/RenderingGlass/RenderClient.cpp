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

#define LOG_TAG "RenderClient.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define HAND_JOINT_COUNT 16

RenderClient::RenderClient() = default;

RenderClient::~RenderClient() {
    // 清理离屏渲染资源
    destroyOffscreenFBO();
}

//TODO：现在没有传进来的关节位姿，后续需要添加

int RenderClient::Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
    LOGI("RenderClient init");


    mModel  = std::make_shared<renderModel>("test");
    mModel->shaderInit();
//    sceneData.getObject<SceneModel>();
//    mModel->loadFbModel("YIBIAOPAN.fb" ,appData.dataDir + "Models");
//    mModel->loadFbModel(appData.dataDir + "Models/YIBIAOPAN.fb");
//    mModel->loadFbModel(MakeSdcardPath("Download/FbModel/di1.fb"));
//    mModel->loadFbModel(MakeSdcardPath("Download/FbModel/di2.fb"));
//    mModel->loadFbModel(MakeSdcardPath("Download/FbModel/di3.fb"));
//    mModel->loadFbModel("TUILIGAN.fb","/storage/emulated/0/Download/FbModel");

    auto scene_virtualObjects = sceneData.getAllObjectsOfType<SceneModel>();
    for(int i = 0; i < scene_virtualObjects.size(); i ++){
        mModel->loadFbModel(scene_virtualObjects[i]->name, scene_virtualObjects[i]->filePath);
    }

    mModel->pushMeshFromCustomData();
    mModel->countIndiceSum();

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
    std::vector<std::string> passOrder = {"equirectangularToCubemap", "irradiance","prefilter","brdf","SSAOGeometry", "SSAO","shadowMappingDepth","pbr","background"};
//    std::vector<std::string> passOrder = {"equirectangularToCubemap", "irradiance","prefilter","brdf","shadowMappingDepth","pbr","background"};
    passManager.setPassOrder(passOrder);

    startTime = std::chrono::high_resolution_clock::now();

    mGizmoPass = std::make_shared<GizmoPass>();
    return STATE_OK;
}

int RenderClient::Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
//    LOGI("RenderClient update");;
    glm::mat4 mProject = project;
    glm::mat4 mView = view;

    glm::mat4 model_trans_mat = glm::mat4(1.0);

    std::string modelName    = "";
    std::string instanceName = "";
    std::string instanceId   = "";
    std::string originState  = "";
    std::string targetState  = "";

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
                //TODO 找不到position因为动画Json数据对应关系没做好
                positionArray   = anKeyframe.positionArray;
                quaternionArray = anKeyframe.quaternionArray;
            }
        }
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
////    cadDataManager::AnimationStateUnit::Ptr animationState = cadDataManager::DataInterface::getAnimationState(modelName, instanceId);
//        std::vector<cadDataManager::AnKeyframe> anKeyframes = animationState->keyframes;
//        for (auto& anKeyframe : anKeyframes) {
//            if (anKeyframe.originState == originState && anKeyframe.targetState == targetState) {
//                //TODO 找不到position因为动画Json数据对应关系没做好
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
        std::unordered_map<std::string, std::vector<cadDataManager::RenderInfo>> modifyModel = cadDataManager::DataInterface::modifyInstanceMatrix(instanceId, matrix);//零件实例位置的修改

//        mModel->getMMeshes().at(modifyModel.begin()->first).mTransformVector = modifyModel.begin()->second.begin()->matrix;

        std::string meshName = modifyModel.begin()->first;
        std::vector<std::vector<float>> meshTransform;
        for(auto& mModify : modifyModel){
            for(auto& mModifyInstance : mModify.second){
                if(mModifyInstance.type == "mesh"){
                    meshTransform.push_back(mModifyInstance.matrix);
                }
            }
        }
//        auto &meshTransform = modifyModel.begin()->second.begin()->matrix;
        mModel->updateMMesh(meshName, meshTransform);

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
    auto testNum = getFps();
    testNum = getIndiceSum();
    testNum = getFps();

    // 自动导出渲染结果（如果启用）
    if (autoExportEnabled) {
        framesSinceLastExport++;
        if (framesSinceLastExport >= exportInterval) {
            // 生成带时间戳的文件名
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            std::string filename = "/storage/emulated/0/Download/render_" + 
                                 std::to_string(timestamp) + ".png";
            exportRenderResult(filename);
            framesSinceLastExport = 0;
        }
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
        frameCount = 0;
        startTime = currentTime;
    }
}

bool RenderClient::exportRenderResult(const std::string& outputPath) {
    LOGI("开始导出渲染结果到: %s", outputPath.c_str());
    
    // 参数验证
    if (outputPath.empty()) {
        LOGI("导出失败: 输出路径为空");
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        LOGI("导出失败: 无效的分辨率 (%dx%d)", width, height);
        return false;
    }
    
    try {
        // 分配内存存储像素数据
        std::vector<unsigned char> pixels(width * height * 4); // RGBA格式
        
        if (pixels.empty()) {
            LOGI("导出失败: 无法分配内存");
            return false;
        }
        
        // 从当前帧缓冲区读取像素数据
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        // 检查OpenGL错误
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            LOGI("glReadPixels错误: 0x%x", err);
            return false;
        }
        
        // 创建OpenCV Mat对象（注意OpenGL的原点在左下角，需要翻转）
        cv::Mat image(height, width, CV_8UC4, pixels.data());
        
        if (image.empty()) {
            LOGI("导出失败: 无法创建图像Mat");
            return false;
        }
        
        // 翻转图像（OpenGL坐标系和图像坐标系Y轴相反）
        cv::Mat flippedImage;
        cv::flip(image, flippedImage, 0);
        
        // 转换RGBA到BGR（OpenCV默认使用BGR格式）
        cv::Mat bgrImage;
        cv::cvtColor(flippedImage, bgrImage, cv::COLOR_RGBA2BGR);
        
        // 保存图像
        bool success = cv::imwrite(outputPath, bgrImage);
        
        if (success) {
            LOGI("渲染结果导出成功: %s (分辨率: %dx%d)", outputPath.c_str(), width, height);
        } else {
            LOGI("保存图像失败: %s (可能是权限或路径问题)", outputPath.c_str());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LOGI("导出渲染结果时发生异常: %s", e.what());
        return false;
    } catch (...) {
        LOGI("导出渲染结果时发生未知异常");
        return false;
    }
}

void RenderClient::enableAutoExport(bool enable, int intervalFrames) {
    autoExportEnabled = enable;
    exportInterval = intervalFrames;
    framesSinceLastExport = 0;
    LOGI("自动导出%s，间隔帧数: %d", enable ? "已启用" : "已禁用", intervalFrames);
}

bool RenderClient::createOffscreenFBO(int width, int height) {
    LOGI("创建离屏FBO: %dx%d", width, height);
    
    // 如果已存在FBO且尺寸相同，直接返回
    if (offscreenFBO != 0 && offscreenWidth == width && offscreenHeight == height) {
        LOGI("FBO已存在且尺寸匹配，复用现有FBO");
        return true;
    }
    
    // 如果尺寸不同，先销毁旧的FBO
    if (offscreenFBO != 0) {
        destroyOffscreenFBO();
    }
    
    // 生成FBO
    glGenFramebuffers(1, &offscreenFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, offscreenFBO);
    
    // 创建颜色纹理
    glGenTextures(1, &offscreenColorTexture);
    glBindTexture(GL_TEXTURE_2D, offscreenColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, offscreenColorTexture, 0);
    
    // 创建深度渲染缓冲
    glGenRenderbuffers(1, &offscreenDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, offscreenDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, offscreenDepthRBO);
    
    // 检查FBO完整性
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGI("FBO创建失败，状态: 0x%x", status);
        destroyOffscreenFBO();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    
    // 解绑FBO，恢复默认帧缓冲
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    offscreenWidth = width;
    offscreenHeight = height;
    
    LOGI("离屏FBO创建成功");
    return true;
}

void RenderClient::destroyOffscreenFBO() {
    if (offscreenFBO != 0) {
        glDeleteFramebuffers(1, &offscreenFBO);
        offscreenFBO = 0;
    }
    if (offscreenColorTexture != 0) {
        glDeleteTextures(1, &offscreenColorTexture);
        offscreenColorTexture = 0;
    }
    if (offscreenDepthRBO != 0) {
        glDeleteRenderbuffers(1, &offscreenDepthRBO);
        offscreenDepthRBO = 0;
    }
    offscreenWidth = 0;
    offscreenHeight = 0;
    LOGI("离屏FBO已销毁");
}

bool RenderClient::exportOffscreenRender(const std::string& outputPath, 
                                         int targetWidth, 
                                         int targetHeight,
                                         const glm::mat4& customProject,
                                         const glm::mat4& customView) {
    LOGI("开始离屏渲染导出: %s, 分辨率: %dx%d", outputPath.c_str(), targetWidth, targetHeight);
    
    // 参数验证
    if (outputPath.empty()) {
        LOGI("导出失败: 输出路径为空");
        return false;
    }
    
    if (targetWidth <= 0 || targetHeight <= 0) {
        LOGI("导出失败: 无效的目标分辨率 (%dx%d)", targetWidth, targetHeight);
        return false;
    }
    
    try {
        // 保存当前OpenGL状态
        GLint prevFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        
        // 创建或复用FBO
        if (!createOffscreenFBO(targetWidth, targetHeight)) {
            LOGI("创建FBO失败");
            return false;
        }
        
        // 绑定离屏FBO
        glBindFramebuffer(GL_FRAMEBUFFER, offscreenFBO);
        
        // 设置视口为目标分辨率
        glViewport(0, 0, targetWidth, targetHeight);
        
        // 清除缓冲区
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 执行渲染
        glm::mat4 model_trans_mat = glm::mat4(1.0);
        
        // 根据新的分辨率调整投影矩阵（保持宽高比）
        glm::mat4 adjustedProject = customProject;
        float aspectRatio = (float)targetWidth / (float)targetHeight;
        float originalAspectRatio = (float)width / (float)height;
        
        // 如果宽高比不同，需要调整投影矩阵
        if (std::abs(aspectRatio - originalAspectRatio) > 0.01f) {
            // 这里可以根据需要调整投影矩阵
            // 简单起见，我们保持原投影矩阵，但用户可以传入自定义的
            LOGI("目标宽高比 %.2f 与原始宽高比 %.2f 不同", aspectRatio, originalAspectRatio);
        }
        
        // 渲染场景
        if (mModel) {
            mModel->render(adjustedProject, customView, model_trans_mat);
        }
        
        if (mGizmoPass) {
            mGizmoPass->updateBoundingBOX(boundingBoxArray);
            mGizmoPass->render(adjustedProject, customView);
        }
        
        if (mPbrPass) {
            mPbrPass->render(adjustedProject, customView, joc);
        }
        
        // 确保渲染完成
        glFinish();
        
        // 检查OpenGL错误
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            LOGI("渲染过程中出现OpenGL错误: 0x%x", err);
        }
        
        // 读取像素数据
        std::vector<unsigned char> pixels(targetWidth * targetHeight * 4); // RGBA格式
        glReadPixels(0, 0, targetWidth, targetHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        // 检查读取错误
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOGI("glReadPixels错误: 0x%x", err);
            glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            return false;
        }
        
        // 恢复原始OpenGL状态
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        
        // 图像处理和保存
        cv::Mat image(targetHeight, targetWidth, CV_8UC4, pixels.data());
        
        if (image.empty()) {
            LOGI("导出失败: 无法创建图像Mat");
            return false;
        }
        
        // 翻转图像（OpenGL坐标系和图像坐标系Y轴相反）
        cv::Mat flippedImage;
        cv::flip(image, flippedImage, 0);
        
        // 转换RGBA到BGR
        cv::Mat bgrImage;
        cv::cvtColor(flippedImage, bgrImage, cv::COLOR_RGBA2BGR);
        
        // 保存图像
        bool success = cv::imwrite(outputPath, bgrImage);
        
        if (success) {
            LOGI("离屏渲染导出成功: %s (分辨率: %dx%d)", outputPath.c_str(), targetWidth, targetHeight);
        } else {
            LOGI("保存图像失败: %s", outputPath.c_str());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LOGI("离屏渲染导出时发生异常: %s", e.what());
        // 确保恢复OpenGL状态
        GLint prevFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        if (prevFBO == (GLint)offscreenFBO) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        return false;
    } catch (...) {
        LOGI("离屏渲染导出时发生未知异常");
        return false;
    }
}