//
// Created by xiaow on 2025/8/28.
//

//#include"arengine.h"
#include"scene.h"

//#include "arucopp.h"
#include "utilsmym.hpp"
#include "demos/model.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <common/xr_linear.h>

#include "ARInput.h"
//#include "CameraTracking.h"
#include "RenderingGlass/RenderClient.h"

namespace {


    std::shared_ptr<ARApp> construct_engine(){
        std::string appName="CameraTracking"; //APP名称，必须和服务器注册的App名称对应（由服务器上appDir中文件夹的名称确定）

        std::vector<ARModulePtr> modules;
        modules.push_back(createModule<ARInputs>("ARInputs"));
//        modules.push_back(createModule<CameraTracking>("CameraTracking"));
//        modules.push_back(createModule<MarkerLocation>("MarkerLocation"));
        auto appData=std::make_shared<AppData>();
        auto sceneData=std::make_shared<SceneData>();

        appData->argc=1;
        appData->argv=nullptr;
        appData->engineDir="./AREngine/";  // for test
        appData->dataDir="/storage/emulated/0/AppVer2Data/";
        appData->offlineDataDir = "";
        // Map setting
        appData->isLoadMap = false;
        appData->isSaveMap = true;

        std::shared_ptr<ARApp> app=std::make_shared<ARApp>();
        app->init(appName,appData,sceneData,modules);
        return app;
    }


    class SceneBuildMap : public IScene{

    public:
        virtual bool initialize(const XrInstance instance,const XrSession session){

            _eng=construct_engine();
            std::string dataDir = _eng->appData->dataDir;

//            _eng->connectServer("192.168.1.100", 1123);
            _eng->start();


            return true;
        }
        virtual void renderFrame(const XrPosef &pose,const glm::mat4 &project,const glm::mat4 &view,int32_t eye){ //由于接口更改，以前的renderFrame函数不再适用，换用以下写法(2025-06-17)
//            auto frameData=std::make_shared<FrameData>();
//
            if (_eng) {
                auto frameData = _eng->frameData;

                if (frameData) {
                    auto ccm = frameData->colorCameraMatrix;
                    int c = frameData->image[0].channels();
                }
            }

        }

        virtual void close(){
            if(_eng) _eng->stop();
        }



    public:
        std::shared_ptr<ARApp> _eng;

    };

}


std::shared_ptr<IScene> _createScene_BuildMap(){
    return std::make_shared<SceneBuildMap>();
}