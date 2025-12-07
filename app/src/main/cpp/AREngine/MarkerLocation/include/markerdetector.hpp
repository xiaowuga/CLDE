#pragma once
#ifndef ROKIDOPENXRANDROIDDEMO_MARKERDETECTOR_HPP
#define ROKIDOPENXRANDROIDDEMO_MARKERDETECTOR_HPP


#include <utility>

#include"opencv2/opencv.hpp"
#include"glm/glm.hpp"
#include"app/utilsmym.hpp"
#include"utils.h"

class MarkerDetector{
public:
    explicit MarkerDetector(cv::Mat camera_matrix={},cv::Mat dist_coeffs={cv::Mat::zeros(5,1,CV_64F)}):cameraMatrix(std::move(camera_matrix)),distCoeffs(std::move(dist_coeffs)){}
    ~MarkerDetector()=default;

    //************************** 检测Aruco *****************************

    static glm::mat4 GetTransMatFromRT(const cv::Vec3d &rvec,const cv::Vec3d &tvec,const glm::mat4 &vmat){
        auto tmp1=FromRT(cv::Vec3f(0,0,0),cv::Vec3f(0,0,0),true);
        auto tmp_false=(tmp1*FromRT(rvec,tvec,false)).t();
        glm::mat4 trans=glm::inverse(vmat)*CV_Matx44f_to_GLM_Mat4(tmp_false);
        return trans;
    }
    /*
     * 返回值: <aruco_id(int),aruco_corners(vector)> 的列表，获取 @image 中所有类型为 @aruco_type 的aruco列表
     * @image: 可接受彩色图，不需要提前转换为灰度图
     * @aruco_type: 待检测aruco的类型
     */
    std::vector<std::tuple<int,std::vector<cv::Point2f>>> detect_aruco_list(const cv::Mat &image,cv::aruco::PredefinedDictionaryType aruco_type){
        std::vector<int> markerIds;
        std::vector<std::vector<cv::Point2f>> markerCorners, rejectedCandidates;
        cv::aruco::DetectorParameters detectorParams = cv::aruco::DetectorParameters();
        cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(aruco_type);
        cv::aruco::ArucoDetector detector(dictionary, detectorParams);
        detector.detectMarkers(image,markerCorners,markerIds,rejectedCandidates);
        std::vector<std::tuple<int,std::vector<cv::Point2f>>> res;
        res.reserve((int)markerIds.size());
        for(int i=0;i<(int)markerIds.size();++i) res.emplace_back(markerIds[i],markerCorners[i]);
        return res;
    }
    /*
     * 返回值: <detect_result,rvec,tvec>
     * @image: 可接受彩色图，不需要提前转换为灰度图
     * @aruco_size: 待检测aruco的实际大小(单位为毫米mm)
     * @aruco_id: 待检测aruco的id，若不提供，则返回检测到的第一个aruco. 若检测到多个该id的aruco，同样返回第一个检测到的
     */
    std::tuple<bool,cv::Vec3d,cv::Vec3d> detect_aruco(const cv::Mat &image,cv::aruco::PredefinedDictionaryType aruco_type,cv::Size aruco_size,int aruco_id=-1){
        std::vector<int> markerIds;
        std::vector<std::vector<cv::Point2f>> markerCorners, rejectedCandidates;
        cv::aruco::DetectorParameters detectorParams = cv::aruco::DetectorParameters();
        cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(aruco_type);
        cv::aruco::ArucoDetector detector(dictionary, detectorParams);
        detector.detectMarkers(image, markerCorners, markerIds, rejectedCandidates);
        int id_matched_index=-1; //符合要求的检测结果下标
        int matched_cnt=0; //有几个符合要求的检测结果
        for(int i=0;i<(int)markerIds.size();i++){
            if((markerIds[i]!=aruco_id)&&aruco_id!=-1) continue;
            ++matched_cnt;
            if(id_matched_index==-1) id_matched_index=i;
        }
        if(id_matched_index==-1) return std::make_tuple(false,cv::Vec3d(),cv::Vec3d()); //没有检测到aruco
        if(matched_cnt>1) warnf(Format("检测到 %d 个Aruco,有 %d 个符合ID: %d, 将采用第 %d 个结果",markerIds.size(),matched_cnt,aruco_id,id_matched_index).c_str());
//        if(markerIds.size()>1) warnf("Detected More than 1 Markers.");
        // 估计相机姿态
        cv::Vec3d rvec,tvec;
        float halfWidth=((float)aruco_size.width/2.0f)*0.001f,halfHeight=((float)aruco_size.height/2.0f)*0.001f; //单位从mm转换为m
        std::vector<cv::Point3f> objectPoints={ //定义标记的 3D 坐标（四个角点的坐标）（以中心为原点，Z=0 的平面上）
                cv::Point3f(-halfWidth, -halfHeight, 0),  // 左下角// 第一个角点 (左下)
                cv::Point3f( halfWidth, -halfHeight, 0),  // 右下角// 第二个角点 (右下)
                cv::Point3f( halfWidth,  halfHeight, 0),  // 右上角// 第三个角点 (右上)
                cv::Point3f(-halfWidth,  halfHeight, 0)   // 左上角// 第四个角点 (左上)
        };
        bool pnp_flag=cv::solvePnP(objectPoints,markerCorners[id_matched_index],cameraMatrix,distCoeffs,rvec,tvec,false);
        if(!pnp_flag) errorf("Detect Aruco: PnP Failed!");
        return std::make_tuple(pnp_flag,rvec,tvec);
    }
    //************************** 检测Chessboard ************************

