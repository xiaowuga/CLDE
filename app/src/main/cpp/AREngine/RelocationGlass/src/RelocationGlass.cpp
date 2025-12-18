//
// Created by xiaow on 2025/9/7.
//

#include <iostream>
#include <RelacationGlass.h>

inline bool StringStartsWith(const std::string &str,const std::string &starts_with){
    if((str.rfind(starts_with,0)==0)) return true; //start with
    return false;
}


inline std::string MakeSdcardPath(const std::string &path) {
    static const std::string SdcardPrefix("/storage/emulated/0/");
    std::string res=path;
    if(!path.empty() && !StringStartsWith(path,SdcardPrefix)) res=SdcardPrefix+res;
    return res;
}

inline glm::mat4 CV_Matx44f_to_GLM_Mat4(const cv::Matx44f &mat) {  // 将 cv::Matx44f 转换为 glm::mat4
    return glm::mat4(
            mat(0, 0), mat(0, 1), mat(0, 2), mat(0, 3),  // 第一行
            mat(1, 0), mat(1, 1), mat(1, 2), mat(1, 3),  // 第二行
            mat(2, 0), mat(2, 1), mat(2, 2), mat(2, 3),  // 第三行
            mat(3, 0), mat(3, 1), mat(3, 2), mat(3, 3)   // 第四行
    );
}


inline cv::Mat Matx44f_to_Mat(const cv::Matx44f& matx) {
    cv::Mat mat(4, 4, CV_32F);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) mat.at<float>(row, col) = matx(row, col);
    return mat;
}

inline void GetGLModelView(const cv::Matx33f &R, const cv::Vec3f &t, float* mModelView, bool cv2gl){ //2025-06-11修改版
    //绕X轴旋转180度，从OpenCV坐标系变换为OpenGL坐标系
    //同时转置，opengl默认的矩阵为列主序
    if (cv2gl){
        mModelView[0] = R(0, 0);
        mModelView[1] = -R(1, 0);
        mModelView[2] = -R(2, 0);
        mModelView[3] = 0.0f;

        mModelView[4] = R(0, 1);
        mModelView[5] = -R(1, 1);
        mModelView[6] = -R(2, 1);
        mModelView[7] = 0.0f;

        mModelView[8] = R(0, 2);
        mModelView[9] = -R(1, 2);
        mModelView[10] = -R(2, 2);
        mModelView[11] = 0.0f;

        mModelView[12] = t(0);
        mModelView[13] = -t(1);
        mModelView[14] = -t(2);
        mModelView[15] = 1.0f;
    }
    else{
//        mModelView[0] = R(0, 0);
//        mModelView[1] = R(1, 0);
//        mModelView[2] = R(2, 0);
//        mModelView[3] = 0.0f;
//
//        mModelView[4] = R(0, 1);
//        mModelView[5] = R(1, 1);
//        mModelView[6] = R(2, 1);
//        mModelView[7] = 0.0f;
//
//        mModelView[8] = R(0, 2);
//        mModelView[9] = R(1, 2);
//        mModelView[10] = R(2, 2);
//        mModelView[11] = 0.0f;
//
//        mModelView[12] = t(0);
//        mModelView[13] = t(1);
//        mModelView[14] = t(2);
//        mModelView[15] = 1.0f;

        mModelView[0] = R(0, 0);
        mModelView[4] = R(1, 0);
        mModelView[8] = R(2, 0);
        mModelView[12] = 0.0f;

        mModelView[1] = R(0, 1);
        mModelView[5] = R(1, 1);
        mModelView[9] = R(2, 1);
        mModelView[13] = 0.0f;

        mModelView[2] = R(0, 2);
        mModelView[6] = R(1, 2);
        mModelView[10] = R(2, 2);
        mModelView[14] = 0.0f;

        mModelView[3] = t(0);
        mModelView[7] = t(1);
        mModelView[11] = t(2);
        mModelView[15] = 1.0f;
    }
}
inline cv::Matx44f FromRT(const cv::Vec3f &rvec, const cv::Vec3f &tvec, bool cv2gl){
    if (isinf(tvec[0]) || isnan(tvec[0]) || isinf(rvec[0]) || isnan(rvec[0]))
        return cv::Matx44f(1.0f, 0.0f, 0.0f, 0.0f,0.0f, 1.0f, 0.0f, 0.0f,0.0f, 0.0f, 1.0f, 0.0f,0.0f, 0.0f, 0.0f, 1.0f);

    cv::Matx33f R;
    cv::Rodrigues(rvec, R);
    cv::Matx44f m;
    GetGLModelView(R, tvec, m.val, cv2gl);
    return m;
}


