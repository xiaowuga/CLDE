//
// Created by 21718 on 2025/8/30.
//

#ifndef ROKIDOPENXRANDROIDDEMO_MYCOLLISIONHANDLERS_H
#define ROKIDOPENXRANDROIDDEMO_MYCOLLISIONHANDLERS_H

#include "BasicData.h"
#include "ARModule.h"
#include "demos/model.h"
#include "InteractionConfigLoader.h"
#include "Animator.h"
#include "AnimationPlayer.h"

std::string getCurrentTimestamp() {
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 转换为time_t
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    // 转换为本地时间
    std::tm local_tm;
    local_tm = *std::localtime(&now_time);
    // 格式化
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &local_tm);
    return std::string(buffer);
}

#define LOG_TAG "MyCollisionHandlers.h"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
namespace MyCollisionHandlers {
#define MakeMyCollisionHandler(__CLASS_NAME__)                                      \
    std::shared_ptr<Animator::Animator> animator = nullptr;                         \
    int animation_init_state = 0;                                                   \
    using Ptr = std::shared_ptr<__CLASS_NAME__>;                                    \
    __CLASS_NAME__() : CollisionHandler(#__CLASS_NAME__) {}                         \
    static CollisionHandler::Ptr create() {                                         \
        return std::make_shared<__CLASS_NAME__>();                                  \
    }                                                                               \
    static inline int registered =                                                  \
        InteractionConfigLoader::RegisterCollisionHandler(                          \
            #__CLASS_NAME__, &__CLASS_NAME__::create);                              \
    std::chrono::steady_clock::time_point last_trigger_time;                        \
    float cool_down_interval = 0.5f;                                                \
    float trigger_interval() {                                                      \
        auto cur_time = std::chrono::steady_clock::now();                           \
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(             \
                    cur_time - last_trigger_time).count();                          \
        last_trigger_time = cur_time;                                               \
        return (float)t / 1000.0f;                                                  \
    }
    class Stick : public CollisionHandler{
    public:
        MakeMyCollisionHandler(Stick)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            LOGI("Collision detected between %s and %s", obj1->modelName.c_str(), obj2->modelName.c_str());
            if (trigger_interval() < cool_down_interval) {
                return;
            }

            //TODO:让渲染模块控制动画播放速度
            if(!animationPlaying){
                currentStateIndex = obj2->originStateIndex;
            }
            auto animationSate = cadDataManager::DataInterface::getAnimationStateByName(obj2->modelName, obj2->instanceName);
            int allSateSize = animationSate->allStates.size();
            // get gesture here and set animator state by gesture
            auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
            if (cur_right_hand_gesture == Gesture::GRASP) {
                if(_frameDataPtr->tip_velocity[0] > 0.09){
                    targetStateIndex = currentStateIndex + 1;
                    targetStateIndex = targetStateIndex > allSateSize - 1 ? allSateSize - 1 : targetStateIndex;
                }else if(_frameDataPtr->tip_velocity[0] < -0.09){
                    targetStateIndex = currentStateIndex - 1;
                    targetStateIndex = targetStateIndex < 0 ? 0 : targetStateIndex;
                }
            }
            if(currentStateIndex != targetStateIndex){
                animationPlaying = true;
                // 将交互动作传递到渲染模块
                sceneData.actionPassage.modelName = obj2->modelName;
                sceneData.actionPassage.instanceName = obj2->instanceName;
                sceneData.actionPassage.originState = animationSate->allStates[currentStateIndex];
                sceneData.actionPassage.targetState = animationSate->allStates[targetStateIndex];
                sceneData.actionPassage.instanceId = obj2->instanceId;
                currentStateIndex = targetStateIndex;
            }
            // Log interaction
            _frameDataPtr->interactionLog.curLGesture = _frameDataPtr->gestureDataPtr->curLGesture;
            _frameDataPtr->interactionLog.curRGesture = _frameDataPtr->gestureDataPtr->curRGesture;
            _frameDataPtr->interactionLog.interactionType = "Stick";
            _frameDataPtr->interactionLog.targetModelName = obj2->modelName;
            _frameDataPtr->interactionLog.targetInstanceName = obj2->instanceName;
            _frameDataPtr->interactionLog.targetInstanceID = obj2->instanceId;
            _frameDataPtr->interactionLog.currentActionState = sceneData.actionPassage.originState;
            _frameDataPtr->interactionLog.targetActionState = sceneData.actionPassage.targetState;
            _frameDataPtr->interactionLog.timestamp = getCurrentTimestamp();
            _frameDataPtr->interactionLog.frameID = _frameDataPtr->frameID;

            //LOGI("InteractionLog %s", _frameDataPtr->interactionLog.toString().c_str());
        }
    private:
        bool animationPlaying = false;
        int currentStateIndex = 0;
        int targetStateIndex = 0;
    };

    class Button : public CollisionHandler {
    public:
        MakeMyCollisionHandler(Button)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->modelName << " and " << obj2->modelName << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
//            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
//            animator = animation_player->findAnimator(obj2->modelName);
//            if (animator == nullptr) {
//                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
//                animator = std::make_shared<Animator::Animator>(
//                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
//                animation_player->addAnimator(obj2->modelName, animator);
//            }
//            if (animator->isPlaying == false) {
//                // get gesture here and set animator state by gesture
//                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
//                if (cur_right_hand_gesture == Gesture::CURSOR) {
//                    animator->nextStateIndex = (animator->currentStateIndex + 1) % animator->allStates.size();
//                }
//                if (animator->currentStateIndex != animator->nextStateIndex) {
//                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
//                    animator->isPlaying = true;
//                    animator->animationSequenceIndex = 0;
//                }
//            }
            if(!animationPlaying){
                currentStateIndex = obj2->originStateIndex;
            }
            auto animationSate = cadDataManager::DataInterface::getAnimationStateByName(obj2->modelName, obj2->instanceName);
            int allSateSize = animationSate->allStates.size();
            // get gesture here and set animator state by gesture
            auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
            if (cur_right_hand_gesture == Gesture::CURSOR) {
                targetStateIndex = (currentStateIndex + 1) % allSateSize;
            }
            if(currentStateIndex != targetStateIndex){
                animationPlaying = true;
                // 将交互动作传递到渲染模块
                sceneData.actionPassage.modelName = obj2->modelName;
                sceneData.actionPassage.instanceName = obj2->instanceName;
                sceneData.actionPassage.originState = animationSate->allStates[currentStateIndex];
                sceneData.actionPassage.targetState = animationSate->allStates[targetStateIndex];
                sceneData.actionPassage.instanceId = obj2->instanceId;
                currentStateIndex = targetStateIndex;
            }

            // Log interaction
            _frameDataPtr->interactionLog.curLGesture = _frameDataPtr->gestureDataPtr->curLGesture;
            _frameDataPtr->interactionLog.curRGesture = _frameDataPtr->gestureDataPtr->curRGesture;
            _frameDataPtr->interactionLog.interactionType = "Button";
            _frameDataPtr->interactionLog.targetModelName = obj2->modelName;
            _frameDataPtr->interactionLog.targetInstanceName = obj2->instanceName;
            _frameDataPtr->interactionLog.targetInstanceID = obj2->instanceId;
            _frameDataPtr->interactionLog.currentActionState = sceneData.actionPassage.originState;
            _frameDataPtr->interactionLog.targetActionState = sceneData.actionPassage.targetState;
            _frameDataPtr->interactionLog.timestamp = getCurrentTimestamp();
            _frameDataPtr->interactionLog.frameID = _frameDataPtr->frameID;
        }
    private:
        bool animationPlaying = false;
        int currentStateIndex = 0;
        int targetStateIndex = 0;
    };

