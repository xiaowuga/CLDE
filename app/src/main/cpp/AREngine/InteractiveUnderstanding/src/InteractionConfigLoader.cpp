#include "InteractionConfigLoader.h"
#include <android/log.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

InteractionConfigLoader::InteractionConfigLoader(SceneData& sceneData, const std::string& filePath):_sceneData(sceneData) {
    std::ifstream file(filePath);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    file >> _jsonData;  // 使用 nlohmann/json 直接解析文件

    file.close();
    
    _collisionBoxMapPtr->insert(std::make_pair("AABB", CollisionBox::AABB));
	_collisionBoxMapPtr->insert(std::make_pair("OBB", CollisionBox::OBB));
	_collisionBoxMapPtr->insert(std::make_pair("Sphere", CollisionBox::Sphere));
	_collisionBoxMapPtr->insert(std::make_pair("Ray", CollisionBox::Ray));

    _collisionTypeMapPtr->insert(std::make_pair("AABB2AABB", CollisionType::AABB2AABB));
    _collisionTypeMapPtr->insert(std::make_pair("Ray2AABB", CollisionType::Ray2AABB));
	_collisionTypeMapPtr->insert(std::make_pair("Sphere2AABB", CollisionType::Sphere2AABB));

	_gestureMapPtr->insert(std::make_pair("ZOOM", Gesture::ZOOM));
	_gestureMapPtr->insert(std::make_pair("GRASP", Gesture::GRASP));
	_gestureMapPtr->insert(std::make_pair("CURSOR", Gesture::CURSOR));
	_gestureMapPtr->insert(std::make_pair("UNDEFINED", Gesture::UNDEFINED));
}

int InteractionConfigLoader::RegisterCollisionHandler(const std::string& name, CollisionHandler::Ptr (*createFunc)()) {
    std::cout << "Registering collision handler: " << name << std::endl;
    try {
        _collisionHandlerMapPtr->insert(std::make_pair(name, createFunc));
    } catch (const std::exception& e) {
        std::cerr << "Error registering collision handler: " << e.what() << std::endl;
        return 0;
    }
    return 1;
}


