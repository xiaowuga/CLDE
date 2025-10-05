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
	int fileSplit = obj1Ptr->filePath.find_last_of("/");
	std::string obj1FileName = obj1Ptr->filePath.substr(fileSplit + 1);
	std::string obj1Path = obj1Ptr->filePath.substr(0, fileSplit);

    auto it = collisionDatas.find(obj1Name);
    CollisionData::Ptr obj1DataPtr;
    if (it == collisionDatas.end()) {
        obj1DataPtr = GetCollisionData(obj1FileName, obj1Path, obj1Ptr, obj1Box, obj1Dir);
        collisionDatas.insert({obj1Name, obj1DataPtr});
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
	int fileSplit2 = obj2Ptr->filePath.find_last_of("/");
	std::string obj2FileName = obj2Ptr->filePath.substr(fileSplit2 + 1);
	std::string obj2Path = obj2Ptr->filePath.substr(0, fileSplit2);

    auto it2 = collisionDatas.find(obj2Name);
    CollisionData::Ptr obj2DataPtr;
    if (it2 == collisionDatas.end()) {
        obj2DataPtr = GetCollisionData(obj2FileName, obj2Path, obj2Ptr, obj2Box, obj2Dir);
        collisionDatas.insert({obj2Name, obj2DataPtr});
    }
    else {
        obj2DataPtr = it2->second;
    }

	CollisionType collisionType = _collisionTypeMapPtr->at(pairJson.at("CollisionType").get<std::string>());
	Gesture lGesture = _gestureMapPtr->at(pairJson.at("LGesture").get<std::string>());
	Gesture rGesture = _gestureMapPtr->at(pairJson.at("RGesture").get<std::string>());
	CollisionHandler::Ptr handler = _collisionHandlerMapPtr->at(pairJson.at("Handler").get<std::string>())();
    // std::cout << fmt::format("collision: <{}, {}>->{}", obj1DataPtr->name, obj2DataPtr->name, handler->class_name) << std::endl;
    __android_log_print(ANDROID_LOG_DEBUG, "Collision", "collision pair created: <%s, %s>->%s", obj1DataPtr->name.c_str(), obj2DataPtr->name.c_str(), handler->class_name.c_str());
    return CollisionDetectionPair::create(obj1DataPtr, obj2DataPtr, collisionType, handler, appData, lGesture, rGesture);;
}
CollisionData::Ptr InteractionConfigLoader::GetCollisionData(const std::string& fileName, const std::string& filePath, SceneObjectPtr sceneObjectPtr, CollisionBox collisionBox, cv::Vec3f dir) {
    //取得初始变换矩阵信息
    cv::Matx44f initTransform = sceneObjectPtr->initTransform.GetMatrix();
    //取得变换矩阵信息
    cv::Matx44f transform = sceneObjectPtr->transform.GetMatrix();
    // 判断文件类型是obj还是fb
	std::string format = fileName.substr(fileName.find_last_of(".") + 1);
    if (format == "obj") {
		//转换obj文件
		tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> objmaterials;
        std::string warn;
        std::string err;
		bool success = tinyobj::LoadObj(&attrib, &shapes, &objmaterials, &warn, &err, (filePath + "/" + fileName).c_str(), "", true);
        if (!success) {
			throw std::runtime_error("Failed to load obj file: " + filePath + "/" + fileName);
        }
		cv::Vec3f min = cv::Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
		cv::Vec3f max = cv::Vec3f(FLT_MIN, FLT_MIN, FLT_MIN);
		cv::Vec3f center;
		for (auto shape : shapes) {
			for (auto index : shape.mesh.indices) {
				cv::Vec4f vertex = cv::Vec4f(attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1], attrib.vertices[3 * index.vertex_index + 2], 1);
                cv::Vec4f transVertex = transform * initTransform * vertex;
				min[0] = min[0] < transVertex[0] ? min[0] : transVertex[0];
				min[1] = min[1] < transVertex[1] ? min[1] : transVertex[1];
				min[2] = min[2] < transVertex[2] ? min[2] : transVertex[2];
				max[0] = max[0] > transVertex[0] ? max[0] : transVertex[0];
				max[1] = max[1] > transVertex[1] ? max[1] : transVertex[1];
				max[2] = max[2] > transVertex[2] ? max[2] : transVertex[2];
			}
		}
		center = (min + max) / 2.0f;
		CollisionData::Ptr collisionDataPtr = CollisionData::create();
		collisionDataPtr->name = sceneObjectPtr->name;
		collisionDataPtr->center = center;
		collisionDataPtr->min = min;
		collisionDataPtr->max = max;
		collisionDataPtr->obj = sceneObjectPtr;
		collisionDataPtr->boxType = collisionBox;
		collisionDataPtr->direction = dir;
		return collisionDataPtr;
	}
    else if (format == "fb") {
        // TODO: 转换云端flatBuffer文件
        //cadDataManager::DataInterface::convertModelByFile("127.0.0.1", 9000, fbFileName, fbFilePath, cadDataManager::ConversionPrecision::low);
        //转换本地flatBuffer模型
        cadDataManager::DataInterface::parseLocalModel(fileName, filePath);
        //取得实例信息
        auto instanceInfoPtr = cadDataManager::DataInterface::getInstanceInfos().begin()->second;
        
        //取得元素信息
        auto elementInfoMap = instanceInfoPtr->mElementInfoMap;
        //计算AABB包围盒
        cv::Vec3f min = cv::Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
        cv::Vec3f max = cv::Vec3f(FLT_MIN, FLT_MIN, FLT_MIN);
        cv::Vec3f center;
        for (auto geometryPtr : instanceInfoPtr->mGeometryInfos) {
            //对顶点进行变换
            for (auto geometry : geometryPtr->geometries) {
                std::vector<float> vertices = geometry->getPosition();
                for (int i = 0; i < vertices.size(); i += 3) {
                    cv::Vec4f vertex = cv::Vec4f(vertices[i], vertices[i + 1], vertices[i + 2], 1);
                    cv::Vec4f transVertex = transform * initTransform * vertex;
                    min[0] = min[0] < transVertex[0] ? min[0] : transVertex[0];
                    min[1] = min[1] < transVertex[1] ? min[1] : transVertex[1];
                    min[2] = min[2] < transVertex[2] ? min[2] : transVertex[2];
                    max[0] = max[0] > transVertex[0] ? max[0] : transVertex[0];
                    max[1] = max[1] > transVertex[1] ? max[1] : transVertex[1];
                    max[2] = max[2] > transVertex[2] ? max[2] : transVertex[2];
                }
            }
        }
        center = (min + max) / 2.0f;
        //创建碰撞数据
        CollisionData::Ptr collisionDataPtr = CollisionData::create();
        collisionDataPtr->name = sceneObjectPtr->name;
        collisionDataPtr->center = center;
        collisionDataPtr->min = min;
        collisionDataPtr->max = max;
        collisionDataPtr->obj = sceneObjectPtr;
        //依据配置文件设置碰撞盒类型
        collisionDataPtr->boxType = collisionBox;
        //依据配置文件设置射线初始方向
        collisionDataPtr->direction = dir;
        return collisionDataPtr;
    }
    else {
		throw std::runtime_error("Unsupported file format: " + filePath + "/" + fileName);
    }
}
std::vector<CollisionDetectionPair::Ptr> InteractionConfigLoader::GetCollisionPairs(AppData& appData) {
    cadDataManager::DataInterface::init();
    std::vector<CollisionDetectionPair::Ptr> collisionPairs;
    for (int i = 0; i < GetCollisionPairSize(); ++i) {
        collisionPairs.push_back(GetCollisionPairAt(i, appData));
    }
    return collisionPairs;
}

