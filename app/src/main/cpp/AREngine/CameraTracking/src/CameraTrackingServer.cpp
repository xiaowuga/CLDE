
#include"CameraTrackingServer.h"
#include "debug_utils.hpp"
#include <mutex>
#include <unistd.h>
#include <sys/stat.h>// file size
using namespace cv;

std::mutex mtx;

// CameraTrackingServer::CameraTrackingServer()
// {
	
// }

int CameraTrackingServer::init(RPCServerConnection& con)
{
	return STATE_OK;
}

//ִ�й��̵���
int CameraTrackingServer::call(RemoteProcPtr proc, FrameDataPtr frameDataPtr, RPCServerConnection& con)
//������Ӧ���ӵ���Ϣ
{
	auto& send = proc->send;
	auto cmd = send.getd<std::string>("cmd");
	std::cout << "cmd: " << cmd << std::endl;
	// std::cout << "frameID: " << frameDataPtr->frameID << std::endl;

	// debug
	static std::vector<double> vSlamTimestamps;
	static std::vector<cv::Mat> vSlamPoses;

	if(cmd == "init"){
		auto& appData = con.app->appData;
		appData->isLoadMap = send.getd<bool>("isLoadMap", 0);
		appData->isSaveMap = send.getd<bool>("isSaveMap", 0);
		sensorType = send.getd<int>("sensorType", 0);
		std::cout << "appData:isLoadMap" << appData->isLoadMap << std::endl;
		std::cout << "appData:isSaveMap" << appData->isSaveMap << std::endl;
		std::cout << "sensorType: " << sensorType << std::endl;
		// auto& sceneData = _app->sceneData; û���õ�

		std::cout << "init start" << std::endl;
		sys = std::make_shared<Reloc::Relocalization>();

		
		std::string engineDir;
		try{
			engineDir=appData->engineDir;
		}catch(const std::bad_any_cast& e){
			std::cout << e.what() << std::endl;
		}
		std::string trackingConfigPath = engineDir + "CameraTracking/config.json";

		ConfigLoader crdr(trackingConfigPath);
		std::string configPath = crdr.getValue<std::string>("configPath");
		std::string configName = appData->engineDir + configPath + crdr.getValue<std::string>("reloc_configName");
		std::string vocaFile = appData->engineDir + configPath + crdr.getValue<std::string>("vocaFile");
		std::string mapFile = appData->dataDir + crdr.getValue<std::string>("mapFile");

		if (access(appData->dataDir.c_str(), 0) == -1)	
		{	
			system(("mkdir -p " + appData->dataDir).c_str());
			std::cout << "dataDir does not exist, create : " << appData->dataDir << std::endl;
		}
		// check if mapfile is empty
		if (access(mapFile.c_str(), 0) == 0)
		{
			struct stat statbuf;
			stat(mapFile.c_str(), &statbuf);
			std::cout << "mapFile size: " << statbuf.st_size << std::endl;
			if (statbuf.st_size == 0)
			{	
				// red color cerr
				std::cerr << "\033[31m" << "mapFile is empty, please check the mapFile path or delete it: " << mapFile << "\033[0m" << std::endl;
				return STATE_ERROR;
			}
		}

		bool status = sys->Init(configName, vocaFile, mapFile,
								appData->isLoadMap, appData->isSaveMap,
								static_cast<unsigned int>(sensorType));
		try {
			if (!status) {
				throw std::runtime_error("Reloc Initialization failed.");
			}
		}catch (const std::runtime_error& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return STATE_ERROR;
		
		}
		initialized = true;
		std::cout << "CameraTrackingServer init successfully" << std::endl;
	}
	else if (cmd == "reloc")
	{
		mtx.lock();
		// std::cout << "frameID:" << frameDataPtr->frameID << std::endl;
		auto rgb = frameDataPtr->image.front().clone();

		// bool hasDepth = !frameDataPtr->depth.empty();
		// cv::Mat depth;
		// if (hasDepth) {
		// 	depth = frameDataPtr->depth.front().clone();
		// }
		auto slam_pose = send.getd<cv::Mat>("slam_pose");
		std::cout << "slam_pose: " << slam_pose << std::endl;
		auto tframe = send.getd<double>("tframe");

		CV_Assert(rgb.size()==Size(640,480)); //��֤����ͼ���С������ֵ

		cv::Mat slam_pose_32f;
		slam_pose.convertTo(slam_pose_32f, CV_32F);
		if (slam_pose.empty())
		{
			std::cerr << "\033[31m" << "slam_pose is empty, please check the slam_pose." << "\033[0m" << std::endl;
			mtx.unlock();
			return STATE_ERROR;
		}

		// 根据初始化时的 sensorTypeStatic 来决定调用哪种处理函数
		if (sensorType == 2) {
			// RGBD 模式
			cv::Mat depth = frameDataPtr->depth.front().clone();
			sys->processRGBD_Pose(tframe, rgb, depth, slam_pose_32f);
		} else {
			// MONO/RGB 模式
			sys->processRGB_Pose(tframe, rgb, slam_pose_32f);
		}

		cv::Mat alignTransform;
		bool align_OK = sys->alignSLAM2Reloc(alignTransform);
		std::cout << "align_OK: " << align_OK << std::endl;
		std::cout << "alignTransform: " << alignTransform << std::endl;
		if (sys->getPoses().size() == 0 || !align_OK)		
		{
			std::cout << "\033[33m" << "alignSLAM2Reloc failed, wait for more poses..." << "\033[0m" << std::endl;
			proc->ret = {
				{"align_OK",Bytes(align_OK)},
				{"alignTransform",Bytes(alignTransform)},
				{"curFrameID",Bytes(frameDataPtr->frameID)},
				{"RelocPose",Bytes(cv::Mat())} // 随便填充，反正 RelocPose 不会被使用
			};
		}
		else{
			proc->ret = {
				{"align_OK",Bytes(align_OK)},
				{"alignTransform",Bytes(alignTransform)},
				{"curFrameID",Bytes(frameDataPtr->frameID)},
				{"RelocPose",Bytes(sys->getPoses().back().second)} // TODO(lsw)： 这里没必要每次都返回所有的。或者使用指针
			};
		}

		// debug
		vSlamTimestamps.push_back(tframe);
		vSlamPoses.push_back(slam_pose.clone());
		// check frameID
		static int last_frameID = -1;
		if (frameDataPtr->frameID < last_frameID)
		{
			// std::cout << "\033[31m" << "frameDataPtr->frameID < last_frameID" << "\033[0m" << std::endl;
			// std::cout << "frameDataPtr->frameID: " << frameDataPtr->frameID << std::endl;
			// std::cout << "last_frameID: " << last_frameID << std::endl;
		}
		last_frameID = frameDataPtr->frameID;
		// check tframe
		static double last_tframe = 0;
		if (tframe < last_tframe)
		{
			std::cerr << "tframe < last_tframe" << std::endl;
			mtx.unlock();
			return STATE_ERROR;
		}
		last_tframe = tframe;

		mtx.unlock();
	}
	else if (cmd == "shutdown")
	{
		std::cout << "into shutdown..." << std::endl;
		// mtx.lock();

		std::vector<std::pair<double, cv::Mat>> vRelocPoses = sys->getPoses();
		// shutdown
		sys->Shutdown();
		std::cout << "CameraTrackingServer shutdown successfully" << std::endl;

		// debug: 构造带后缀的文件名
		auto& appData = con.app->appData;
		bool isSave = appData->isSaveMap;
		bool isLoad = appData->isLoadMap;

		// 计算各自的位姿个数：SLAM 位姿和 Reloc 位姿
		size_t slamCount = vSlamTimestamps.size();
		size_t relocCount = vRelocPoses.size();

		std::string suffix = "_isSave" + std::to_string(isSave)
						   + "_isLoad" + std::to_string(isLoad)
						   + "_slamCount" + std::to_string(slamCount)
						   + "_relocCount" + std::to_string(relocCount);

		std::string SLAMPoseFile  = debug_output_path + "/ServerSLAMPose" + suffix + ".txt";
		std::string RelocPoseFile = debug_output_path + "/ServerRelocPose" + suffix + ".txt";
		static bool first_time = true;
		if (first_time)
		{
			first_time = false;
			if (access(SLAMPoseFile.c_str(), 0) == 0)
			{
				system(("rm " + SLAMPoseFile).c_str());
				std::cout << "remove " << SLAMPoseFile << std::endl;
			}
			if (access(RelocPoseFile.c_str(), 0) == 0)
			{
				system(("rm " + RelocPoseFile).c_str());
				std::cout << "remove " << RelocPoseFile << std::endl;
			}
		}
		// save SLAM poses
		if (vSlamTimestamps.size() != vSlamPoses.size())
		{
			std::cerr << "vSlamTimestamps.size() != vSlamPoses.size()" << std::endl;
			return STATE_ERROR;
		}
		for (int i = 0; i < vSlamTimestamps.size(); i++)
		{
			double timestamp = vSlamTimestamps[i];
			cv::Mat pose = vSlamPoses[i];
			// std::cout << "timestamp: " << timestamp << std::endl;
			// std::cout << "pose: " << pose << std::endl;
			save_pose_as_tum(SLAMPoseFile, timestamp, pose);
		}
		std::cout << "save SLAM poses to " << SLAMPoseFile << std::endl;
		for (int i = 0; i < vRelocPoses.size(); i++)
		{
			double timestamp = vRelocPoses[i].first;
			cv::Mat pose = vRelocPoses[i].second;
			save_pose_as_tum(RelocPoseFile, timestamp, pose);
		}
		std::cout << "save Reloc poses to " << RelocPoseFile << std::endl;
		// mtx.unlock();
	}
	return STATE_OK;
}

