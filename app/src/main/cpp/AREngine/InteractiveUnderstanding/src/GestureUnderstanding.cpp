#include "GestureUnderstanding.h"
#include <chrono>
#include <android/log.h>

GestureUnderstanding::GestureUnderstanding() {
    _moduleName = "GestureUnderstanding";
};
GestureUnderstanding::~GestureUnderstanding(){

};

void GestureUnderstanding::PreCompute(std::string configPath) {

};
int GestureUnderstanding::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameData) {
    //初始化手关节点模型列表
    _handNodes.clear();
    //获取相机位置
    //向sceneData中添加预制手关节点模型
    for (int i = 0; i < 42; i++) {
        SceneObjectPtr handNode(new VirtualObject());
		handNode->name = "HandNode_" + std::to_string(i);
        handNode->fileName = handNode->name + ".fb";
        handNode->filePath = appData.dataDir + "handNode";
        handNode->Init();
        _handNodes.push_back(handNode);
        sceneData.setObject(handNode->name, handNode);
    }
    return STATE_OK;

};
int GestureUnderstanding::Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameData) {
    
    //更新手部骨架位置
    frameData->gestureDataPtr = std::make_shared<GestureUnderstandingData>();
    if(frameData->handPoses.size() != 2){
		std::cerr << "HandPose size in frameData less than 2" << std::endl;
        return STATE_ERROR;
    }
    else {
        //更新手势类别

        std::vector<HandPose> handPoses;
        {
            std::lock_guard<std::mutex> guard(frameData->handPoses_mtx);
            HandPose handPoses_0 = frameData->handPoses[0];
            HandPose handPoses_1 = frameData->handPoses[1];
            handPoses.push_back(handPoses_0);
            handPoses.push_back(handPoses_1);
        }
        frameData->gestureDataPtr->curLGesture = _gesturePredictor.predict(handPoses[0]);
        frameData->gestureDataPtr->curRGesture = _gesturePredictor.predict(handPoses[1]);
        __android_log_print(ANDROID_LOG_INFO, "Hand Gesture", "L: %s R: %s", GestureNames[(int)frameData->gestureDataPtr->curLGesture].c_str(), GestureNames[(int)frameData->gestureDataPtr->curRGesture].c_str());
        //更新手关节点模型的位置
		// TODO: 统一坐标系
        //更新相机变化矩阵
        SceneObject* cam = sceneData.getMainCamera();
        int i = 0;
        for (HandPose handPose : handPoses) {
            for (cv::Vec3f joint : handPose.getjoints()) {
				cv::Matx44f jointMat = cv::Matx44f::eye();
                joint *= 1.f;
                // World xyz <---> PoseEsitimation xyz
				jointMat(0, 3) = joint[0];
				jointMat(1, 3) = joint[1];
				jointMat(2, 3) = joint[2];
				// Align with the camera
				cv::Matx44f camInitRot = cam->initTransform.GetRotation().inv();
				cv::Matx44f camInitPos = camInitRot * cam->initTransform.GetMatrix();
				camInitPos(0, 3) = -camInitPos(0, 3);
				camInitPos(1, 3) = -camInitPos(1, 3);
				camInitPos(2, 3) = -camInitPos(2, 3);
//				cv::Matx44f initTransForm = camInitPos * camInitRot * jointMat * _initScale;
                cv::Matx44f initTransForm = jointMat;
				_handNodes[i]->initTransform.setPose(initTransForm);

				cv::Matx44f camRot = cam->transform.GetRotation().inv();
				cv::Matx44f camPos = camRot * cam->transform.GetMatrix();
				camPos(0, 3) = -camPos(0, 3);
				camPos(1, 3) = -camPos(1, 3);
				camPos(2, 3) = -camPos(2, 3);
//                cv::Matx44f transForm = camPos * camRot;
                cv::Matx44f transForm = cv::Matx44f::eye();
                _handNodes[i]->transform.setPose(transForm);
                i++;
            }
        }

        // calculate the velocity of finger tip, we will use it during judging the effects of hand gesture
        std::array<cv::Vec3f, HandPose::jointNum> joints = handPoses[1].getjoints();
        auto index_mcp = joints[(int)HandJoint::INDEX_FINGER_MCP];
        auto middle_mcp = joints[(int)HandJoint::MIDDLE_FINGER_MCP];
        auto ring_mcp = joints[(int)HandJoint::RING_FINGER_MCP];
        auto pinky_mcp = joints[(int)HandJoint::PINKY_MCP];

        auto cur_time = std::chrono::steady_clock::now();
        cv::Vec3f cur_finger_tip_position = (index_mcp + middle_mcp + ring_mcp + pinky_mcp) / 4.0f;
        if (sceneData.is_start) {
            // initialize
            __android_log_print(ANDROID_LOG_DEBUG, "Hand Gesture", "initialize");
            sceneData.is_start = false;
            sceneData.last_time = cur_time;
            sceneData.last_tip_pos = cur_finger_tip_position;
        }
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - sceneData.last_time).count(); // ms
        auto delta_pos = cur_finger_tip_position - sceneData.last_tip_pos;

        auto v = delta_pos / (float)delta_time * 1000.0f;
        frameData->tip_velocity = v;
        if (v.val[2] > 0.09) {
            __android_log_print(ANDROID_LOG_DEBUG, "Hand Gesture", "R Tip Dir: up");
            frameData->tip_movement = "up";
        }
        else if (v.val[2] < -0.09){
            __android_log_print(ANDROID_LOG_DEBUG, "Hand Gesture", "R Tip Dir: down");
            frameData->tip_movement = "down";
        }
        sceneData.last_tip_pos = cur_finger_tip_position;
        sceneData.last_time = cur_time;
    }
    return STATE_OK;
};
int GestureUnderstanding::ShutDown(AppData &appData, SceneData &sceneData) {
    return STATE_OK;
};
int GestureUnderstanding::CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr) {
    return 0;
};
int GestureUnderstanding::ProRemoteReturn(RemoteProcPtr proc) {
    return 0;
};