static glm::mat4 GetTransMatFromRT(const cv::Vec3d &rvec,const cv::Vec3d &tvec,const glm::mat4 &vmat){
    auto tmp1=FromRT(cv::Vec3f(0,0,0),cv::Vec3f(0,0,0),true);
    auto tmp_false=(tmp1*FromRT(rvec,tvec,false)).t();
    glm::mat4 trans=glm::inverse(vmat)*CV_Matx44f_to_GLM_Mat4(tmp_false);
    return trans;
}


int RelocationGlass::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    marker_world_pose = glm::mat4(1.0);
    marker_industrial_camera_pose = glm::mat4(1.0);
    industrial_camera_world_pose = glm::mat4(1.0);
    std::string dataDir = appData.dataDir;
    _detector.loadTemplate(dataDir + "templ_1.json");

    return STATE_OK;
}

int RelocationGlass::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cv::Mat sendMat,cameraMat; //要发送给服务器的Mat
    //====================== Aruco & Chessboard Detect ===============================
    if(!frameDataPtr->image.empty()){
        cv::Mat img=frameDataPtr->image.front(); //相机图像
        auto frame_data=std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
        cv::Matx44f vmat = frame_data.cameraMat; //计算marker位姿需要的相机pose,这个也需要发送
        const cv::Matx33f &camK = frameDataPtr->colorCameraMatrix;

        cameraMat=Matx44f_to_Mat(vmat);
        cv::Vec3d rvec, tvec;
        glm::mat4 glm_vmat = CV_Matx44f_to_GLM_Mat4(vmat);
        if (_detector.detect(img, camK, rvec, tvec)) {
            cv::Vec3f rvec_float(rvec[0], rvec[1], rvec[2]);
            cv::Vec3f tvec_float(tvec[0], tvec[1], tvec[2]);

            marker_world_pose = GetTransMatFromRT(rvec, tvec,CV_Matx44f_to_GLM_Mat4(vmat));
            industrial_camera_world_pose =  marker_industrial_camera_pose * glm::inverse(marker_world_pose);
        }
    }

    return STATE_OK;
}

int RelocationGlass::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){
    auto &frameData=*frameDataPtr;
//            serilizedFrame.addRGBImage(frameData,0,".png"); //测试代码功能需要无损压缩，用png格式

    SerilizedObjs cmdSend = {
            {"cmd", std::string("RelocationGlass")}
    };

    procs.push_back(std::make_shared<RemoteProc>(this,frameDataPtr,cmdSend,
                                                 RPCF_SKIP_BUFFERED_FRAMES)); //添加add命令，将输入帧的像素值加上指定的值，并返回结果图像(参考TestServer.cpp中TestPro1Server类的实现)

    return STATE_OK;
}

int RelocationGlass::ProRemoteReturn(RemoteProcPtr proc) {
    auto &send=proc->send;
    auto &ret=proc->ret;
    auto cmd=send.getd<std::string>("cmd");

    printf("ProRemoteReturn: Relocation cmd=%s, frameID=%d\n",cmd.c_str(),proc->frameDataPtr->frameID);

    if(cmd == "RelocationGlass"){//处理set命令返回值

        auto res =  ret.getd<cv::Matx44f>("aruco_pose");

        marker_industrial_camera_pose = glm::transpose(CV_Matx44f_to_GLM_Mat4(res));
    }

    return STATE_OK;
}

int RelocationGlass::ShutDown(AppData &appData,SceneData &sceneData){
    return STATE_OK;
}