    /*
     * 返回值: <detect_result,rvec,tvec>
     * @image: 可接受彩色图，不需要提前转换为灰度图
     * @board_size: width棋盘格水平方向多少个格子(列)，height棋盘格竖直方向多少个格子(行). 注意是格子数不是角点数
     * @cell_size: 每一个小格子的尺寸，单位为mm
     */
    std::tuple<bool,cv::Vec3d,cv::Vec3d> detect_chessboard(const cv::Mat &image,cv::Size board_size,cv::Size cell_size){
        cv::Mat gray; //检测chessboard需要灰度图
        if(image.channels()==3) cv::cvtColor(image,gray,cv::COLOR_BGR2GRAY); //彩色图转换灰度图，没有考虑其他格式
        else gray=image;
        board_size.width--; board_size.height--; //格子数转换为角点数
        std::vector<cv::Point2f> imagePoints;
        bool found=cv::findChessboardCorners(gray,board_size,imagePoints,cv::CALIB_CB_ADAPTIVE_THRESH|cv::CALIB_CB_FAST_CHECK|cv::CALIB_CB_NORMALIZE_IMAGE);
        if(!found) return std::make_tuple(false,cv::Vec3d(),cv::Vec3d()); //没有检测到chessboard
        // 提升角点检测精度
        cv::cornerSubPix(gray,imagePoints,cv::Size(11,11),cv::Size(-1,-1),cv::TermCriteria(cv::TermCriteria::EPS+cv::TermCriteria::MAX_ITER,30,0.001));
        std::vector<cv::Point3f> objectPoints;

        double squareWidth=cell_size.width*0.001,squareHeight=cell_size.height*0.001; //转换单位为m
        for(int i=0;i<board_size.height;++i) // 构造棋盘格世界坐标（Z=0）
            for(int j=0;j<board_size.width;++j) objectPoints.emplace_back(j*squareWidth,i*squareHeight,0);

        cv::Vec3d rvec,tvec;
        bool pnp_flag=cv::solvePnP(objectPoints,imagePoints,cameraMatrix,distCoeffs,rvec,tvec);
        if(!pnp_flag) errorf("Detect Chessboard: PnP Failed!");
        return std::make_tuple(pnp_flag,rvec,tvec);
    }
    //************************** 相机参数 *******************************
    void set_camera_matrix(const cv::Mat &mat){cameraMatrix=mat;}
    void set_camera_matrix(double fx, double fy, double cx, double cy){
        cameraMatrix=(cv::Mat_<double>(3,3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    }
    void set_dist_coeffs(const cv::Mat &mat){distCoeffs=mat;}
    void set_dist_coeffs(double k1, double k2, double k3, double p1, double p2){
        distCoeffs=(cv::Mat_<double>(1,5) <<k1, k2, p1, p2, k3);
    }

private:
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs{cv::Mat::zeros(5,1,CV_64F)}; //默认没有畸变
};



#endif //ROKIDOPENXRANDROIDDEMO_MARKERDETECTOR_HPP
