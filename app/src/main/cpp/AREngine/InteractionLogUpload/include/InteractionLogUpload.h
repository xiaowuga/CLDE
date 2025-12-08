//
// Created by xiaow on 2025/12/8.
//

#ifndef APPVER2_INTERACTIONLOGUPLOAD_H
#define APPVER2_INTERACTIONLOGUPLOAD_H

#include "BasicData.h"
#include "ARModule.h"
#include "ARInput.h"
#include "App.h"


#ifdef _WIN32
#include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SOCKET_HANDLE;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET_HANDLE;
#define INVALID_SOCKET -1
#endif

class InteractionLogUpload : public ARModule {
public:
    SOCKET_HANDLE clientSocket = INVALID_SOCKET; // 存储建立的连接句柄
    std::string log_data;                       // 日志数据，在CollectRemoteProcs中填充

    // 假设的服务器配置
    const std::string SERVER_IP = "192.168.31.9"; // 替换为你的目标IP
    const int SERVER_PORT = 1234;                 // 替换为你的目标端口

    // 辅助函数：确保所有字节都被发送（TCP 流式发送的常见问题）
    int sendAll(const char* buf, int len);

    // 辅助函数：清理 Socket 资源
    void cleanupSocket();

public:

    int Init(AppData& appData, SceneData& SceneData, FrameDataPtr frameDataPtr) override;
    int Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) override;
    int ShutDown(AppData& appData, SceneData& sceneData) override;
    int CollectRemoteProcs(
            SerilizedFrame &serilizedFrame,
            std::vector<RemoteProcPtr> &procs,
            FrameDataPtr frameDataPtr) override;
    int ProRemoteReturn(RemoteProcPtr proc) override;
};


#endif //APPVER2_INTERACTIONLOGUPLOAD_H
