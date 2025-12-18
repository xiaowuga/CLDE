#ifndef ARENGINE_BASICDATA_H
#define ARENGINE_BASICDATA_H

#include <unordered_map>
#include <any>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include "PoseUtils.h"
#include "glm/detail/type_mat.hpp"
#include "glm/glm.hpp"
//#include "log.h"


/**
 * HandPose
 * use by HandPoseEstimation
 */
enum  class hand_tag:int{Null, Left , Right};
class HandPose {
public:
    static const std::size_t jointNum = 21;
public:
    hand_tag tag;
    std::array<cv::Vec3f, jointNum> joints;
    // std::vector<std::vector<std::vector<float>>> rotation; // 16 个 3x3 旋转矩阵
    std::array<std::array<float, 2>, 21> joints2d;
    std::array<std::array<std::array<float, 3>, 3>, 16> rotation;
    std::array<float, 10> shape;  // 10个形状参数
    cv::Vec3f landmarkOrigin;

public:
    HandPose();
    HandPose(int tag_value, std::array<cv::Vec3f, 21> joint_positions,
             std::array<std::array<float, 2>, 21> joints2d,
             std::array<std::array<std::array<float, 3>, 3>, 16> rotation_matrices,
             std::array<float, 10> shape_params, cv::Vec3f origin);
    HandPose(int tag_value, std::vector<cv::Point3f> joint_positions,
             std::array<std::array<float, 2>, 21> joints2d,
             std::array<std::array<std::array<float, 3>, 3>, 16> rotation_matrices,
             std::array<float, 10> shape_params, cv::Vec3f origin);
    HandPose(const HandPose& other);
    HandPose& operator=(const HandPose& other);
    std::array<cv::Vec3f, 21>& getjoints();
    std::array<std::array<float, 2>, 21>& getjoints2d();
    std::array<std::array<std::array<float, 3>, 3>, 16>& getrotation();
    std::array<float, 10>& getshape();
    cv::Vec3f getlandmarkOrigin();
    hand_tag gettag();
    void setPose(int,std::array<cv::Vec3f, jointNum>,  cv::Vec3f landmarkOrigin);
    void showResult();
};

class BodyPose {
public:
    static const std::size_t jointNum = 33;
private:
    std::array<cv::Vec3f, jointNum> joints;
    // cv::Vec3f landmarkOrigin;

public:
    BodyPose();
    BodyPose(std::array<cv::Vec3f, jointNum>);
    std::array<cv::Vec3f, jointNum> getjoints();
    // cv::Vec3f getlandmarkOrigin();
    void setPose(std::array<cv::Vec3f, jointNum>);
};

class FacePose {
public:
    static const std::size_t jointNum = 478;
private:
    std::array<cv::Vec3f, jointNum> joints;
    // cv::Vec3f landmarkOrigin;
public:
    FacePose();
    FacePose(std::array<cv::Vec3f, jointNum>);
    std::array<cv::Vec3f, jointNum> getjoints();
    // cv::Vec3f getlandmarkOrigin();
    void setPose(std::array<cv::Vec3f, jointNum>);
};





/**
 * Pose
 * use by ObjectTracking
 */
class Pose {
private:
    cv::Matx44f matrix;

public:

    Pose()
        :matrix(cv::Matx44f::eye())
    {};

    Pose(const cv::Matx44f &mat)
    :matrix(mat)
    {};

    ~Pose(){};

    void setPose(cv::Matx44f pose);

    cv::Matx44f GetMatrix() const;
    cv::Matx44f GetPosition() const;
    cv::Matx44f GetRotation() const;
};

/**
 * ObjectPose
 * 三维跟踪使用的模型位姿
 */

//class ObjectPose
//{
//public:
//    ObjectPose(){};
//    ~ObjectPose(){};
//    Pose pose;
//    std::string modelName;
//    std::string modelPath;
//};


//class CameraPose
//{
//public:
//    Pose pose;
//    double timestamp;  // unit: s
//};

/**
 * BasicData
 * 数据类的基类
 * 使用map来存入string和任意类型变量的键值对
 */

class BasicData
{
public:
    virtual bool setData(const std::string& key, const std::any& value);
    virtual std::any getData(const std::string& key);
    virtual bool hasData(const std::string& key) const;
    virtual bool removeData(const std::string& key);
    virtual void clearData();

    virtual ~BasicData();
private:
    bool _hasData(const std::string& key) const
    {
        return this->data.find(key) != this->data.end();
    }
    std::unordered_map<std::string, std::any> data;
    mutable std::shared_mutex mutex_;
};

