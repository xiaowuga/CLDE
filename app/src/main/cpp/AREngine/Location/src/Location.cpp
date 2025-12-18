//
// Created by xiaow on 2025/9/7.
//

#include <iostream>
#include <Location.h>
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


glm::mat4 CVPose2GLMPoseMat(cv::Mat cvMat) {
    // 1. 基础转换：OpenCV (Row-Major) -> GLM (Column-Major)
    // 此时得到的 glmMat 数学意义上等于 cvMat
    glm::mat4 glmMat = glm::make_mat4(cvMat.ptr<float>());
    glmMat = glm::transpose(glmMat);

    // 2. 坐标系修正：OpenCV -> OpenGL
    // OpenCV: +Y Down, +Z Forward
    // OpenGL: +Y Up,   -Z Forward
    // 构建修正矩阵：Scale(1, -1, -1) 等同于绕 X 轴旋转 180 度
    glm::mat4 yz_flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));

    // 3. 应用修正
    // 假设 cvMat 是 View Matrix (World -> Camera)，修正矩阵应该左乘
    // 逻辑：先进行 cvMat 变换(到了CV相机系)，再进行翻转(到了GL相机系)
    return yz_flip * glmMat;
}

cv::Mat RT2Matrix(const cv::Vec3d &rvec, const cv::Vec3d &tvec) {
    // 1. 异常值检查
    if (std::isinf(tvec[0]) || std::isnan(tvec[0]) || std::isinf(rvec[0]) || std::isnan(rvec[0]))
        return cv::Mat::eye(4, 4, CV_64F); // 建议返回 double 类型以匹配输入

    // 2. 罗德里格斯变换：将旋转向量转换为 3x3 旋转矩阵
    cv::Matx33d R;
    cv::Rodrigues(rvec, R);

    // 3. 初始化 4x4 单位矩阵
    cv::Mat T = cv::Mat::eye(4, 4, CV_64F);

    // 4. 填充旋转部分 (左上角 3x3)
    // 使用 T(Rect) 获取子矩阵并赋值
    cv::Mat(R).copyTo(T(cv::Rect(0, 0, 3, 3)));

    // 5. 填充平移部分 (第 4 列，即索引 3)
    T.at<double>(0, 3) = tvec[0];
    T.at<double>(1, 3) = tvec[1];
    T.at<double>(2, 3) = tvec[2];

    return T;
}

cv::Mat GLMPose2CVPoseMat(const glm::mat4& glmPose) {
    // 1. 轴向修正
    glm::mat4 axisFix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, -1.0f));
    glm::mat4 cvPoseGLM = glmPose * axisFix;

    // 2. 转为 OpenCV Mat (注意转置问题)
    cv::Mat cvMat = cv::Mat(4, 4, CV_32F);
    // 这里利用 memcpy + transpose 技巧
    // GLM(Col) -> memcpy -> OpenCV(Row) = 自动转置了
    std::memcpy(cvMat.data, glm::value_ptr(cvPoseGLM), 16 * sizeof(float));
    return cvMat.t(); // 显式转置以获得正确的 Row-Major 矩阵
}

std::vector<glm::mat4> interpolatePose(const glm::mat4& startMat, const glm::mat4& endMat, int steps) {
    std::vector<glm::mat4> path;

    // 保护措施：如果步数太少，直接返回终点或起点
    if (steps <= 0) return path;
    if (steps == 1) {
        path.push_back(startMat);
        return path;
    }

    // 预分配内存，防止频繁 realloc
    path.reserve(steps);

    // -------------------------------------------------
    // 1. 提取基础数据 (分解)
    // -------------------------------------------------
    // 提取位移 (GLM 第4列对应内存最后4个数，即你的最后一行)
    glm::vec3 p0 = glm::vec3(startMat[3]);
    glm::vec3 p1 = glm::vec3(endMat[3]);

    // 提取旋转 (转为四元数)
    glm::quat q0 = glm::quat_cast(startMat);
    glm::quat q1 = glm::quat_cast(endMat);

    // -------------------------------------------------
    // 2. 循环生成插值
    // -------------------------------------------------
    for (int i = 0; i < steps; ++i) {
        // 计算当前进度 t (从 0.0 到 1.0)
        // i=0 时 t=0 (起点)
        // i=steps-1 时 t=1 (终点)
        float t = (float)i / (float)(steps - 1);

        // A. 位置线性插值 (LERP)
        glm::vec3 pt = glm::mix(p0, p1, t);

        // B. 旋转球面插值 (SLERP)
        // GLM 的 slerp 会自动处理最短路径
        glm::quat qt = glm::slerp(q0, q1, t);

        // C. 重组矩阵
        glm::mat4 mat = glm::mat4_cast(qt); // 旋转部分
        mat[3] = glm::vec4(pt, 1.0f);       // 位移部分 (填入最后4个float)

        path.push_back(mat);
    }
    std::reverse(path.begin(), path.end());

    return path;
}