int InteractionConfigLoader::GetCollisionPairSize() {
    return _jsonData.at("CollisionPairs").size();
}
CollisionDetectionPair::Ptr InteractionConfigLoader::GetCollisionPairAt(const int& index, AppData& appData) {
    //依据配置文件创建碰撞检测对
    auto pairJson = _jsonData.at("CollisionPairs")[index];
	std::string obj1Name = pairJson.at("Obj1").get<std::string>();
	SceneObjectPtr obj1Ptr = _sceneData.getObject(obj1Name);
	if (obj1Ptr == nullptr) {
		throw std::runtime_error("Object not found: " + obj1Name);
	}
	CollisionBox obj1Box = _collisionBoxMapPtr->at(pairJson.at("Obj1Box").get<std::string>());
	cv::Vec3f obj1Dir = cv::Vec3f(pairJson.at("Obj1Dir")[0].get<float>(), pairJson.at("Obj1Dir")[1].get<float>(), pairJson.at("Obj1Dir")[2].get<float>());
	float obj1Radius = pairJson.at("Obj1Radius").get<float>();
    std::string instance1 = pairJson.at("Instance1").get<string_t>();
    std::string instanceId1 = pairJson.at("InstanceId1").get<string_t>();

    auto it = collisionDatas.find(obj1Name + "/" + instance1);
    CollisionData::Ptr obj1DataPtr;
    if (it == collisionDatas.end()) {
        obj1DataPtr = GetCollisionData(obj1Ptr->fileName, obj1Ptr->filePath, obj1Ptr, instance1, instanceId1, obj1Box, obj1Dir, 0);
        collisionDatas.insert({obj1Name + "/" + instance1, obj1DataPtr});
    }
    else {
        obj1DataPtr = it->second;
    }

	std::string obj2Name = pairJson.at("Obj2").get<std::string>();
	SceneObjectPtr obj2Ptr = _sceneData.getObject(obj2Name);
    if (obj2Ptr == nullptr) {
		throw std::runtime_error("Object not found: " + obj2Name);
    }
	CollisionBox obj2Box = _collisionBoxMapPtr->at(pairJson.at("Obj2Box").get<std::string>());
	cv::Vec3f obj2Dir = cv::Vec3f(pairJson.at("Obj2Dir")[0].get<float>(), pairJson.at("Obj2Dir")[1].get<float>(), pairJson.at("Obj2Dir")[2].get<float>());
	float obj2Radius = pairJson.at("Obj2Radius").get<float>();
    std::string instance2 = pairJson.at("Instance2").get<string_t>();
    std::string instanceId2 = pairJson.at("InstanceId2").get<string_t>();
    int originStateIndex = pairJson.at("InitialAnimationState").get<int>();
    auto it2 = collisionDatas.find(obj2Name + "/" + instance2);
    CollisionData::Ptr obj2DataPtr;
    if (it2 == collisionDatas.end()) {
        obj2DataPtr = GetCollisionData(obj2Ptr->fileName, obj2Ptr->filePath, obj2Ptr, instance2, instanceId2, obj2Box, obj2Dir, originStateIndex);
        collisionDatas.insert({obj2Name + "/" + instance2, obj2DataPtr});
    }
    else {
        obj2DataPtr = it2->second;
    }

	CollisionType collisionType = _collisionTypeMapPtr->at(pairJson.at("CollisionType").get<std::string>());
	Gesture lGesture = _gestureMapPtr->at(pairJson.at("LGesture").get<std::string>());
	Gesture rGesture = _gestureMapPtr->at(pairJson.at("RGesture").get<std::string>());
	CollisionHandler::Ptr handler = _collisionHandlerMapPtr->at(pairJson.at("Handler").get<std::string>())();
    __android_log_print(ANDROID_LOG_DEBUG, "Collision", "collision pair created: <%s, %s>->%s", obj1DataPtr->modelName.c_str(), obj2DataPtr->modelName.c_str(), handler->class_name.c_str());
    return CollisionDetectionPair::create(obj1DataPtr, obj2DataPtr, collisionType, handler, appData, lGesture, rGesture);;
}
CollisionData::Ptr InteractionConfigLoader::GetCollisionData(const std::string& fileName, const std::string& filePath, SceneObjectPtr sceneObjectPtr, const std::string& instance, const std::string& instanceID, CollisionBox collisionBox, cv::Vec3f dir, int originStateIndex) {
    // 判断文件类型是不是fb
	std::string format = fileName.substr(fileName.find_last_of(".") + 1);
    if (format == "fb") {
        //转换本地flatBuffer模型
//        cadDataManager::DataInterface::parseLocalModel(fileName, filePath);
        cadDataManager::DataInterface::setActiveDocumentData(fileName.substr(0, fileName.find_last_of('.')));
        std::string instanceName = instance;
        std::string instanceId = instanceID;
        //取得实例信息
        auto instanceInfoMap = cadDataManager::DataInterface::getInstanceInfos();
        for(auto instanceInfoPair : instanceInfoMap) {
            if(instanceId == "" || instanceInfoPair.first.substr(instanceInfoPair.first.find_last_of("_") + 1) == instanceId) {
                auto instanceInfoPtr = instanceInfoPair.second;
                //创建碰撞盒
                cv::Vec3f min = cv::Vec3f(instanceInfoPtr->mInstanceAABBBox[0], instanceInfoPtr->mInstanceAABBBox[1], instanceInfoPtr->mInstanceAABBBox[2]) * 0.001f;
                cv::Vec3f max = cv::Vec3f(instanceInfoPtr->mInstanceAABBBox[3], instanceInfoPtr->mInstanceAABBBox[4], instanceInfoPtr->mInstanceAABBBox[5]) * 0.001f;
                if(instanceId == ""){
                    min *= 0.5f;
                    max *= 0.5f;
                }
                cv::Vec3f center = (min + max) / 2.0f;
                CollisionData::Ptr collisionDataPtr = CollisionData::create();
                collisionDataPtr->modelName = sceneObjectPtr->name;
                collisionDataPtr->instanceName = instanceName;
                collisionDataPtr->instanceId = instanceInfoPtr->mInstanceId;
                collisionDataPtr->center = center;
                collisionDataPtr->min = min;
                collisionDataPtr->max = max;
                collisionDataPtr->extents = max - min;
                collisionDataPtr->obj = sceneObjectPtr;
                //依据配置文件设置碰撞盒类型
                collisionDataPtr->boxType = collisionBox;
                //依据配置文件设置射线初始方向
                collisionDataPtr->direction = dir;
                collisionDataPtr->originStateIndex = originStateIndex;
                return collisionDataPtr;
            }
        }
        throw std::runtime_error("Instance not found: " + instanceName);
    }
    else {
		throw std::runtime_error("Unsupported file format: " + filePath + "/" + fileName);
    }
}
std::vector<CollisionDetectionPair::Ptr> InteractionConfigLoader::GetCollisionPairs(AppData& appData) {
    //cadDataManager::DataInterface::init();
    std::vector<CollisionDetectionPair::Ptr> collisionPairs;
    for (int i = 0; i < GetCollisionPairSize(); ++i) {
        collisionPairs.push_back(GetCollisionPairAt(i, appData));
    }
    return collisionPairs;
}