    class ProtectiveCover : public CollisionHandler {
    public:
        MakeMyCollisionHandler(ProtectiveCover)
        bool is_open() {
            //return animator->allStates[animator->currentStateIndex] == "OPEN";
            return animationState->allStates[currentStateIndex] == "OPEN";
        }
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->modelName << " and " << obj2->modelName
                      << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
//            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
//            animator = animation_player->findAnimator(obj2->modelName);
//            if (animator == nullptr) {
//                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
//                animator = std::make_shared<Animator::Animator>(
//                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
//                animation_player->addAnimator(obj2->modelName, animator);
//            }
//            if (animator->isPlaying == false) {
//                // get gesture here and set animator state by gesture
//                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
//                if (cur_right_hand_gesture == Gesture::GRASP) {
//                    if (_frameDataPtr->tip_movement == "up") {
//                        if (animator->currentStateIndex == animator->allStates.size() - 1) {
//                            return;
//                        }
//                        animator->nextStateIndex = animator->currentStateIndex + 1;
//                    }
//                    else if (_frameDataPtr->tip_movement == "down") {
//                        if (animator->currentStateIndex == 0) {
//                            return;
//                        }
//                        animator->nextStateIndex = animator->currentStateIndex - 1;
//                    }
//                    else {
//                        return;
//                    }
//                }
//                if (animator->currentStateIndex != animator->nextStateIndex) {
//                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
//                    animator->isPlaying = true;
//                    animator->animationSequenceIndex = 0;
//                }
//            }
            if(!animationPlaying){
                currentStateIndex = obj2->originStateIndex;
            }
            animationState = cadDataManager::DataInterface::getAnimationStateByName(obj2->modelName, obj2->instanceName);
            int allSateSize = animationState->allStates.size();
            // get gesture here and set animator state by gesture
            auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
            if (cur_right_hand_gesture == Gesture::GRASP) {
                //if(_frameDataPtr->tip_velocity[0] > 0.02){
                    targetStateIndex = currentStateIndex + 1;
                    targetStateIndex = targetStateIndex > allSateSize - 1 ? allSateSize - 1 : targetStateIndex;
                //}else if(_frameDataPtr->tip_velocity[0] < -0.02){
                //    targetStateIndex = currentStateIndex - 1;
                //    targetStateIndex = targetStateIndex < 0 ? 0 : targetStateIndex;
                //}
            }
            if(currentStateIndex != targetStateIndex){
                animationPlaying = true;
                // 将交互动作传递到渲染模块
                sceneData.actionPassage.modelName = obj2->modelName;
                sceneData.actionPassage.instanceName = obj2->instanceName;
                sceneData.actionPassage.originState = animationState->allStates[currentStateIndex];
                sceneData.actionPassage.targetState = animationState->allStates[targetStateIndex];
                sceneData.actionPassage.instanceId = obj2->instanceId;
                currentStateIndex = targetStateIndex;
            }