class SceneObject
    :public BasicData
{
public:
    SceneObject() = default;
    SceneObject(std::string name, Pose transform) :
            name(name), transform(transform) {
        initTransform.setPose(cv::Matx44f::eye());
    }
    SceneObject(const std::string& name, const std::string& filepath, const Pose& initTransform, const Pose& transform)
            : name(name), filePath(filepath), initTransform(initTransform), transform(transform) {

    }


    std::string  name;//"xxxx"
    std::string  fileName;//"xxxx.fb"
    std::string  filePath;//"path/to"

    Pose         initTransform; //模型初始变换(重定位后)

    Pose         transform; //从初始变换到当前状态的变换

    cv::Matx44f ARSelfPose; // AR眼镜内部获取的位姿。从相机坐标到世界坐标的齐次变换
    // virtual void Init() = 0;
    virtual void Init(){
        // default imply
    }
};

typedef std::shared_ptr<SceneObject>  SceneObjectPtr;

class Camera
    :public SceneObject
{
public:
    Camera() {};
    void Init() override;
    double timestamp;  // unit: s
    int width;
    int height;
    float fps;
    cv::Matx33f IntrinsicMatrix;
    cv::Matx14f distCoeffs;
};

class SceneModel
    :public SceneObject
{
public:
};

class RealObject
    :public SceneObject
{
public:
};

class VirtualObject
    :public SceneObject
{
public:
};

class ShadowRecevier
        :public SceneObject
{
public:
};

class Anchor
    :public SceneObject
{
public:

};

class Plane
    :public SceneObject
{
public:
};

class actionPassage{
public:
    std::string modelName;
    std::string instanceName;
    std::string originState;
    std::string targetState;
    std::string instanceId;

    bool isEmpty(){
        if( this->modelName.empty() && this->instanceName.empty() && this->originState.empty() && this->targetState.empty() && this->instanceId.empty()){
            return true;
        } else return false;
    }

    void clear(){
        this->modelName.clear();
        this->instanceName.clear();
        this->originState.clear();
        this->targetState.clear();
        this->instanceId.clear();
    }

};

class SerilizedFrame;

class CollisionDetectionPair;

class GestureUnderstandingData;

class SceneObjectFactory {
public:
    using CreateFunc = std::function<std::unique_ptr<SceneObject>()>;

    static SceneObjectFactory& instance() {
        static SceneObjectFactory instance;
        return instance;
    }

    void registerClass(const std::string& type, CreateFunc func) {
        registry_[type] = func;
    }

