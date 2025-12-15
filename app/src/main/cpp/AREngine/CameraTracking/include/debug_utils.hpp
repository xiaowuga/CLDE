
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <Eigen/Eigen>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <opencv2/core/eigen.hpp>
#include <unistd.h> 


std::string get_tum_string(double pose_timestamp, const cv::Mat& pose_)
{
    // std::cout << "get_tum_string..." << std::endl;
    // transform the pose matrix to CV_32F
    cv::Mat pose;
    // std::cout << "pose_.type():" << pose_.type() << std::endl;
    if (pose_.type() != CV_32F) {
        // std::cout << "pose_.type():" << pose_.type() << std::endl;
        pose_.convertTo(pose, CV_32F);
        // std::cout << "convert pose to CV_32F" << std::endl;
    }
    else
    {
        pose = pose_;
    }
    cv::Mat pose_t = pose.t();
    pose = pose_t;
    std::stringstream ss;

    ss << std::fixed;
    ss << std::setprecision(6);
    // timestamp
    ss << pose_timestamp << " ";
    // translation
    ss << pose.at<float>(0, 3) << " " << pose.at<float>(1, 3) << " " << pose.at<float>(2, 3) << " ";
    // quaternion
    Eigen::Matrix3f R;
    cv::cv2eigen(pose(cv::Rect(0, 0, 3, 3)), R);
    Eigen::Quaternionf q(R);
    ss << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << std::endl;
    // Convert the stringstream to a string and return
    std::string ret = ss.str();
    return ret;
}

void save_pose_as_tum(std::string output_file_path, double pose_timestamp, cv::Mat pose)
{
    std::string output_dir = output_file_path.substr(0, output_file_path.find_last_of("/"));

    if (access(output_dir.c_str(), 0) == -1)
    {
        std::string cmd = "mkdir -p " + output_dir;
        int ret = system(cmd.c_str());
        if(ret != 0){
            std::cerr << "[save_pose_as_tum]Command execution failed with return code: " << ret << std::endl;
        }
    }
    std::ofstream out_file(output_file_path, std::ios::out|std::ios::app);
    std::string ret = get_tum_string(pose_timestamp, pose);
    // std::cout << "get tum string: " << ret << std::endl;
    out_file << ret;
}


//inline void save_pose_as_tum(std::string output_file_path, uint64_t pose_timestamp, cv::Mat pose)
//{
//    std::string output_dir = output_file_path.substr(0, output_file_path.find_last_of("/"));
//
//    if (access(output_dir.c_str(), 0) == -1)
//    {
//        std::string cmd = "mkdir -p " + output_dir;
//        int ret = system(cmd.c_str());
//        if(ret != 0){
//            std::cerr << "[save_pose_as_tum]Command execution failed with return code: " << ret << std::endl;
//        }
//    }
//    std::ofstream out_file(output_file_path, std::ios::out|std::ios::app);
//    std::string ret = get_tum_string(pose_timestamp, pose);
//    // std::cout << "get tum string: " << ret << std::endl;
//    out_file << ret;
//}



void onLBUTTONDOWN (int event, int x, int y, int flags, void *userdata) {
    std::vector<cv::Point2f> &points = *(std::vector<cv::Point2f> *)userdata;
    if (event == cv::EVENT_LBUTTONDOWN) {
    points.push_back(cv::Point2f(x, y));
    std::cout << "p[i]: " << x << " " << y << std::endl;
    }
}

class TrackSelecedPoints {

  std::vector<cv::Point2f> p2d_seleted;
  std::vector<cv::Point3f> points3d;
  cv::Matx33f K{385.128, 0, 326.371, 0, 384.118, 243.675, 0, 0, 1};
  int frame_after_init = 200; // 初始化后第frame_after_init帧开始选点

