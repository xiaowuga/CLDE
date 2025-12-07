//
// Created by xiaow on 2025/9/7.
//

#include <iostream>
#include "MarkerLocation.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

int MarkerLocation::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    std::string dataDir = appData.dataDir;
    _detector.loadTemplate(dataDir + "templ_1.json");
    markerSlamPose = glm::mat4(1.0);
    return STATE_OK;
}

int MarkerLocation::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cv::Mat sendMat,cameraMat; //要发送给服务器的Mat
    //====================== Aruco & Chessboard Detect ===============================
    auto frame_data=std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
    cv::Matx44f cvCamGlassMatFloat = frame_data.cameraMat; //计算marker位姿需要的相机pose,这个也需要发送


    glm::mat4 glmCamGlassMat =  CV_Matx44f_to_GLM_Mat4(cvCamGlassMatFloat);
    auto cp = sceneData.getMainCamera();
    Matx44d cvCamSlamMat = cp->transform.GetMatrix();
    glm::mat4 glmCamSlamMat = CV_Matx44f_to_GLM_Mat4(cvCamSlamMat);
    markerSlamPose = glm::mat4(1.0);
    if(!frameDataPtr->image.empty()){
        cv::Mat img=frameDataPtr->image.front(); //相机图像
        const cv::Matx33f &camK = frameDataPtr->colorCameraMatrix;
        cv::Vec3d rvec, tvec;
        if (_detector.detect(img, camK, rvec, tvec)) {
            cv::Vec3f rvec_float(rvec[0], rvec[1], rvec[2]);
            cv::Vec3f tvec_float(tvec[0], tvec[1], tvec[2]);

            glm::mat4 markerGlassPose = GetTransMatFromRT(rvec, tvec,glmCamGlassMat);

            markerSlamPose = markerGlassPose * glm::inverse(glmCamGlassMat) * glmCamSlamMat;

        }
    }

    return STATE_OK;
}

int MarkerLocation::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){
    SerilizedObjs send = {
            {"cmd", std::string("markloc")},
            {"marker_pose", Bytes(markerSlamPose)}
    };
    procs.push_back(std::make_shared<RemoteProc>(this, frameDataPtr, send));
    return STATE_OK;
}

int MarkerLocation::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int MarkerLocation::ShutDown(AppData &appData,SceneData &sceneData){
    return STATE_OK;
}