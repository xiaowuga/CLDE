#include "BasicData.h"
#include "ConfigLoader.h"
#include"RPC.h"



// HandPose 类成员函数定义
HandPose::HandPose() : tag(hand_tag::Null), joints{} {}

HandPose::HandPose(int tag_value, std::array<cv::Vec3f, 21> joint_positions,
                   std::array<std::array<float, 2>, 21> joints2d_positions,
                   std::array<std::array<std::array<float, 3>, 3>, 16> rotation_matrices,
                   std::array<float, 10> shape_params, cv::Vec3f origin)
        : tag(static_cast<hand_tag>(tag_value)),
          joints(joint_positions), joints2d(joints2d_positions),rotation(rotation_matrices),
          shape(shape_params), landmarkOrigin(origin) {}

HandPose::HandPose(int tag_value, std::vector<cv::Point3f> joint_positions,
                   std::array<std::array<float, 2>, 21> joints2d_positions,
                   std::array<std::array<std::array<float, 3>, 3>, 16> rotation_matrices,
                   std::array<float, 10> shape_params, cv::Vec3f origin)
        :tag(static_cast<hand_tag>(tag_value)),	joints2d(joints2d_positions),rotation(rotation_matrices),shape(shape_params), landmarkOrigin(origin)
{
    for(int i=0;i<jointNum;i++){
        joints[i] = cv::Vec3f(joint_positions[i].x,
                              joint_positions[i].y,
                              joint_positions[i].z);
    }
}

HandPose::HandPose(const HandPose& other)
        : tag(other.tag),
          joints(other.joints),
          joints2d(other.joints2d),
          rotation(other.rotation),
          shape(other.shape),
          landmarkOrigin(other.landmarkOrigin) {}




HandPose& HandPose::operator=(const HandPose& other) {
    if (this != &other) { // 防止自赋值
        tag = other.tag;
        joints = other.joints;
        joints2d = other.joints2d;
        rotation = other.rotation;
        shape = other.shape;
        landmarkOrigin = other.landmarkOrigin;
    }
    return *this;
}
// 获取 joints
std::array<cv::Vec3f, 21>& HandPose::getjoints() {
    return joints;
}
std::array<std::array<float, 2>, 21>& HandPose::getjoints2d() {
    return joints2d;
}

// 获取 rotation
std::array<std::array<std::array<float, 3>, 3>, 16>& HandPose::getrotation() {
    return rotation;
}

// 获取 shape
std::array<float, 10>& HandPose::getshape() {
    return shape;
}
hand_tag HandPose::gettag(){
    return tag;
}

cv::Vec3f HandPose:: getlandmarkOrigin(){
    return landmarkOrigin;
}
void HandPose::setPose(int tag, std::array<cv::Vec3f, HandPose::jointNum> joints, cv::Vec3f landmarkOrigin) {
    this->tag = hand_tag(tag);
    this->joints = joints;

    this->landmarkOrigin = landmarkOrigin;
}

void HandPose::showResult(){
    std::cout<<"joints[i]:  ";
    for(int i=0;i<21;i++){
        std::cout<<joints[i] <<"\t";
    }
    std::cout<<"joints2d[i]:  ";
    for(int i=0;i<21;i++){
        std::cout<<joints2d[i][0]<<" "<< joints2d[i][1]<<"\t";
    }

    std::cout<<"\nrotation[i]:  ";
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                std::cout<<rotation[i][j][k]<<"\t";
            }
        }
        std::cout<<"\t";
    }
    std::cout<<"\nshape[i]:  ";
    for(int i=0;i<10;i++){
        std::cout<<shape[i]<<"\t";
    }
}

// BodyPose 类成员函数定义
BodyPose::BodyPose() :  joints{} {}

BodyPose::BodyPose( std::array<cv::Vec3f, jointNum> joints) :joints(joints) {}


std::array<cv::Vec3f, BodyPose::jointNum> BodyPose::getjoints() {
    return joints;
}

void BodyPose::setPose(std::array<cv::Vec3f, BodyPose::jointNum> joints) {
    this->joints = joints;
}

// FacePose 类成员函数定义
FacePose::FacePose() :  joints{} {}

FacePose::FacePose( std::array<cv::Vec3f, jointNum> joints) :joints(joints) {}


std::array<cv::Vec3f, FacePose::jointNum> FacePose::getjoints() {
    return joints;
}

void FacePose::setPose(std::array<cv::Vec3f, FacePose::jointNum> joints) {
    this->joints = joints;
}


bool BasicData::setData(const std::string& key, const std::any& value)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

	if (!_hasData(key))
	{
		this->data.emplace(key, value);
	}
	else {
		this->data[key] = value;
	}
	return true;
}

std::any BasicData::getData(const std::string& key)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
	if (_hasData(key))
	{
		return data[key];
	}
	return {}; //return empty
}

bool BasicData::hasData(const std::string& key) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return _hasData(key);
}

bool BasicData::removeData(const std::string& key)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
	if (!_hasData(key))
	{
		return false;
	}
	this->data.erase(this->data.find(key));
	return true;
}

void BasicData::clearData()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
	this->data.clear();
}

BasicData::~BasicData()
{}

cv::Matx44f Pose::GetMatrix() const{
    return this->matrix;
}


