//
// Created by xiaow on 2025/9/13.
//

#include "PoseEstimationRokid.h"
#include "demos/cube.h"
#include <openxr/openxr.h>
#include <common/xr_linear.h>
#include <glm/gtc/type_ptr.hpp>

RokidHandPose* RokidHandPose::instance() {
    static RokidHandPose *ptr = nullptr;
    if(!ptr) {
        ptr = new RokidHandPose();
        ptr->hand_pose.resize(2);
        ptr->hand_pose[0].tag = hand_tag::Left;
        ptr->hand_pose[1].tag = hand_tag::Right;
        ptr->joint_loc.reserve(HAND_COUNT * 21);
    }
    return ptr;
}

void RokidHandPose::set(const XrHandJointLocationEXT *location) {
    std::unique_lock<std::shared_mutex> _lock(_dataMutex);
    joint_loc.clear();
    for (auto hand = 0; hand < HAND_COUNT; hand++) {
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
            int index = hand * XR_HAND_JOINT_COUNT_EXT + i;
            const XrHandJointLocationEXT& jointLocation = location[index];
            if (jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT && jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {

                int idx = mapping[i];
                if(idx == -1)
                    continue;
                XrMatrix4x4f m{};
                XrVector3f scale{1.0f, 1.0f, 1.0f};
                XrMatrix4x4f_CreateTranslationRotationScale(&m, &jointLocation.pose.position, &jointLocation.pose.orientation, &scale);

                glm::mat4 mat = glm::make_mat4((float*)&m);
                // fix: 20251005-kylee
                hand_pose[hand].joints[idx] = cv::Vec3f(jointLocation.pose.position.x, jointLocation.pose.position.y, jointLocation.pose.position.z);
                // end
                joint_loc.emplace_back(mat);
            }
        }
    }
}

const std::vector<HandPose> &RokidHandPose::get_hand_pose() const {
    return hand_pose;
}

const std::vector<glm::mat4>& RokidHandPose::get_joint_loc() const {
    return joint_loc;
}


int PoseEstimationRokid::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    return STATE_OK;
}

int PoseEstimationRokid::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    auto instance = RokidHandPose::instance();
    glm::mat4 alignTransMap2Cockpit = frameDataPtr->alignTransMap2Cockpit;
    glm::mat4 alignTransTracking2Map = frameDataPtr->alignTransTracking2Map;

    const auto& srcHandPoses = instance->get_hand_pose();

    std::lock_guard<std::mutex> guard(frameDataPtr->handPoses_mtx);
    if (frameDataPtr->handPoses.size() != srcHandPoses.size()) {
        frameDataPtr->handPoses.resize(srcHandPoses.size());
    }

    for(int i = 0; i < srcHandPoses.size(); i++) {
        frameDataPtr->handPoses[i].tag = srcHandPoses[i].tag;
        const auto& srcJoints = srcHandPoses[i].joints;
        auto& dstJoints = frameDataPtr->handPoses[i].joints;
        for (int j = 0; j < srcJoints.size(); ++j) {
            auto p =  alignTransMap2Cockpit * alignTransTracking2Map * glm::vec4(srcJoints[j].val[0], srcJoints[j].val[1], srcJoints[j].val[2], 1.0f);
            dstJoints[j] = cv::Vec3f(p.x, p.y, p.z);
        }
    }
    // end

    const auto& srcJointLocs = instance->get_joint_loc();
    if (this->joint_loc.size() != srcJointLocs.size()) {
        this->joint_loc.resize(srcJointLocs.size());
    }
//#pragma omp parallel for
    for(int i = 0;  i < joint_loc.size(); i++) {
        this->joint_loc[i] = alignTransMap2Cockpit * alignTransTracking2Map * srcJointLocs[i];
    }

    return STATE_OK;
}

int PoseEstimationRokid::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){

    return STATE_OK;
}


int PoseEstimationRokid::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int PoseEstimationRokid::ShutDown(AppData &appData,SceneData &sceneData){
    return STATE_OK;
}


std::vector<glm::mat4>& PoseEstimationRokid::get_joint_loc() {
    return joint_loc;
}