    SceneObjectPtr create(const std::string& type) {
        auto it = registry_.find(type);
        if (it != registry_.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    SceneObjectFactory() = default;
    std::unordered_map<std::string, CreateFunc> registry_;
    // 注册所有 SceneObject 类
};

/**
 * FrameData
 * 存放每一帧刷新的数据，主要是传感器获取的数据
 * 一些需要在每帧使用的临时变量也可存放
 */
class FrameData : public BasicData
{
public:

    FrameData() {}

    ~FrameData() {}

    /**
     * RGB image
     * use by ObjectTracking
     */
    std::vector<cv::Mat> image;
    cv::Matx33f  colorCameraMatrix;
    cv::Mat      colorDistCoeffs;

    //Depth image
    std::vector<cv::Mat> depth;

    //todo add your variable here
    //各算法单位需根据自己的需求在此新增变量或参数，如新增变量较复杂，可新建一个头文件
    std::vector<double> projectVector;
    std::vector<double> viewVector;
    // Cockpit -> Glass, view:CG, joint:GC
    glm::mat4 transformCG;
    glm::mat4 transformGC;
    glm::mat4 relocMatrix;
    glm::mat4 viewRelocMatrix;
    glm::mat4 jointRelocMatrix;
    glm::mat4 modelRelocMatrix;
    cv::Mat imgColor;
    cv::Mat imgDepth;
    double timestamp;  // unit: s
    uint    frameID = 0;

    std::shared_ptr<GestureUnderstandingData> gestureDataPtr = std::make_shared<GestureUnderstandingData>();
    void convertRGB2BGR(cv::Mat& BGR){
        cv::cvtColor(imgColor, BGR, cv::COLOR_RGB2BGR);
    }


    std::vector<Camera> cameras;
    std::mutex handPoses_mtx; // add: 20251005-kylee
    std::vector<HandPose> handPoses;
    std::vector<BodyPose> bodyPoses;
    std::vector<FacePose> facePoses;
    std::shared_ptr<SerilizedFrame>  serilizedFramePtr;


    cv::Vec3f tip_velocity;
    std::string tip_movement = ""; // valid value: up, down
    
    //指定的帧数据是否已经上传到服务器, key可以是"RGB0","RGB1",...,"Depth0","Depth1",...
    bool hasUploaded(const std::string& key) const;
};


/**
 * SceneData
 * 存放场景管理的数据，主要是算法输出的数据
 * 一些临时变量也可存放
 */

class SceneData : public BasicData
{
public:

    SceneData() {}

    ~SceneData() {}

    /**
     * object pose in camera coordinate
     * output by objectTracking
     */

    //std::vector<ObjectPose> objectPose;

    //todo add your variable here
    //各算法单位需根据自己的需求在此新增变量或参数，如新增变量较复杂，可新建一个头文件
    //std::vector<CameraPose> cameraPose;  // zyd

    // add: 250928-kylee
    std::chrono::steady_clock::time_point last_time;
    cv::Vec3f last_tip_pos;
    bool is_start = true;
    // end

    std::vector<std::shared_ptr<CollisionDetectionPair>> collisionPairs;

    std::vector<FrameData> frameBuffer;
    std::vector<std::vector<double>> imuBuffer;
    std::vector<std::vector<double>> imuTemp;
    std::mutex imu_mutex;

    std::mutex actionLock;
    actionPassage actionPassage{};


    std::vector<SceneObjectPtr> sceneObjects;  // store all objects
private:
    std::vector<SceneObjectPtr>::iterator _findObject(const std::string& name) 
    {
        auto itr = sceneObjects.begin();
        for (; itr != sceneObjects.end(); ++itr)
            if ((*itr)->name == name)
                break;
        return itr;
    }
public:
    SceneObjectPtr  getObject(const std::string& name) 
    {
        auto itr = this->_findObject(name);
        return itr == sceneObjects.end() ? SceneObjectPtr(nullptr) : *itr;
    }

    void setObject(const std::string& name, SceneObjectPtr objPtr) 
    {
        auto itr = this->_findObject(name);
        if (objPtr)
        {
            objPtr->name = name;
            if (itr == sceneObjects.end())
                sceneObjects.push_back(objPtr);
            else
                *itr = objPtr;
        }
        else
        {
            if (itr != sceneObjects.end())
            {
                sceneObjects.erase(itr);
            }
        }
    }
    void removeObject(const std::string& name)
    {
        this->setObject(name, nullptr);
    }

    template<typename _ObjT>
    _ObjT* getObject(const std::string& name, bool createIfNotExist = false)  
    {
        SceneObjectPtr ptr = this->getObject(name);
        if (!ptr && createIfNotExist)
        {
            ptr = SceneObjectPtr(new _ObjT());
            this->setObject(name, ptr);
        }
        _ObjT* obj = nullptr;
        if (ptr)
        {
            obj = dynamic_cast<_ObjT*>(ptr.get());
            if (!obj)
                throw std::runtime_error("object found but dynamic_cast failed, please check the type specified.");
        }
        return obj;
    }
    template<typename _ObjT>
    std::vector<_ObjT*>  getAllObjectsOfType() 
    {
        std::vector<_ObjT*>  objs;
        for (auto ptr : this->sceneObjects)
        {
            _ObjT* obj = dynamic_cast<_ObjT*>(ptr.get());
            if(obj)
                objs.push_back(obj);
        }
        return objs;
    }

    Camera* getMainCamera()
    {
        Camera* cam = this->getObject<Camera>("MainCamera", true);
        if (!cam)
            throw std::runtime_error("failed to get main camera");
        return cam;
    }
};


/**
 * Params
 * 存放中途可能会修改的参数
 */

class Params : public BasicData
{
public:

    Params() {}

    ~Params() {}

    std::vector<cv::Matx33f > intrinsicMatrices;
    std::vector<cv::Matx14f > distCoeffs;
    std::string shaderPath;

    //todo add your variable here
    //各算法单位需根据自己的需求在此新增变量或参数，如新增变量较复杂，可新建一个头文件

};

#include <atomic>

/**
 * AppData
 * 存放全局定义的应用程序配置
 */
class AppData : public BasicData
{
public:
    int   argc;
    char** argv;

public:

    AppData();

    ~AppData() {}

    std::atomic<bool> _continue;

    Params params;

    std::string rootDir;

    std::string engineDir;  // abs path to /AREngine

    std::string dataDir;    //abs path to /data/

    bool record=false;

    std::string offlineDataDir=""; // abs path to load offline data

    std::string vfrLinkConfigFile;

    std::string sceneObjConfig;

    std::string animationActionConfigFile;
    std::string animationStateConfigFile;

    bool collectMap = false;  // Reloc module, first run-> set true 
    // offline data structure
    /*
    |-offlineDataDir
        |-cam0  // RGB   images
            |-data.csv
            |-cam_timestamp.txt
            |-data
                |-[timestamp].png
                |-...
        |-cam1  // Depth images
            |-same as cam0
            |-...
        |-imu0  // imu   data
            |-data.csv
    */

    bool isLoadMap;
    bool isSaveMap;
    // // first run
    // bool isLoadMap = false;
    // bool isSaveMap = true;
    // second run
    // bool isLoadMap = true;
    // bool isSaveMap = false;

    bool bUseRelocPose = false; // true：使用重定位传回的位姿。false：使用：与重定位位姿的对齐变换 * SLAM位姿

    //todo add your variable here
    //各算法单位需根据自己的需求在此新增变量或参数，如新增变量较复杂，可新建一个头文件
    std::string interactionConfigFile = "InteractionConfig.json";
};

typedef std::shared_ptr<FrameData> FrameDataPtr;
typedef std::shared_ptr<SceneData>  SceneDataPtr;
typedef std::shared_ptr<AppData>   AppDataPtr;


/**
 * 单手原子手势类别
*/
enum Gesture{
    ZOOM,GRASP,CURSOR,UNDEFINED
};
const std::string GestureNames[] = {"ZOOM", "GRASP", "CURSOR", "UNDEFINED"};

enum Hands{
    LEFT,RIGHT
};

class GestureUnderstandingData{
public:
    /**
     * 当前左手手势
    */
    Gesture curLGesture=Gesture::UNDEFINED;
    /**
     * 当前右手手势
    */
    Gesture curRGesture=Gesture::UNDEFINED;
    /**
     * 最大缩放距离
     */
    float MAX_ZOOM_DISTANCE=0.15;
    /**
     * 最小缩放距离
     */
    float MIN_ZOOM_DISTANCE=0.05;
    /**
     * 缩放速度
     */
    float ZOOM_SPEED=0.01;
    /**
     * 最小缩放尺度
     */
    float MIN_ZOOM_SCALE=0.5;
};

/**
 * Collision box types
 * use by CollisionDetection
 */
enum CollisionBox{
    AABB,
    OBB,
    Sphere,
    Ray,
};
/**
 * CollisionData
 * use by CollisionDetection
 */
class CollisionData {
public:
    std::string modelName = "";
    std::string instanceName = "";
    std::string instanceId = "";
    int originStateIndex = 0;
    cv::Vec3f max = cv::Vec3f(0,0,0);
    cv::Vec3f min = cv::Vec3f(0,0,0);
    cv::Vec3f center = cv::Vec3f(0,0,0);
    std::vector<cv::Vec3f> axes;
    cv::Vec3f extents = cv::Vec3f(0,0,0);
    cv::Vec3f direction = cv::Vec3f(0,0,0);
    std::shared_ptr<SceneObject> obj = nullptr;
    float radius = 1.0f;
    CollisionBox boxType = CollisionBox::AABB;
    void SetBox();
    CollisionData();
    using Ptr = std::shared_ptr<CollisionData>;
    static Ptr create() {
        return std::make_shared<CollisionData>();
    }

    bool isActive = true;
private:
    float _curLength = 0.5f;
    float _curExtentsRatio = 0.5f;
    void _SetAABB();
    void _SetSphere();
    void _SetRay();
};
/**
 * CollisionHandler
 * use by CollisionDetection
 */
enum CollisionType {
    AABB2AABB,
    Ray2AABB,
    Sphere2AABB,
};
/**
 * CollisionHandler
 * use by CollisionDetection
 */
class CollisionHandler {
public:
    std::string class_name;
    using Ptr = std::shared_ptr<CollisionHandler>;
    CollisionHandler(const std::string& class_name) : class_name(class_name) {}
    virtual void OnCollision(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, FrameDataPtr _frameDataPtr, AppData* appDataPtr, SceneData& sceneData) = 0;
};
class CollisionDetectionPair{
private:
    CollisionData::Ptr _obj1;
    CollisionData::Ptr _obj2;
    FrameDataPtr _frameDataPtr;
    AppData* _appDataPtr;
    CollisionType _type;
    CollisionHandler::Ptr _handler;
    bool _isColliding;
    Gesture _lGesture;
    Gesture _rGesture;
public:
    using Ptr = std::shared_ptr<CollisionDetectionPair>;
    static Ptr create(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, CollisionType type, CollisionHandler::Ptr handler, AppData& appData, Gesture lGesture,Gesture rGesture) {
        return std::make_shared<CollisionDetectionPair>(obj1, obj2, type, handler, appData, lGesture,rGesture);
    }
    CollisionDetectionPair(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, CollisionType type, std::shared_ptr<CollisionHandler> handler, AppData& appData, Gesture lGesture,Gesture rGesture);
    void SetColliding(bool isColliding, SceneData& sceneData);
    void SetFrameData(FrameDataPtr frameDataPtr);
    CollisionType GetType();
    std::shared_ptr<CollisionData> GetObj1();
    std::shared_ptr<CollisionData> GetObj2();
    std::shared_ptr<CollisionHandler> GetHandler();
};



#endif //ARENGINE_BASICDATA_H
