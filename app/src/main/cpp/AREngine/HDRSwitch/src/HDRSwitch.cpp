#include "HDRSwitch.h"
// #include "debug_utils.hpp"

#include <string>
#include <fstream>
#include <unistd.h>
#include <thread>

#include <cstring>

using namespace cv;
using namespace std;


int HDRSwitch::Init(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {
    environmentalState = appData.environmentalState;
    return STATE_OK;
}

int HDRSwitch::Update(AppData &appData, SceneData &sceneData, FrameDataPtr frameDataPtr) {
    appData.environmentalState = environmentalState;
    return STATE_OK;
}


int HDRSwitch::CollectRemoteProcs(SerilizedFrame &serilizedFrame,
                                  std::vector<RemoteProcPtr> &procs, FrameDataPtr frameDataPtr) {
    SerilizedObjs cmdSend = {
            {"cmd", std::string("HDRSwitchGlass")}
    };

    procs.push_back(std::make_shared<RemoteProc>(this,frameDataPtr,cmdSend,
                                                 RPCF_SKIP_BUFFERED_FRAMES));
    return STATE_OK;
}
int HDRSwitch::ProRemoteReturn(RemoteProcPtr proc) {
    auto& send = proc->send;
    auto& ret = proc->ret;
    auto cmd = send.getd<std::string>("cmd");
    if (cmd == "HDRSwitchGlass") {
        environmentalState = ret.getd<int>("environmentalState", 0);
//        LOGI()
    }
    return STATE_OK;
}

int HDRSwitch::ShutDown(AppData &appData, SceneData &sceneData)
{
    return STATE_OK;
}