int Location::Init(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){
    std::string dataDir = appData.dataDir;
    _detector.loadTemplate(dataDir + "templ_2.json");
    _detector2.loadTemplate(dataDir + "templ_3.json");
//    marker = glm::make_mat4(new float[16]{
//            1.00000000,
//            0,
//            0,
//            0.00000000,
//
//            0,
//            0.00000000,
//            1.00000000,
//            0.00000000,
//
//            0,
//            -1.00000000,
//            0,
//            0.00000000,
//
//            -0.268250488,
//            -0.897835083,
//            0.588000000,
//            1.00000000
//    });

    marker = glm::make_mat4(new float[16]{
            1.00000000,
            0,
            0,
            0.00000000,

            0,
            0.00000000,
            1.00000000,
            0.00000000,

            0,
            -1.00000000,
            0,
            0.00000000,

            0.278194919,
            -0.117810422,
            0.515670925,
            1.00000000
    });
    marker2 = marker;

    // 旋转角度
    float angleX = glm::radians(-90.0f);
    float angleY = glm::radians(180.0f);
    float angleZ = glm::radians(0.0f);

    // 分别绕各轴旋转
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), angleZ, glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 rotationMatrix = rotationX * rotationY * rotationZ;

    marker = marker * rotationMatrix;
    marker_inv = glm::inverse(marker);
//    marker = F * marker * F;
    markerPose = glm::mat4(1.0);
    markerPose2 = glm::mat4(1.0);
    diff = glm::mat4(1.0);
    cache.clear();
    lastTrans = glm::mat4(1.0);
    trans = glm::mat4(1.0);
    return STATE_OK;
}

int Location::Update(AppData &appData,SceneData &sceneData,FrameDataPtr frameDataPtr){

    auto frame_data=std::any_cast<ARInputSources::FrameData>(sceneData.getData("ARInputs"));
    glm::mat4 T_wc_glm = frame_data.cameraMat;

    if(!frameDataPtr->image.empty()){
        cv::Mat img=frameDataPtr->image.front(); //相机图像
        const cv::Matx33f &camK = frameDataPtr->colorCameraMatrix;

        cv::Vec3d rvec, tvec;
        if (_detector.detect(img, camK, rvec, tvec)) {
            cv::Mat T_cm_cv = RT2Matrix(rvec, tvec);
            glm::mat4 T_cm_glm = CVPose2GLMPoseMat(T_cm_cv);
            markerPose = T_wc_glm * T_cm_glm;
//            trans = marker * glm::inverse(markerPose);
        }
    }


    std::shared_lock<std::shared_mutex> _lock(_dataMutex);
    if(cache.size()==0) {
        cache = interpolatePose(lastTrans, trans,30);
        lastTrans = trans;
    }
//    frameDataPtr->viewRelocMatrix = glm::inverse(cache.back());
//    frameDataPtr->jointRelocMatrix = cache.back();
   frameDataPtr->transformCG = cache.back();
    frameDataPtr->transformGC = glm::inverse(cache.back());


//    float angleX = glm::radians(0.0f);
//    float angleY = glm::radians(180.0f);
//    float angleZ = glm::radians(90.0f);
//
//    // 分别绕各轴旋转
//    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1.0f, 0.0f, 0.0f));
//    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0.0f, 1.0f, 0.0f));
//    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), angleZ, glm::vec3(0.0f, 0.0f, 1.0f));
//    glm::mat4 rotationMatrix = rotationX * rotationY * rotationZ;

//    frameDataPtr->modelRelocMatrix =   marker  * diff  * rotationMatrix ;
    cache.pop_back();
    return STATE_OK;
}

int Location::CollectRemoteProcs(SerilizedFrame &serilizedFrame,std::vector<RemoteProcPtr> &procs,FrameDataPtr frameDataPtr){

    return STATE_OK;
}

int Location::ProRemoteReturn(RemoteProcPtr proc) {

    return STATE_OK;
}

int Location::ShutDown(AppData &appData,SceneData &sceneData){

    return STATE_OK;
}