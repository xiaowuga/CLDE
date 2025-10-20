#include "CollisionDetection.h"



CollisionDetection::CollisionDetection(){
    _moduleName = "CollisionDetection";
}
CollisionDetection::~CollisionDetection(){

}
void CollisionDetection::PreCompute(std::string configPath){

}
int CollisionDetection::Init(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr){
    // 加载交互配置文件
	InteractionConfigLoader::Ptr interactionConfigPtr = InteractionConfigLoader::create(sceneData, appData.dataDir +  appData.interactionConfigFile);
    // 添加碰撞检测对到frameData的collisionPairs中
    std::vector<CollisionDetectionPair::Ptr> collisionPairs =  interactionConfigPtr->GetCollisionPairs(appData);
    sceneData.collisionPairs = collisionPairs;
    _collisionDatas = interactionConfigPtr->collisionDatas;
    return STATE_OK;
}
int CollisionDetection::Update(AppData &appData, SceneData& sceneData, FrameDataPtr frameData){
    //更新碰撞箱
    for (auto collisionDataPair: _collisionDatas){
        collisionDataPair.second->SetBox();
    }
    //遍历碰撞对
    for (std::shared_ptr<CollisionDetectionPair> pairPtr: sceneData.collisionPairs){
        pairPtr->SetFrameData(frameData);
        if(_isColliding(pairPtr->GetObj1(), pairPtr->GetObj2(), pairPtr->GetType())){
            pairPtr->SetColliding(true, sceneData);
        } else{
            pairPtr->SetColliding(false, sceneData);
        }
		//TODO: Only for test
        //pairPtr->SetColliding(true);
    }
    return STATE_OK;
}
int CollisionDetection::ShutDown(AppData& appData,  SceneData& sceneData){

    return STATE_OK;
}
int CollisionDetection::CollectRemoteProcs(SerilizedFrame& serilizedFrame, std::vector<RemoteProcPtr>& procs, FrameDataPtr frameDataPtr){

    return STATE_OK;
}
int CollisionDetection::ProRemoteReturn(RemoteProcPtr proc){

    return STATE_OK;
}
void CollisionDetection::AddCollisionHandler(CollisionHandler* handler){

}
void CollisionDetection::RemoveCollisionHandler(CollisionHandler* handler){

}
bool CollisionDetection::_isColliding(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, CollisionType type){
    switch (type) {
        case AABB2AABB:
            return _AABB2AABB(obj1, obj2);
        case Ray2AABB:
            return _Ray2AABB(obj1, obj2);
        case Sphere2AABB:
            return _Sphere2AABB(obj1, obj2);
    }
}
bool CollisionDetection::_AABB2AABB(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2){
    //包围盒1的最小值比包围盒2的最大值还大，或包围盒1的最大值比包围盒2的最小值还小，则不发生碰撞
    if (obj1->max[0] < obj2->min[0] || obj1->min[0] > obj2->max[0] || obj1->max[1] < obj2->min[1] || obj1->min[1] > obj2->max[1] || obj1->max[2] < obj2->min[2] || obj1->min[2] > obj2->max[2])
        return false;
    return true;
}
bool CollisionDetection::_Ray2AABB(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2){
    //判断射线是否不在AABB包围盒内
    bool notInside = obj1->center[0] < obj2->min[0] || obj1->center[0] > obj2->max[0] || obj1->center[1] < obj2->min[1] || obj1->center[1] > obj2->max[1] || obj1->center[2] < obj2->min[2] || obj1->center[2] > obj2->max[2];
    //判断射线方向是否与射线中心到AABB包围盒中心的连线方向相反
    bool backward = obj1->direction.dot(obj2->center - obj1->center) < 0;
    if (notInside && backward)
        return false;
    //判断射线是否与AABB包围盒相交
    cv::Vec3f min = obj2->min - obj1->center;
    cv::Vec3f max = obj2->max - obj1->center;
    cv::Vec3f proj = cv::Vec3f(1.0f / obj1->direction[0], 1.0f / obj1->direction[1], 1.0f / obj1->direction[2]);
    cv::Vec3f pMin = proj.mul(min);
    cv::Vec3f pMax = proj.mul(max);
    if (obj1->direction[0] < 0){
        std::swap(pMin[0], pMax[0]);
    }
    if (obj1->direction[1] < 0){
        std::swap(pMin[1], pMax[1]);
    }
    if (obj1->direction[2] < 0){
        std::swap(pMin[2], pMax[2]);
    }
    float n = pMin[0] > pMin[1] ? pMin[0] : pMin[1];
    n = n > pMin[2] ? n : pMin[2];
    float f = pMax[0] < pMax[1] ? pMax[0] : pMax[1];
    f = f < pMax[2] ? f : pMax[2];
    if(!notInside){
        cv::Vec3f hitPoint = obj1->center + obj1->direction * f;
        return true;
    }else{
        if (n < f && obj1->radius >= n)
            return true;
        else{
            return false;
        }
        cv::Vec3f hitPoint = obj1->center + obj1->direction * n;
    }
    return false;
}
bool CollisionDetection::_Sphere2AABB(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2){
    //求出最近点
    cv::Vec3f center = obj1->center;
    cv::Vec3f closestP = center;
    //在obj2的x轴上截断center
    if(center[0] < obj2->min[0]){
        closestP[0] = obj2->min[0];
    }else if(center[0] > obj2->max[0]){
        closestP[0] = obj2->max[0];
    }
    //在obj2的y轴上截断center
    if(center[1] < obj2->min[1]){
        closestP[1] = obj2->min[1];
    }else if(center[1] > obj2->max[1]){
        closestP[1] = obj2->max[1];
    }
    //在obj2的z轴上截断center
    if(center[2] < obj2->min[2]){
        closestP[2] = obj2->min[2];
    }else if(center[2] > obj2->max[2]){
        closestP[2] = obj2->max[2];
    }
    //求出最近点到center的距离
    float distance = cv::norm(center - closestP);
    //判断是否发生碰撞
    if(distance <= obj1->radius){
        return true;
    }
    return false;
}

std::vector<float> CollisionDetection::GetBoundingBoxArray(){
    std::vector<float> bboxArray;
    for (auto collisionDataPair: _collisionDatas){
        CollisionData::Ptr collisionDataPtr = collisionDataPair.second;
        bboxArray.push_back(collisionDataPtr->min[0]);
        bboxArray.push_back(collisionDataPtr->min[1]);
        bboxArray.push_back(collisionDataPtr->min[2]);
        bboxArray.push_back(collisionDataPtr->max[0]);
        bboxArray.push_back(collisionDataPtr->max[1]);
        bboxArray.push_back(collisionDataPtr->max[2]);
    }
    return bboxArray;
}