  public:
  TrackSelecedPoints() {
    const std::string point_file = "data/points.txt";
    // /media/qwe/T7Shield/Linux_share/Result/ShanDong/Reloc/red_pos.asc
    std::ifstream in(point_file);
    if (!in.is_open()) {
      std::cerr << "open file failed" << std::endl;
      return;
    }
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream iss(line);
      float x, y, z;
      iss >> x >> y >> z;
      points3d.push_back(cv::Point3f(x, y, z));
    }
  }

  // 变换点到指定坐标系
  void TransformPoint(cv::Matx44f T) {
  
    for (int i = 0; i < points3d.size(); ++i) {
      cv::Point3f p = points3d[i];
      cv::Matx41f tmp(p.x, p.y, p.z, 1);
      cv::Matx41f tmp2 = T * tmp;
      points3d[i] = cv::Point3f(tmp2(0), tmp2(1), tmp2(2));
    }
  }

  void save_points(){
    std::ofstream out("data/points_new.txt");
    for (int i = 0; i < points3d.size(); ++i) {
      cv::Point3f p = points3d[i];
      out << p.x << " " << p.y << " " << p.z << std::endl;
    }
  }

  // click multiple position and press any key
  void select_points(uint frameID, cv::Mat & imgColor) {
    static int count_frame = 0;
    if(count_frame++ < frame_after_init) return;
    static bool first = true;
    if (first) {
      first = false;
      cv::imshow("img", imgColor.clone());
      // 设定交互
      cv::setMouseCallback("img", onLBUTTONDOWN, &p2d_seleted);
      // 选中的点
      cv::waitKey(0);
    }
  }

  void get_3dpoints(uint frameID,
                  cv::Mat imgDepthBuffer,
                  cv::Mat &camera_tracking_pose) {
    if (p2d_seleted.size() == 0) {
      return;
    }
    static bool first = true;
    if (first) {
      first = false;
      // 500
      // p2d_seleted.push_back(cv::Point2f(272, 230));
      // p2d_seleted.push_back(cv::Point2f(275, 304));
      // p2d_seleted.push_back(cv::Point2f(355, 305));
      // p2d_seleted.push_back(cv::Point2f(367, 229));
      // assert(p2d_seleted.size() == 4);


      // 逐个点恢复3D位置
      for (int i = 0; i < p2d_seleted.size(); ++i) {
        cv::Point2f p = p2d_seleted[i];
        float d = imgDepthBuffer.at<ushort>(p.y, p.x) / 1000.0;
        // T_wc * K.inv() * p * d
        cv::Point3f tmp = K.inv() * cv::Point3f(p.x, p.y, 1) * d;
      //   std::cout << "tmp: " << tmp << std::endl;
        auto p3d = cv::Mat(camera_tracking_pose * cv::Vec4f(tmp.x, tmp.y, tmp.z, 1));
        std::cout << "p3d: " << p3d << std::endl;
        // get first 3 elements
        points3d.push_back(
            cv::Point3f(p3d.at<float>(0), p3d.at<float>(1), p3d.at<float>(2)));
      }
    }
  }

  /**
   * @brief 
   * 
   * @param camera_tracking_pose T_wc
   * @param imgColor 
   */
  void point_reprojection(cv::Mat &camera_tracking_pose, cv::Mat imgColor) {
    if (points3d.size() == 0) {
      return;
    }
      // 把每个红点的3D位置用红圈表示出来
      cv::Mat imgColorShow = imgColor.clone(); // 用于显示的图像
      for (int i = 0; i < points3d.size(); ++i) {
        cv::Point3f p = points3d[i];
        cv::Vec4f tmp = cv::Mat(cv::Mat(camera_tracking_pose.inv()) *
                                cv::Vec4f(p.x, p.y, p.z, 1)); // 相机坐标系下的3D点
        cv::Point3f p_2d = K * cv::Point3f(tmp[0], tmp[1], tmp[2]) / tmp[2];
        cv::circle(imgColorShow, cv::Point(p_2d.x, p_2d.y), 5,
                  cv::Scalar(0, 0, 255), -1);
      }
      static int frame_id = 0;
      std::cout << "frame_id" << frame_id ++ << std::endl;
      cv::imshow("imgColorShow", imgColorShow);
      cv::waitKey(1);
    }
};
