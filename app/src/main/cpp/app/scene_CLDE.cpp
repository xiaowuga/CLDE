//
// Created by xiaow on 2025/8/28.
//

#include"scene.h"
#include "utilsmym.hpp"
#include "markerdetector.hpp"
#include "demos/model.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <common/xr_linear.h>

#include "ARInput.h"
#include "Location.h"
#include "CameraTracking.h"
#include "PoseEstimationRokid.h"


#include "GestureUnderstanding.h"
#include "CollisionDetection.h"
#include "AnimationPlayer.h"
#include "MyCollisionHandlers.h"
#include "InteractionLogUpload.h"
#include "HDRSwitch.h"

#include "RenderingGlass/RenderClient.h"


namespace {


    std::shared_ptr<ARApp> construct_engine(){
        std::string appName="CameraTracking"; //APP名称，必须和服务器注册的App名称对应（由服务器上appDir中文件夹的名称确定）

        std::vector<ARModulePtr> modules;
        modules.push_back(createModule<ARInputs>("ARInputs"));
        modules.push_back(createModule<CameraTracking>("CameraTracking"));
        modules.push_back(createModule<Location>("Location"));

        modules.push_back(createModule<HDRSwitch>("HDRSwitch"));
        modules.push_back(createModule<PoseEstimationRokid>("PoseEstimationRokid"));
        modules.push_back(createModule<GestureUnderstanding>("GestureUnderstanding"));
        modules.push_back(createModule<CollisionDetection>("CollisionDetection"));
        modules.push_back(createModule<AnimationPlayer>("AnimationPlayer"));
//        modules.push_back(createModule<InteractionLogUpload>("InteractionLogUpload"));
        auto appData=std::make_shared<AppData>();
        auto sceneData=std::make_shared<SceneData>();


        appData->argc=1;
        appData->argv=nullptr;

        appData->engineDir="./AREngine/";  // for test
        appData->dataDir="/storage/emulated/0/AppVer2Data/";        // for test
        appData->interactionConfigFile = "InteractionConfig.json";
        appData->offlineDataDir = appData->dataDir + "CameraTracking/GlassOfflineData";
        appData->animationActionConfigFile = appData->dataDir + "CockpitAnimationAction.json";
        appData->animationStateConfigFile = appData->dataDir + "CockpitAnimationState.json";

        // Map setting
        appData->isLoadMap = false;
        appData->isSaveMap = false;
        appData->isBuildMap = false;
        appData->updateMarkerPoseInMap = false;
        appData->isCaptureOfflineData = false;
        appData->environmentalState = 0;
        std::vector<std::string> model_list = {"di0", "di1", "di2", "di3", "di5",
                                               "di7", "di8",
//                                                "Marker",
                                               "ranyoukongzhi", "shang1(you)", "shang1",
                                               "TUILIGAN",
                                               "YIBIAOPAN",
                                               "zhong1", "zhong2",
                                               "zhongyou", "zhongzuo", "zhongzuo1"};

        for(int i = 0; i < model_list.size(); i++) {
            std::string model_name = model_list[i];
            Pose transform(cv::Matx44f::eye());
            Pose initTransform(cv::Matx44f::eye());
            std::string mesh_file_path = appData->dataDir + "Models";
            auto ptr = std::make_shared<SceneModel>();
            ptr->name = model_name;
            ptr->fileName = model_name + ".fb";
            ptr->filePath = mesh_file_path;
            cadDataManager::DataInterface::parseLocalModel( model_name + ".fb", mesh_file_path);
            sceneData->setObject(model_name, ptr);
        }

        std::vector<float> matrixModify = { -0.049222, 0.998740, 0.009768, 0.000000,
                                            0.956004, 0.044280, 0.289991, 0.000000,
                                            0.289193, 0.023612, -0.956979, 0.000000,
                                            0.0, 0.0, 1510.213, 1.000000};

        cadDataManager::DataInterface::setActiveDocumentData("YIBIAOPAN");
        cadDataManager::DataInterface::modifyInstanceMatrix("52", matrixModify);


        std::shared_ptr<ARApp> app=std::make_shared<ARApp>();
        app->init(appName,appData,sceneData,modules);

        return app;
    }


    class SceneCLDE : public IScene{

    public:
        virtual bool initialize(const XrInstance instance,const XrSession session){

            _eng=construct_engine();
            std::string dataDir = _eng->appData->dataDir;
            Rendering = createModule<RenderClient>("RenderClient");

            auto frameData = _eng->frameData;
            Rendering->Init(*_eng->appData.get(), *_eng->sceneData.get(), frameData);
            if(_eng->appData->isLoadMap || _eng->appData->isBuildMap)
                _eng->connectServer("192.168.1.100", 1203);
            _eng->start();


            return true;
        }

        virtual void renderFrame(const XrPosef &pose,const glm::mat4 &project,const glm::mat4 &view,int32_t eye){ //由于接口更改，以前的renderFrame函数不再适用，换用以下写法(2025-06-17)

            auto frameData=std::make_shared<FrameData>();

            if (_eng) {
                std::shared_ptr<PoseEstimationRokid> poseEstimationRokidPtr = std::static_pointer_cast<PoseEstimationRokid>(_eng->getModule("PoseEstimationRokid"));
                if(poseEstimationRokidPtr != nullptr) {
                    std::vector<glm::mat4> &joc = poseEstimationRokidPtr->get_joint_loc();
                    Rendering->joc = joc;
                }
                std::shared_ptr<CollisionDetection> collisionDetectionPtr = std::static_pointer_cast<CollisionDetection>(_eng->getModule("CollisionDetection"));
                if(collisionDetectionPtr != nullptr) {
                    Rendering->boundingBoxArray = collisionDetectionPtr->GetBoundingBoxArray();
                    Rendering->boundingBoxMap = collisionDetectionPtr->GetBoundingBoxMap();
                }
                auto& frameDataPtr = _eng->frameData;
                if(frameDataPtr) {
                    glm::mat4 alignTrans = frameDataPtr->alignTrans;
                    Rendering->project = project;
                    Rendering->view =  view * glm::inverse(alignTrans); //位姿对齐矩阵的逆是视图对齐矩阵
                    Rendering->Update(*_eng->appData.get(), *_eng->sceneData.get(), frameDataPtr);
                }

            }

        }

        virtual void close(){
            if(_eng) _eng->stop();
        }



    public:
        std::shared_ptr<ARApp> _eng;
        std::shared_ptr<RenderClient> Rendering = createModule<RenderClient>("RenderClient");

    };

}


std::shared_ptr<IScene> _createScene_CLDE(){
    return std::make_shared<SceneCLDE>();
}