            // Log interaction
            _frameDataPtr->interactionLog.curLGesture = _frameDataPtr->gestureDataPtr->curLGesture;
            _frameDataPtr->interactionLog.curRGesture = _frameDataPtr->gestureDataPtr->curRGesture;
            _frameDataPtr->interactionLog.interactionType = "ProtectiveCover";
            _frameDataPtr->interactionLog.targetModelName = obj2->modelName;
            _frameDataPtr->interactionLog.targetInstanceName = obj2->instanceName;
            _frameDataPtr->interactionLog.targetInstanceID = obj2->instanceId;
            _frameDataPtr->interactionLog.currentActionState = sceneData.actionPassage.originState;
            _frameDataPtr->interactionLog.targetActionState = sceneData.actionPassage.targetState;
            _frameDataPtr->interactionLog.timestamp = getCurrentTimestamp();
            _frameDataPtr->interactionLog.frameID = _frameDataPtr->frameID;
        }
    private:
        cadDataManager::AnimationStateUnit::Ptr animationState = nullptr;
        bool animationPlaying = false;
        int currentStateIndex = 0;
        int targetStateIndex = 0;
    };

    class ButtonUnderCover : public CollisionHandler {
    public:
        MakeMyCollisionHandler(ButtonUnderCover)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->modelName << " and " << obj2->modelName << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
//            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
//            animator = animation_player->findAnimator(obj2->modelName);
//            if (animator == nullptr) {
//                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
//                animator = std::make_shared<Animator::Animator>(
//                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
//                animation_player->addAnimator(obj2->modelName, animator);
//            }
//            if (animator->isPlaying == false) {
//                // get gesture here and set animator state by gesture
//                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
//                if (cur_right_hand_gesture == Gesture::CURSOR) {
//                    // find the corresponding protective cover's collision handler
//                    // obj2_name: xxxxx<k>, then the corresponding protective cover should be named: xxxxx-G<k>
//                    auto obj2_name = obj2->modelName;
//                    size_t pos = obj2_name.find('<');
//                    if (pos == std::string::npos) {
//                        throw std::invalid_argument("Sample name format invalid: missing <k>");
//                    }
//                    std::string prefix = obj2_name.substr(0, pos);
//                    std::string suffix = obj2_name.substr(pos);
//                    std::string protective_cover_name = prefix + "-G" + suffix;
//                    ProtectiveCover::Ptr protective_cover = nullptr;
//                    for (auto pair : sceneData.collisionPairs) {
//                        if (pair->GetObj2()->modelName == protective_cover_name) {
//                            assert(pair->GetHandler()->class_name != "ProtectiveCover");
//                            protective_cover = std::static_pointer_cast<ProtectiveCover>(pair->GetHandler());
//                        }
//                    }
//                    if (protective_cover == nullptr) {
//                        throw std::runtime_error("Cannot find corresponding ProtectiveCover for: " + obj2_name);
//                    }
//                    if (!protective_cover->is_open()) {
//                        return;
//                    }
//                    else {
//                        animator->nextStateIndex =
//                                (animator->currentStateIndex + 1) % animator->allStates.size();
//                    }
//                }
//                if (animator->currentStateIndex != animator->nextStateIndex) {
//                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
//                    animator->isPlaying = true;
//                    animator->animationSequenceIndex = 0;
//                }
//            }
            if(!animationPlaying){
                currentStateIndex = obj2->originStateIndex;
            }
            auto animationSate = cadDataManager::DataInterface::getAnimationStateByName(obj2->modelName, obj2->instanceName);
            int allSateSize = animationSate->allStates.size();
            // get gesture here and set animator state by gesture
            auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
            if (cur_right_hand_gesture == Gesture::CURSOR) {
                // find the corresponding protective cover's collision handler
                // obj2_instanceName: xxxxx<k>, then the corresponding protective cover instance should be named: xxxxx-G<k>
                auto obj2_instanceName = obj2->instanceName;
                size_t pos = obj2_instanceName.find('<');
                if (pos == std::string::npos) {
                    throw std::invalid_argument("Sample name format invalid: missing <k>");
                }
                std::string prefix = obj2_instanceName.substr(0, pos);
                std::string suffix = obj2_instanceName.substr(pos);
                std::string protective_cover_name = prefix + "-G" + suffix;
                ProtectiveCover::Ptr protective_cover = nullptr;
                for (auto pair : sceneData.collisionPairs) {
                    if (pair->GetObj2()->instanceName == protective_cover_name) {
                        assert(pair->GetHandler()->class_name == "ProtectiveCover");
                        protective_cover = std::static_pointer_cast<ProtectiveCover>(pair->GetHandler());
                    }
                }
                if (protective_cover == nullptr) {
                    throw std::runtime_error("Cannot find corresponding ProtectiveCover for: " + obj2_instanceName);
                }
                if (!protective_cover->is_open()) {
                    return;
                }
                else {
                    targetStateIndex = (currentStateIndex + 1) % allSateSize;
                }
            }
            if(currentStateIndex != targetStateIndex){
                animationPlaying = true;
                // 将交互动作传递到渲染模块
                sceneData.actionPassage.modelName = obj2->modelName;
                sceneData.actionPassage.instanceName = obj2->instanceName;
                sceneData.actionPassage.originState = animationSate->allStates[currentStateIndex];
                sceneData.actionPassage.targetState = animationSate->allStates[targetStateIndex];
                sceneData.actionPassage.instanceId = obj2->instanceId;
                currentStateIndex = targetStateIndex;
            }

            // Log interaction
            _frameDataPtr->interactionLog.curLGesture = _frameDataPtr->gestureDataPtr->curLGesture;
            _frameDataPtr->interactionLog.curRGesture = _frameDataPtr->gestureDataPtr->curRGesture;
            _frameDataPtr->interactionLog.interactionType = "ButtonUnderCover";
            _frameDataPtr->interactionLog.targetModelName = obj2->modelName;
            _frameDataPtr->interactionLog.targetInstanceName = obj2->instanceName;
            _frameDataPtr->interactionLog.targetInstanceID = obj2->instanceId;
            _frameDataPtr->interactionLog.currentActionState = sceneData.actionPassage.originState;
            _frameDataPtr->interactionLog.targetActionState = sceneData.actionPassage.targetState;
            _frameDataPtr->interactionLog.timestamp = getCurrentTimestamp();
            _frameDataPtr->interactionLog.frameID = _frameDataPtr->frameID;
        }
    private:
        bool animationPlaying = false;
        int currentStateIndex = 1;
        int targetStateIndex = 1;
    };

    class Dial : public CollisionHandler {
    public:
        MakeMyCollisionHandler(Dial)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->modelName << " and " << obj2->modelName << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
//            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
//            animator = animation_player->findAnimator(obj2->modelName);
//            if (animator == nullptr) {
//                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
//                animator = std::make_shared<Animator::Animator>(
//                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
//                animation_player->addAnimator(obj2->modelName, animator);
//            }
//            if (animator->isPlaying == false) {
//                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
//                if (cur_right_hand_gesture == Gesture::GRASP) {
//                    if (_frameDataPtr->tip_movement == "up") {
//                        if (animator->currentStateIndex == animator->allStates.size() - 1) {
//                            return;
//                        }
//                        animator->nextStateIndex = animator->currentStateIndex + 1;
//                    }
//                    else if (_frameDataPtr->tip_movement == "down") {
//                        if (animator->currentStateIndex == 0) {
//                            return;
//                        }
//                        animator->nextStateIndex = animator->currentStateIndex - 1;
//                    }
//                    else {
//                        return;
//                    }
//                }
//                if (animator->currentStateIndex != animator->nextStateIndex) {
//                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
//                    animator->isPlaying = true;
//                    animator->animationSequenceIndex = 0;
//                }
//            }
            if(!animationPlaying){
                currentStateIndex = obj2->originStateIndex;
            }
            auto animationState = cadDataManager::DataInterface::getAnimationStateByName(obj2->modelName, obj2->instanceName);
            int allSateSize = animationState->allStates.size();
            // get gesture here and set animator state by gesture
            auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
            //if (cur_right_hand_gesture == Gesture::ZOOM) {
                if(_frameDataPtr->tip_velocity[1] < -0.002){
                    targetStateIndex = currentStateIndex + 1;
                    targetStateIndex = targetStateIndex > allSateSize - 1 ? allSateSize - 1 : targetStateIndex;
                }else if(_frameDataPtr->tip_velocity[1] > 0.002){
                    targetStateIndex = currentStateIndex - 1;
                    targetStateIndex = targetStateIndex < 0 ? 0 : targetStateIndex;
                }
            //}
            if(currentStateIndex != targetStateIndex){
                animationPlaying = true;
                // 将交互动作传递到渲染模块
                sceneData.actionPassage.modelName = obj2->modelName;
                sceneData.actionPassage.instanceName = obj2->instanceName;
                sceneData.actionPassage.originState = animationState->allStates[currentStateIndex];
                sceneData.actionPassage.targetState = animationState->allStates[targetStateIndex];
                sceneData.actionPassage.instanceId = obj2->instanceId;
                currentStateIndex = targetStateIndex;
            }

            // Log interaction
            _frameDataPtr->interactionLog.curLGesture = _frameDataPtr->gestureDataPtr->curLGesture;
            _frameDataPtr->interactionLog.curRGesture = _frameDataPtr->gestureDataPtr->curRGesture;
            _frameDataPtr->interactionLog.interactionType = "Dial";
            _frameDataPtr->interactionLog.targetModelName = obj2->modelName;
            _frameDataPtr->interactionLog.targetInstanceName = obj2->instanceName;
            _frameDataPtr->interactionLog.targetInstanceID = obj2->instanceId;
            _frameDataPtr->interactionLog.currentActionState = sceneData.actionPassage.originState;
            _frameDataPtr->interactionLog.targetActionState = sceneData.actionPassage.targetState;
            _frameDataPtr->interactionLog.timestamp = getCurrentTimestamp();
            _frameDataPtr->interactionLog.frameID = _frameDataPtr->frameID;
        }
    private:
        bool animationPlaying = false;
        int currentStateIndex = 0;
        int targetStateIndex = 0;
    };
}

#endif //ROKIDOPENXRANDROIDDEMO_MYCOLLISIONHANDLERS_H