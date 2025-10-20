#ifndef ARTEST_INTERACTIONCONFIGLOADER_H
#define ARTEST_INTERACTIONCONFIGLOADER_H


#include "json.hpp"
#include "BasicData.h"
#include "communication/dataInterface.h"

#include <fstream>
#include <stdexcept>
#include <cfloat>

class InteractionConfigLoader {
public:
    using Ptr = std::shared_ptr<InteractionConfigLoader>;
    static Ptr create(SceneData& sceneData, const std::string& filePath) {
        return std::make_shared<InteractionConfigLoader>(sceneData, filePath);
    }
    static int RegisterCollisionHandler(const std::string& name, CollisionHandler::Ptr (*createFunc)());

    InteractionConfigLoader(SceneData& sceneData, const std::string& filePath);
    int GetCollisionPairSize();
    CollisionDetectionPair::Ptr GetCollisionPairAt(const int& index, AppData& appData);
    std::vector<CollisionDetectionPair::Ptr> GetCollisionPairs(AppData& appData);
	CollisionData::Ptr GetCollisionData(const std::string& fileName, const std::string& filePath, SceneObjectPtr sceneObjectPtr, const std::string& instance, const std::string& instanceID, CollisionBox collisionBox, cv::Vec3f dir, int originStateIndex);

    std::unordered_map<std::string, CollisionData::Ptr> collisionDatas;
private:
    static inline std::map<std::string, CollisionHandler::Ptr (*)()>* _collisionHandlerMapPtr = new std::map<std::string, CollisionHandler::Ptr(*)()>;
    static inline std::map<std::string, enum CollisionBox>* _collisionBoxMapPtr = new std::map<std::string, enum CollisionBox>;
    static inline std::map<std::string, enum CollisionType>* _collisionTypeMapPtr = new std::map<std::string, enum CollisionType>;
    static inline std::map<std::string, enum Gesture>* _gestureMapPtr = new std::map<std::string, enum Gesture>;
    nlohmann::json _jsonData;
	SceneData& _sceneData;
};


#endif //ARTEST_INTERACTIONCONFIGLOADER_H
