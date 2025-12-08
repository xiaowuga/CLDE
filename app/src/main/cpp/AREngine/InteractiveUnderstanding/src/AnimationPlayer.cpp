//
// Created by xiaow on 2025/9/13.
//
#include "AnimationPlayer.h"

int AnimationPlayer::Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
    // create the mapping: obj_name -> sceneObject
    std::map<std::string, SceneObjectPtr> scene_objects_map;
    for (auto scene_obj : sceneData.sceneObjects) {
        scene_objects_map[scene_obj->name] = scene_obj;
    }
    // create animators for the interactive scene object in all collision pairs
    std::ifstream instance_config_json_file(appData.dataDir + "InteractionConfig.json");
    if (!instance_config_json_file.is_open()) {
        throw std::runtime_error("InteractionConfig.json not found");
    }
    nlohmann::json instance_config_json;
    instance_config_json_file >> instance_config_json;
    try {
        for (auto collision_pair: instance_config_json.at("CollisionPairs")) {
            std::string interactive_obj_name = collision_pair.at("Obj2");
            int initial_animation_state = collision_pair.at("InitialAnimationState");
            auto it = scene_objects_map.find(interactive_obj_name);
            if (it == scene_objects_map.end()) {
                throw std::runtime_error("cannot find SceneObject: " + interactive_obj_name);
            }
            SceneObjectPtr interactive_scene_obj = it->second;
            animators.emplace(interactive_obj_name,
                              std::make_shared<Animator::Animator>(
                                      initial_animation_state,
                                      interactive_scene_obj,
                                      appData.dataDir + "InstanceState.json"));
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    appData.setData("AnimationPlayer", this);
    return STATE_OK;
}

int AnimationPlayer::Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameData) {
    for (auto& [name, animator_ptr] : animators) {
        if (animator_ptr && animator_ptr->isPlaying) {
            animator_ptr->PlayNextFrame();
        }
    }
    return STATE_OK;
}