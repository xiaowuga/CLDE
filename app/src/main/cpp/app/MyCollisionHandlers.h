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


    class Stick : public CollisionHandler {
    public:
        MakeMyCollisionHandler(Stick)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->name << " and " << obj2->name << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
            animator = animation_player->findAnimator(obj2->name);
            if (animator == nullptr) {
                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
                animator = std::make_shared<Animator::Animator>(
                        0, obj2->obj, appDataPtr->dataDir + "InstanceState.json");
                animation_player->addAnimator(obj2->name, animator);
            }
            if (animator->isPlaying == false) {
                // get gesture here and set animator state by gesture
                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
                if (cur_right_hand_gesture == Gesture::GRASP) {
                    if(_frameDataPtr->tip_movement == "up") {
                        if (animator->currentStateIndex == animator->allStates.size() - 1) {
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex + 1;
                    }
                    else if(_frameDataPtr->tip_movement == "down") {
                        if (animator->currentStateIndex == 0){
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex - 1;
                    }
                    else {
                        return;
                    }
                }
                if (animator->currentStateIndex != animator->nextStateIndex) {
                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
                    animator->isPlaying = true;
                    animator->animationSequenceIndex = 0;
                }
            }
        }
    };

    class Button : public CollisionHandler {
    public:
        MakeMyCollisionHandler(Button)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->name << " and " << obj2->name << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
            animator = animation_player->findAnimator(obj2->name);
            if (animator == nullptr) {
                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
                animator = std::make_shared<Animator::Animator>(
                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
                animation_player->addAnimator(obj2->name, animator);
            }
            if (animator->isPlaying == false) {
                // get gesture here and set animator state by gesture
                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
                if (cur_right_hand_gesture == Gesture::CURSOR) {
                    animator->nextStateIndex = (animator->currentStateIndex + 1) % animator->allStates.size();
                }
                if (animator->currentStateIndex != animator->nextStateIndex) {
                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
                    animator->isPlaying = true;
                    animator->animationSequenceIndex = 0;
                }
            }
        }

    };

    class ProtectiveCover : public CollisionHandler {
    public:
        MakeMyCollisionHandler(ProtectiveCover)
        bool is_open() {
            return animator->allStates[animator->currentStateIndex] == "OPEN";
        }
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->name << " and " << obj2->name
                      << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
            animator = animation_player->findAnimator(obj2->name);
            if (animator == nullptr) {
                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
                animator = std::make_shared<Animator::Animator>(
                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
                animation_player->addAnimator(obj2->name, animator);
            }
            if (animator->isPlaying == false) {
                // get gesture here and set animator state by gesture
                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
                if (cur_right_hand_gesture == Gesture::GRASP) {
                    if (_frameDataPtr->tip_movement == "up") {
                        if (animator->currentStateIndex == animator->allStates.size() - 1) {
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex + 1;
                    }
                    else if (_frameDataPtr->tip_movement == "down") {
                        if (animator->currentStateIndex == 0) {
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex - 1;
                    }
                    else {
                        return;
                    }
                }
                if (animator->currentStateIndex != animator->nextStateIndex) {
                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
                    animator->isPlaying = true;
                    animator->animationSequenceIndex = 0;
                }
            }
        }
    };

    class ButtonUnderCover : public CollisionHandler {
    public:
        MakeMyCollisionHandler(ButtonUnderCover)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->name << " and " << obj2->name << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
            animator = animation_player->findAnimator(obj2->name);
            if (animator == nullptr) {
                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
                animator = std::make_shared<Animator::Animator>(
                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
                animation_player->addAnimator(obj2->name, animator);
            }
            if (animator->isPlaying == false) {
                // get gesture here and set animator state by gesture
                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
                if (cur_right_hand_gesture == Gesture::CURSOR) {
                    // find the corresponding protective cover's collision handler
                    // obj2_name: xxxxx<k>, then the corresponding protective cover should be named: xxxxx-G<k>
                    auto obj2_name = obj2->name;
                    size_t pos = obj2_name.find('<');
                    if (pos == std::string::npos) {
                        throw std::invalid_argument("Sample name format invalid: missing <k>");
                    }
                    std::string prefix = obj2_name.substr(0, pos);
                    std::string suffix = obj2_name.substr(pos);
                    std::string protective_cover_name = prefix + "-G" + suffix;
                    ProtectiveCover::Ptr protective_cover = nullptr;
                    for (auto pair : sceneData.collisionPairs) {
                        if (pair->GetObj2()->name == protective_cover_name) {
                            assert(pair->GetHandler()->class_name != "ProtectiveCover");
                            protective_cover = std::static_pointer_cast<ProtectiveCover>(pair->GetHandler());
                        }
                    }
                    if (protective_cover == nullptr) {
                        throw std::runtime_error("Cannot find corresponding ProtectiveCover for: " + obj2_name);
                    }
                    if (!protective_cover->is_open()) {
                        return;
                    }
                    else {
                        animator->nextStateIndex =
                                (animator->currentStateIndex + 1) % animator->allStates.size();
                    }
                }
                if (animator->currentStateIndex != animator->nextStateIndex) {
                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
                    animator->isPlaying = true;
                    animator->animationSequenceIndex = 0;
                }
            }
        }
    };

    class Dial : public CollisionHandler {
    public:
        MakeMyCollisionHandler(Dial)
        void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) override {
            std::cout << "Collision detected between " << obj1->name << " and " << obj2->name << std::endl;

            if (trigger_interval() < cool_down_interval) {
                return;
            }
            auto animation_player = std::any_cast<std::shared_ptr<AnimationPlayer>>(appDataPtr->getData("AnimationPlayer"));
            animator = animation_player->findAnimator(obj2->name);
            if (animator == nullptr) {
                // make a new animator referring to the obj2, which is stored in appData->sceneObjects
                animator = std::make_shared<Animator::Animator>(
                        0, obj2->obj, appDataPtr->dataDir + "/InstanceState.json");
                animation_player->addAnimator(obj2->name, animator);
            }
            if (animator->isPlaying == false) {
                auto cur_right_hand_gesture = _frameDataPtr->gestureDataPtr->curRGesture;
                if (cur_right_hand_gesture == Gesture::GRASP) {
                    if (_frameDataPtr->tip_movement == "up") {
                        if (animator->currentStateIndex == animator->allStates.size() - 1) {
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex + 1;
                    }
                    else if (_frameDataPtr->tip_movement == "down") {
                        if (animator->currentStateIndex == 0) {
                            return;
                        }
                        animator->nextStateIndex = animator->currentStateIndex - 1;
                    }
                    else {
                        return;
                    }
                }
                if (animator->currentStateIndex != animator->nextStateIndex) {
                    animator->LoadAnimationForStateChange(animator->allStates[animator->currentStateIndex], animator->allStates[animator->nextStateIndex]);
                    animator->isPlaying = true;
                    animator->animationSequenceIndex = 0;
                }
            }
        }
    };
}

#endif //ROKIDOPENXRANDROIDDEMO_MYCOLLISIONHANDLERS_H