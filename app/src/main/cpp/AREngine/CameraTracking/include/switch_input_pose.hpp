#include <queue>
#include <sstream>
#include <fstream>
#include <iostream>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

class PoseSwicher
{
public:
    // 构造函数：读取 TUM 格式文件（timestamp tx ty tz qx qy qz qw），
    // 逐行解析后构造 4x4 位姿矩阵，压入队列。
    explicit PoseSwicher(const std::string& tum_file_path)
    {
        std::ifstream fin(tum_file_path);
        if (!fin.is_open())
        {
            std::cerr << "cannot open tum file: " << tum_file_path << std::endl;
            return;
        }

        std::string line;
        while (std::getline(fin, line))
        {
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            std::istringstream iss(line);
            double ts, tx, ty, tz, qx, qy, qz, qw;
            // TUM 通用格式：timestamp tx ty tz qx qy qz qw
            if (!(iss >> ts >> tx >> ty >> tz >> qx >> qy >> qz >> qw))
            {
                // 解析失败则跳过
                continue;
            }

            // 四元数 -> 旋转矩阵（Eigen 使用构造顺序 w,x,y,z）
            Eigen::Quaternionf q((float)qw, (float)qx, (float)qy, (float)qz);
            q.normalize();
            Eigen::Matrix3f R = q.toRotationMatrix();

            // 构造 4x4 齐次变换，类型为 CV_32F
            cv::Mat T = cv::Mat::eye(4, 4, CV_32F);
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    T.at<float>(r, c) = R(r, c);
                }
            }
            T.at<float>(0, 3) = (float)tx;
            T.at<float>(1, 3) = (float)ty;
            T.at<float>(2, 3) = (float)tz;

            // 压入队列（按文件顺序，时间戳从小到大时，back() 即“最近/最新”）
            pose_queue.push(std::make_pair(ts, T));
        }

        fin.close();
    }

    // 功能：根据传入的 bool 决定输出内容。
    // 如果 控制变量为true：不修改位姿
    // 否则：用离线文件中最近的
    cv::Mat getPose(bool use_online_pose, const cv::Mat& pose_online, double ts_online)
    {
        if (!use_online_pose)
        {
            // 如果缓存为空且队列也为空，报错
            if (last_pose_return.second.empty() && pose_queue.empty())
            {
                std::cerr << "\033[31mPoseTest: pose_queue is empty, return online pose.\033[0m" << std::endl;
            }

            // 如果队首更近，更新/维护 last_pose_return, 保证last_pose_return距离 ts_online 最近
            for (;;)
            {
                double dt_last_cur = std::abs(last_pose_return.first - ts_online);
                const std::pair<double, cv::Mat>& front_pair = pose_queue.front();
                double dt_front_cur = std::abs(front_pair.first - ts_online);
                std::cout<< "dt_front_cur: " << dt_front_cur << ", dt_last_cur: " << dt_last_cur << std::endl;
                if (dt_front_cur < dt_last_cur)
                {
                    last_pose_return.first = front_pair.first;
                    last_pose_return.second = front_pair.second.clone();
                    pose_queue.pop();
                }
                else
                {
                    break;
                }
            }
            return last_pose_return.second.clone();
        }
        else
        {
            return pose_online.clone();
        }
    }

private:
    // 队列元素：<timestamp, T(4x4)>
    std::queue<std::pair<double, cv::Mat>> pose_queue;
    // 上一次返回的位姿：<timestamp, T(4x4)>
    std::pair<double, cv::Mat> last_pose_return;
};