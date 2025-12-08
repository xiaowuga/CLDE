//
// Created by xiaow on 2025/12/8.
//

#include "InteractionLogUpload.h"

void closeSocket(SOCKET_HANDLE sock) {
#ifdef _WIN32
    // Windows 使用 closesocket()
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
#else
    // Linux/macOS 使用 close()
    if (sock != INVALID_SOCKET) {
        close(sock);
    }
#endif
}


// 辅助函数实现 (用于确保所有数据都发送成功)
int InteractionLogUpload::sendAll(const char* buf, int len) {
    int total = 0; // 实际发送字节数
    int bytesleft = len; // 剩余字节数
    int n;

    while(total < len) {
        n = send(clientSocket, buf + total, bytesleft, 0);
        if (n == -1) { break; } // Socket Error
        total += n;
        bytesleft -= n;
    }
    return (n == -1) ? STATE_ERROR : STATE_OK; // 返回 STATE_OK 或 STATE_ERROR
}

// 辅助函数实现 (清理)
void InteractionLogUpload::cleanupSocket() {
    if (clientSocket != INVALID_SOCKET) {
        closeSocket(clientSocket); // 假设 closeSocket 是跨平台的
        clientSocket = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}


int InteractionLogUpload::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    // Windows 初始化
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return STATE_ERROR;
    }
#endif

    // 1. 创建 Socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        return STATE_ERROR;
    }

    // 2. 准备地址信息
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddr.sin_addr) <= 0) {
        cleanupSocket();
        return STATE_ERROR;
    }

    // 3. 连接服务器
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cleanupSocket();
        return STATE_ERROR;
    }

    return STATE_OK;
}

int InteractionLogUpload::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    log_data =  frameDataPtr->interactionLog.toString();

    if (clientSocket == INVALID_SOCKET || log_data.empty()) {
        return STATE_OK; // 没有连接或没有新日志，直接返回
    }

    // 2. 准备数据长度和数据体
    int data_len = log_data.length();

    // TCP 流协议：必须先告知服务器数据有多长
    // 注意：网络字节序 (htons/htonl) 对 int 也很重要
    uint32_t net_data_len = htonl(data_len);

    // 3. 发送数据长度 (4 字节)
    if (sendAll((const char*)&net_data_len, sizeof(net_data_len)) != STATE_OK) {
        std::cerr << "Error sending log length." << std::endl;
        // 如果发送失败，清理连接并返回错误
        cleanupSocket();
        return STATE_ERROR;
    }

    // 4. 发送日志数据体
    if (sendAll(log_data.c_str(), data_len) != STATE_OK) {
        cleanupSocket();
        return STATE_ERROR;
    }

    // 5. 成功发送后，清空日志缓存，等待下一轮收集
    log_data.clear();
    return STATE_OK;
}

int InteractionLogUpload::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){

    return STATE_OK;
}

int InteractionLogUpload::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int InteractionLogUpload::ShutDown(AppData &appData,SceneData &sceneData){
    cleanupSocket();
    return STATE_OK;
}