//
// Created by xiaow on 2025/9/13.
//

#ifndef ROKIDOPENXRANDROIDDEMO_POSEESTIMATIONROKID_H
#define ROKIDOPENXRANDROIDDEMO_POSEESTIMATIONROKID_H

#include "BasicData.h"
#include "ARModule.h"
#include "glm/glm.hpp"
#include <openxr/openxr.h>
#include <common/xr_linear.h>
#include "App.h"

namespace Side {
    const int LEFT = 0;
    const int RIGHT = 1;
    const int COUNT = 2;
}  // namespace Side

#define HAND_COUNT 2


class RokidHandPose{
public:
    static RokidHandPose* instance();


    void set(const XrHandJointLocationEXT* location);

    const std::vector<HandPose>& get_hand_pose() const;

    const std::vector<glm::mat4>& get_joint_loc() const;

private:
    std::vector<HandPose> hand_pose;
    std::vector<glm::mat4> joint_loc;
    std::shared_mutex  _dataMutex;
    int mapping[26] = {-1,0,1,2,3,4,-1, 5,6,7,8,-1, 9,10,11,12,-1, 13,14,15,16,-1, 17,18,19, 20};

};


class PoseEstimationRokid : public ARModule {
public:
//    std::vector<HandPose> hand_pose;
    std::vector<glm::mat4> joint_loc;
//    std::shared_mutex  _dataMutex;

public:

    int Init(AppData& appData, SceneData& SceneData, FrameDataPtr frameDataPtr) override;
    int Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;
    int ShutDown(AppData& appData, SceneData& sceneData) override;
    int CollectRemoteProcs(
            SerilizedFrame &serilizedFrame,
            std::vector<RemoteProcPtr> &procs,
            FrameDataPtr frameDataPtr) override;
    int ProRemoteReturn(RemoteProcPtr proc) override;

    void setHandJointLocation(const XrHandJointLocationEXT* location);

    std::vector<glm::mat4>& get_joint_loc();


};


#endif //ROKIDOPENXRANDROIDDEMO_POSEESTIMATIONROKID_H