cv::Matx44f Pose::GetRotation() const
{
    return cv::Matx44f(
            matrix(0, 0), matrix(0, 1), matrix(0, 2), 0,
            matrix(1, 0), matrix(1, 1), matrix(1, 2), 0,
            matrix(2, 0), matrix(2, 1), matrix(2, 2), 0,
            0, 0, 0, 1);
}

cv::Matx44f Pose::GetPosition() const
{
    return cv::Matx44f(
            1, 0, 0, matrix(0, 3),
            0, 1, 0, matrix(1, 3),
            0, 0, 1, matrix(2, 3),
            0, 0, 0, 1);
}

void Pose::setPose(cv::Matx44f mat)
{
    matrix=mat;
}

AppData::AppData()
{
	this->_continue = true;
	// this->setData("engineDir", std::any(std::string(PROJECT_PATH)+"/AREngine"));  // AREngine Dir path
}

void Camera::Init(){
	ConfigLoader configloader(this->filePath);
	this->width = configloader.getValue<int>("width");
	this->height = configloader.getValue<int>("height");
	this->fps = configloader.getValue<int>("fps");
	this->IntrinsicMatrix = configloader.getCamIntrinsicMatrix();
	std::cout << "cam-K:" << this->IntrinsicMatrix << std::endl;
    this->distCoeffs = configloader.getCamDistCoeff();
	std::cout << "cam-K2:" << this->distCoeffs << std::endl;
	return;
}

bool FrameData::hasUploaded(const std::string& key) const
{
	return serilizedFramePtr && serilizedFramePtr->has(key);
}



// CollisionData 类成员函数定义
CollisionData::CollisionData() {
    axes = std::vector<cv::Vec3f>(3);
}
void CollisionData::SetBox() {
    switch (boxType) {
        case CollisionBox::AABB:
            _SetAABB();
            break;
        case CollisionBox::Ray:
            _SetRay();
            break;
        case CollisionBox::Sphere:
            _SetSphere();
            break;
    }
}
void CollisionData::_SetAABB() {
    if(this->obj->name.find("HandNode")!=std::string::npos){
        cv::Matx44f translate = this->obj->transform.GetMatrix() * this->obj->initTransform.GetMatrix();
        cv::Vec3f pos = cv::Vec3f(translate(0, 3), translate(1, 3), translate(2, 3));
        this->center = pos;
        this->max = this->center + this->extents * this->_curExtentsRatio;
        this->min = this->center - this->extents * this->_curExtentsRatio;
    }else {
        cv::Matx44f matrix = this->obj->transform.GetMatrix();
        cv::Vec4f tmp = matrix * cv::Vec4f(center[0], center[1], center[2], 1);
        this->center = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
        tmp = matrix * cv::Vec4f(max[0], max[1], max[2], 1);
        this->max = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
        tmp = matrix * cv::Vec4f(min[0], min[1], min[2], 1);
        this->min = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
    }
}
void CollisionData::_SetRay() {
    cv::Matx44f matrix = this->obj->transform.GetMatrix();
    cv::Vec4f tmp = matrix * cv::Vec4f(center[0], center[1], center[2], 1);
    this->center = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
    this->radius = _curExtentsRatio * 10;
    tmp = matrix * cv::Vec4f(direction[0], direction[1], direction[2], 0);
    this->direction = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
}
void CollisionData::_SetSphere() {
    cv::Matx44f matrix = this->obj->transform.GetMatrix();
    cv::Vec4f tmp = matrix * cv::Vec4f(center[0], center[1], center[2], 1);
    this->center = cv::Vec3f(tmp[0], tmp[1], tmp[2]);
    this->radius = _curExtentsRatio;
}

CollisionDetectionPair::CollisionDetectionPair(std::shared_ptr<CollisionData> obj1, std::shared_ptr<CollisionData> obj2, CollisionType type, std::shared_ptr<CollisionHandler> handler, AppData& appData,  Gesture lGesture,Gesture rGesture){
    _obj1 = obj1;
    _obj2 = obj2;
    _type = type;
    _handler = handler;
    _isColliding = false;
    _lGesture = lGesture;
    _rGesture = rGesture;
    _appDataPtr = &appData;
//    _sceneDataPtr = sceneDataPtr;
}

void CollisionDetectionPair::SetColliding(bool isColliding, SceneData& sceneData){
    _isColliding = isColliding;
    if(_isColliding
    && (_frameDataPtr->gestureDataPtr->curLGesture == _lGesture || _lGesture == Gesture::UNDEFINED)
    && (_frameDataPtr->gestureDataPtr->curRGesture == _rGesture || _rGesture == Gesture::UNDEFINED)
    ){
        _handler->OnCollision(_obj1, _obj2, _frameDataPtr, _appDataPtr, sceneData);
    }
}
void CollisionDetectionPair::SetFrameData(FrameDataPtr frameDataPtr) {
    _frameDataPtr = frameDataPtr;
}
CollisionType CollisionDetectionPair::GetType(){
    return _type;
}
std::shared_ptr<CollisionData> CollisionDetectionPair::GetObj1(){
    return _obj1;
}
std::shared_ptr<CollisionData> CollisionDetectionPair::GetObj2(){
    return _obj2;
}

std::shared_ptr<CollisionHandler> CollisionDetectionPair::GetHandler() {
    return _handler;
}

