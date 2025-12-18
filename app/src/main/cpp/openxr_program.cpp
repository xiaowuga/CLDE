// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"
#include <common/xr_linear.h>
#include <array>
#include <cmath>
#include "app/application.h"
#include "stb_image.h"
#include "ARInput.h"
#include "PoseEstimationRokid.h"

#include "app/utilsmym.hpp"
#include "app/recorder.hpp"

namespace {

// 控制显示手部射线还是控制器射线
// #define USE_HAND_AIM  //使用控制器射线(2025-08-12)

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

namespace Side {
const int LEFT = 0;
const int RIGHT = 1;
const int COUNT = 2;
}  // namespace Side

inline std::string GetXrVersionString(XrVersion ver) {
    return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

inline XrReferenceSpaceCreateInfo GetXrReferenceSpaceCreateInfo(const std::string& referenceSpaceTypeStr) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
    if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
        // Render head-locked 2m in front of device.
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Translation({0.f, 0.f, -2.f}),
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else {
        throw std::invalid_argument(Fmt("Unknown reference space type '%s'", referenceSpaceTypeStr.c_str()));
    }
    return referenceSpaceCreateInfo;
}

PFN_xrGetHistoryCameraPhysicsPose pfnXrGetHistoryCameraPhysicsPose;

struct OpenXrProgram : IOpenXrProgram {
    OpenXrProgram(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                  const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin)
        : m_options(*options), m_platformPlugin(platformPlugin), m_graphicsPlugin(graphicsPlugin)
        {
            m_application = createApplication(this, options, graphicsPlugin);
        }

    ~OpenXrProgram() override {
        if (m_input.actionSet != XR_NULL_HANDLE) {
            for (auto hand : {Side::LEFT, Side::RIGHT}) {
                xrDestroySpace(m_input.handSpace[hand]);
                xrDestroySpace(m_input.aimSpace[hand]);
            }
            xrDestroyActionSet(m_input.actionSet);
        }

        for (Swapchain swapchain : m_swapchains) {
            xrDestroySwapchain(swapchain.handle);
        }

        if (m_appSpace != XR_NULL_HANDLE) {
            xrDestroySpace(m_appSpace);
        }

        if (m_session != XR_NULL_HANDLE) {
            xrDestroySession(m_session);
        }

        if (m_instance != XR_NULL_HANDLE) {
            xrDestroyInstance(m_instance);
        }
        //modify by qi.cheng
        if (markerEnabled && pfnXrRKCloseMarker2 != nullptr) {
            CHECK_XRCMD(pfnXrRKCloseMarker2());
            Log::Write(Log::Level::Info, "Marker recognition resources released.");
        }
    }

    void CheckDeviceSupportExtentions() {
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, nullptr};
        CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

        // Log system properties.
        Log::Write(Log::Level::Info, Fmt("System Properties: Name=%s VendorId=%d", systemProperties.systemName, systemProperties.vendorId));
        Log::Write(Log::Level::Info, Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
                                         systemProperties.graphicsProperties.maxSwapchainImageWidth,
                                         systemProperties.graphicsProperties.maxSwapchainImageHeight,
                                         systemProperties.graphicsProperties.maxLayerCount));
        Log::Write(Log::Level::Info, Fmt("System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
                                         systemProperties.trackingProperties.orientationTracking == XR_TRUE ? "True" : "False",
                                         systemProperties.trackingProperties.positionTracking == XR_TRUE ? "True" : "False"));

    }

    static void LogLayersAndExtensions() {
        // Write out extension properties for a given layer.
        const auto logExtensions = [](const char* layerName, int indent = 0) {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

            std::vector<XrExtensionProperties> extensions(instanceExtensionCount);
            for (XrExtensionProperties& extension : extensions) {
                extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount, extensions.data()));

            const std::string indentStr(indent, ' ');
            Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(), instanceExtensionCount));
            for (const XrExtensionProperties& extension : extensions) {
                Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions:  Name=%s", indentStr.c_str(), extension.extensionName,
                                                    extension.extensionVersion));
            }
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

            std::vector<XrApiLayerProperties> layers(layerCount);
            for (XrApiLayerProperties& layer : layers) {
                layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

            Log::Write(Log::Level::Info, Fmt("Available Layers: (%d)", layerCount));
            for (const XrApiLayerProperties& layer : layers) {
                Log::Write(Log::Level::Info, Fmt("  Name=%s SpecVersion=%s LayerVersion=%d Description=%s", layer.layerName,
                                                        GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description));
                logExtensions(layer.layerName, 4);
            }
        }
    }

    void LogInstanceInfo() {
        CHECK(m_instance != XR_NULL_HANDLE);

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

        Log::Write(Log::Level::Info, Fmt("Instance RuntimeName=%s RuntimeVersion=%s", instanceProperties.runtimeName,
                                         GetXrVersionString(instanceProperties.runtimeVersion).c_str()));
    }

    void CreateInstanceInternal() {
        CHECK(m_instance == XR_NULL_HANDLE);

        // Create union of extensions required by platform and graphics plugins.
        std::vector<const char*> extensions;
        // Transform platform and graphics extension std::strings to C strings.
        const std::vector<std::string> platformExtensions = m_platformPlugin->GetInstanceExtensions();
        std::transform(platformExtensions.begin(), platformExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });
        const std::vector<std::string> graphicsExtensions = m_graphicsPlugin->GetInstanceExtensions();
        std::transform(graphicsExtensions.begin(), graphicsExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

        extensions.push_back(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);

        //hand tracking
        extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = m_platformPlugin->GetInstanceCreateExtension();
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();
        
        strcpy(createInfo.applicationInfo.applicationName, "RokidOpenXRAndroidDemo");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        Log::Write(Log::Level::Error, Fmt("====== RokidOpenXRAndroidDemo createInfo: %d", createInfo));
        Log::Write(Log::Level::Error, Fmt("====== RokidOpenXRAndroidDemo m_instance: %d", m_instance));
        CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));


    }

    void CreateInstance() override {
        LogLayersAndExtensions();
        CreateInstanceInternal();
        LogInstanceInfo();

        //hand tracking
        PFN_INITIALIZE(xrCreateHandTrackerEXT);
        PFN_INITIALIZE(xrDestroyHandTrackerEXT);
        PFN_INITIALIZE(xrLocateHandJointsEXT);
    }

    void LogViewConfigurations() {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        uint32_t viewConfigTypeCount;
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
        std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount, viewConfigTypes.data()));
        CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

        Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)", viewConfigTypeCount));
        for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
            Log::Write(Log::Level::Verbose, Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
                                                viewConfigType == m_options.Parsed.ViewConfigType ? "(Selected)" : ""));

            XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
            CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

            Log::Write(Log::Level::Verbose, Fmt("  View configuration FovMutable=%s", viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
            if (viewCount > 0) {
                std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
                CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

                for (uint32_t i = 0; i < views.size(); i++) {
                    const XrViewConfigurationView& view = views[i];

                    Log::Write(Log::Level::Verbose, Fmt(" View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i,
                                                            view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                                            view.recommendedSwapchainSampleCount));
                    Log::Write(Log::Level::Verbose, Fmt(" View [%d]:     Maximum Width=%d Height=%d SampleCount=%d", i, view.maxImageRectWidth,
                                                            view.maxImageRectHeight, view.maxSwapchainSampleCount));
                }
            } else {
                Log::Write(Log::Level::Error, Fmt("Empty view configuration type"));
            }

            LogEnvironmentBlendMode(viewConfigType);
        }
    }

    void LogEnvironmentBlendMode(XrViewConfigurationType type) {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != 0);

        uint32_t count;
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0, &count, nullptr));
        CHECK(count > 0);

        Log::Write(Log::Level::Info, Fmt("Available Environment Blend Mode count : (%d)", count));

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

        bool blendModeFound = false;
        for (XrEnvironmentBlendMode mode : blendModes) {
            const bool blendModeMatch = (mode == m_options.Parsed.EnvironmentBlendMode);
            Log::Write(Log::Level::Info, Fmt("Environment Blend Mode (%s) : %s", to_string(mode), blendModeMatch ? "(Selected)" : ""));
            blendModeFound |= blendModeMatch;
        }
        CHECK(blendModeFound);
    }

    void InitializeSystem() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId == XR_NULL_SYSTEM_ID);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = m_options.Parsed.FormFactor;
        CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

        Log::Write(Log::Level::Verbose, Fmt("Using system %d for form factor %s", m_systemId, to_string(m_options.Parsed.FormFactor)));
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        LogViewConfigurations();

        // The graphics API can initialize the graphics device now that the systemId and instance
        // handle are available.
        m_graphicsPlugin->InitializeDevice(m_instance, m_systemId);
    }

    void LogReferenceSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);

        uint32_t spaceCount;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, spaces.data()));

        Log::Write(Log::Level::Info, Fmt("Available reference spaces: %d", spaceCount));
        for (XrReferenceSpaceType space : spaces) {
            Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
        }
    }

    struct InputState {
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<XrSpace, Side::COUNT> handSpace;
        std::array<XrSpace, Side::COUNT> aimSpace;
        XrAction handAimPoseAction{XR_NULL_HANDLE};
        XrAction handPinchAction{XR_NULL_HANDLE};
        XrAction handGripAction{XR_NULL_HANDLE};

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction selectAction{XR_NULL_HANDLE};
        XrAction xAction{XR_NULL_HANDLE};
        XrAction oAction{XR_NULL_HANDLE};
        XrAction upAction{XR_NULL_HANDLE};
        XrAction downAction{XR_NULL_HANDLE};
        XrAction leftAction{XR_NULL_HANDLE};
        XrAction rightAction{XR_NULL_HANDLE};
        XrAction menuAction{XR_NULL_HANDLE};
        XrAction gamepadPoseAction{XR_NULL_HANDLE};
        XrPath gamepadPoseSubactionPath;
        XrSpace gamepadPoseSpace;
        XrBool32 gamepadPoseActive;
    };

    // 自定义扩展接口
    PFN_xrRecenterPhonePose pfnXrRecenterPhonePose = nullptr;
    PFN_xrRecenterHeadTracker pfnXrRecenterHeadTracker = nullptr;
    PFN_xrGetHeadTrackingStatus pfnGetHeadTrackingStatus = nullptr;
    PFN_xrGetCameraPhysicsPose pfnXrGetCameraPhysicsPose = nullptr;
    PFN_xrOpenCameraPreview pfnXrOpenCameraPreview = nullptr;
    PFN_xrCloseCameraPreview pfnXrCloseCameraPreview = nullptr;
    //================================================================
    PFN_xrGetDistortion pfnXrGetDistortion=nullptr;
    PFN_xrGetFocalLength pfnXrGetFocalLength=nullptr;
    PFN_xrGetPrincipalPoint pfnXrGetPrincipalPoint=nullptr;
    //================================================================
    // 相机数据回调(640*480, y-channel data)
    static void OnCameraUpdate(const uint8_t* data, uint32_t size, uint32_t width, uint32_t height, uint64_t timestamp) {
        float position[3]={},orientation[4]={};
        if(pfnXrGetHistoryCameraPhysicsPose != nullptr) {
            CHECK_XRCMD(pfnXrGetHistoryCameraPhysicsPose(timestamp, position, orientation));
            //Log::Write(Log::Level::Info, Fmt("OnCameraUpdate Call pfnXrGetHistoryCameraPhysicsPose in App, position=(%f, %f, %f) orientation=(%f, %f, %f, %f)", position[0], position[1], position[2], orientation[0], orientation[1], orientation[2], orientation[3]));
        }
        //std::cout << "Received camera frame: " << width << "x" << height << " Size: " << size << " Timestamp: " << timestamp << std::endl;
        if(data == nullptr || size == 0) {
            errorf("Invalid camera data!"); return;
        }
        // Calc Matrix From XrGetHistoryCameraPhysicsPose
        XrMatrix4x4f result{}, tmp{};
        XrQuaternionf quat={orientation[0], orientation[1],orientation[2],orientation[3]};
        XrMatrix4x4f_CreateFromQuaternion(&result, &quat);
        XrVector3f t={position[0],position[1],position[2]};
        XrVector3f scale={1.f,1.f,1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&tmp,&t,&quat,&scale);
        XrMatrix4x4f_InvertRigidBody(&result, &tmp);
        cv::Matx44f m;
        //GetGLModelView(R, t, m.val, true);
        memcpy(m.val,result.m,sizeof(float)*16);

        // 假设数据是 RGB888 格式
        cv::Mat image=cv::Mat((int)height,(int)width, CV_8UC1,(void*)data).clone();


        if(false){ // Record Image
            static bool isfirst=true;
            static Recorder RR;
            static std::ofstream file;
            if(isfirst){
                std::string s="Download/"+CurrentDateTime("%Y-%m-%d_%H-%M-%S");
                RR.set_recorder_save_dir(s);
                file.open(MakeSdcardPath(s+"/position.txt"));
                RR.start_recording();
                isfirst=false;
            }
            RR.record_image(image);
            std::ostringstream oss;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) oss << m(i, j) << ",";
            std::string mat_str = oss.str();
            if (!mat_str.empty()) mat_str.pop_back();
            file<<mat_str<<std::endl;
        }
        _RokidOriginalCameraImage=image; //** 调试用，需要删除！！！***

        ARInputSources::FrameData frame_data;
        frame_data.img=image; frame_data.timestamp=timestamp; frame_data.cameraMat=m;
        ARInputSources::instance()->set(frame_data,ARInputSources::DATAF_CAMERAMAT|ARInputSources::DATAF_IMAGE|ARInputSources::DATAF_TIMESTAMP);
        //执行自行添加的回调函数
        for(const auto &[key,func]:CameraUpdateCallbackList) func(image,m,timestamp);
    }


    //modify by qi.cheng
    //Added custom extension interface definitions for marker recognition
    bool markerEnabled = false;
    PFN_xrRKOpenMarker2 pfnXrRKOpenMarker2 = nullptr;
    PFN_xrRKCloseMarker2 pfnXrRKCloseMarker2 = nullptr;
    PFN_xrRKMarker2AcquireChanges pfnXrRKMarker2AcquireChanges = nullptr;
    PFN_xrRKMarker2AddImagePhy pfnXrRKMarker2AddImagePhy = nullptr;
    PFN_xrRKMarker2GetCenterPose pfnXrRKMarker2GetCenterPose = nullptr;
    PFN_xrRKMarker2GetExtent pfnXrRKMarker2GetExtent = nullptr;
    PFN_xrRKMarker2GetId pfnXrRKMarker2GetId = nullptr;
    PFN_xrRKMarker2GetAlgoId pfnXrRKMarker2GetAlgoId = nullptr;

    //Added custom extension interface definitions for plane recognition.
    // 平面检测相关成员变量
    bool planeTrackingEnabled = false;                // 平面检测功能是否启用
    int currentPlaneDetectMode = 0;                   // 当前平面检测模式
    // 平面检测功能接口
    PFN_xrRKOpenPlaneTracker pfnXrRKOpenPlaneTracker = nullptr;
    PFN_xrRKClosePlaneTracker pfnXrRKClosePlaneTracker = nullptr;
    PFN_xrRKGetPlaneDetectMode pfnXrRKGetPlaneDetectMode = nullptr;
    PFN_xrRKSetPlaneDetectMode pfnXrRKSetPlaneDetectMode = nullptr;
    PFN_xrRKGetUpdatePlanes pfnXrRKGetUpdatePlanes = nullptr;
    PFN_xrRKGetPlanePolygon pfnXrRKGetPlanePolygon = nullptr;
    PFN_xrRKGetPlaneType pfnXrRKGetPlaneType = nullptr;
    PFN_xrRKGetPlaneCenterPose pfnXrRKGetPlaneCenterPose = nullptr;
    PFN_xrRKGetPlaneRectangle pfnXrRKGetPlaneRectangle = nullptr;
    PFN_xrRKReleasePlane pfnXrRKReleasePlane = nullptr;
    PFN_xrRKCreateArtificialPlane pfnXrRKCreateArtificialPlane = nullptr;

    //Initialize Marker recognition interface.
    void InitializeMarker() {
        Log::Write(Log::Level::Info, "Initializing Marker recognition...");
        //Load Marker extension interfaces
        if (pfnXrRKOpenMarker2 == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKOpenMarker2", (PFN_xrVoidFunction*)(&pfnXrRKOpenMarker2)))) {
            }
        }
        if (pfnXrRKCloseMarker2 == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKCloseMarker2", (PFN_xrVoidFunction*)(&pfnXrRKCloseMarker2)))) {
            }
        }
        if (pfnXrRKMarker2AcquireChanges == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2AcquireChanges", (PFN_xrVoidFunction*)(&pfnXrRKMarker2AcquireChanges)))) {
            }
        }
        if (pfnXrRKMarker2AddImagePhy == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2AddImagePhy", (PFN_xrVoidFunction*)(&pfnXrRKMarker2AddImagePhy)))) {
            }
        }
        if (pfnXrRKMarker2GetCenterPose == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2GetCenterPose", (PFN_xrVoidFunction*)(&pfnXrRKMarker2GetCenterPose)))) {
            }
        }
        if (pfnXrRKMarker2GetExtent == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2GetExtent", (PFN_xrVoidFunction*)(&pfnXrRKMarker2GetExtent)))) {
            }
        }
        if (pfnXrRKMarker2GetId == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2GetId", (PFN_xrVoidFunction*)(&pfnXrRKMarker2GetId)))) {
            }
        }
        if (pfnXrRKMarker2GetAlgoId == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKMarker2GetAlgoId", (PFN_xrVoidFunction*)(&pfnXrRKMarker2GetAlgoId)))) {
            }
        }

        // 初始化 Marker 数据库
        if (pfnXrRKOpenMarker2 != nullptr) {
            CHECK_XRCMD(pfnXrRKOpenMarker2(nullptr, 0)); // 传入空数据库初始化
            markerEnabled = true;
            Log::Write(Log::Level::Info, "Marker recognition initialized.");
        } else {
            Log::Write(Log::Level::Error, "Failed to initialize Marker recognition.");
        }
    }
    //处理 Marker 数据
    void ProcessMarkerData(MarkerData &markerData) {
        if (!markerEnabled || pfnXrRKMarker2AcquireChanges == nullptr) {
            Log::Write(Log::Level::Warning, "====== Marker recognition is not enabled or acquire changes function is null.");
            return;
        }
        XrRokidMarker2ArrayExt added{}, updated{}, removed{};
        XrResult result = pfnXrRKMarker2AcquireChanges(&added, &updated, &removed);
        if (XR_FAILED(result)) {
            Log::Write(Log::Level::Error, Fmt("====== Failed to acquire marker changes. Error code: %d", result));
            return;
        }

        auto getMarkerData=[this](XrRokidMarker2ArrayExt &data, std::vector<MarkerData::DMarker> &v){
            v.resize(0);
            for (uint32_t i = 0; i < data.size; ++i) {
                intptr_t marker = data.elements[i];
                MarkerData::DMarker dm={};
                dm.id=0; //!!to be fixed
                if (pfnXrRKMarker2GetCenterPose != nullptr) {
                    XrResult result = pfnXrRKMarker2GetCenterPose(marker, dm.pose);
                    if (XR_SUCCEEDED(result)) {
                        auto pose=dm.pose;
                        Log::Write(Log::Level::Info, Fmt("====== Marker added with pose: [%f, %f, %f, %f, %f, %f, %f]",pose[0], pose[1], pose[2], pose[3], pose[4], pose[5], pose[6]));
                        v.push_back(dm);
                    } else {
                        Log::Write(Log::Level::Error, Fmt("====== Failed to get center pose for added marker: %ld. Error code: %d", marker, result));
                    }
                }
            }
        };
        // 处理新增的 Marker
        Log::Write(Log::Level::Info, Fmt("====== Processing added markers. Count: %d", added.size));
        getMarkerData(added, markerData.added);
        getMarkerData(updated, markerData.updated);
//        getMarkerData(removed, markerData.removed);
        // 处理更新的 Marker
        for (uint32_t i = 0; i < updated.size; ++i) {
            intptr_t marker = updated.elements[i];
            Log::Write(Log::Level::Info, Fmt("====== Marker updated: %ld", marker));
        }
        // 处理移除的 Marker
        for (uint32_t i = 0; i < removed.size; ++i) {
            intptr_t marker = removed.elements[i];
            Log::Write(Log::Level::Info, Fmt("====== Marker removed: %ld", marker));
        }
    }
    //添加 Marker 数据
    int AddMarkerImages(const std::vector<MarkerInfo> &markers) override {
        // 指定图片路径
        // List of image paths
//        std::vector<std::string> imagePaths = {
//                "/storage/emulated/0/pictures/sunflower.png",
//                "/storage/emulated/0/pictures/aurora.png", // Add more images as needed
//                "/storage/emulated/0/pictures/terrace.png"
//        };
        int n_ok=0;
        for (const auto& marker : markers) {
            auto &filePath=marker.markerImageFile;
            // 定义图像属性变量
            int width, height, channels;

            // 使用 stb_image 加载 PNG 图像并转换为灰度图
            uint8_t *imageData = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_grey);
            if (!imageData) {
                // 加载失败的日志
                Log::Write(Log::Level::Error, Fmt("Failed to load image: %s", filePath.c_str()));
                continue;
            }

            // 检查并调用 pfnXrRKMarker2AddImagePhy 接口
            if (pfnXrRKMarker2AddImagePhy != nullptr) {
                // 将图像数据添加到 Marker 数据库
                CHECK_XRCMD(pfnXrRKMarker2AddImagePhy(
                        filePath.c_str(), // Marker ID
                        imageData,          // 灰度图像素数据
                        width,              // 图像宽度（像素）
                        height,             // 图像高度（像素）
                        width,              // 每行字节数（灰度图等于宽度）
                        marker.phyWidth,               // 图像物理宽度（米），根据实际场景调整
                        marker.phyHeight// 图像物理高度（米），根据实际场景调整
                ));
                ++n_ok;
                // 添加成功的日志
                Log::Write(Log::Level::Info, Fmt("Marker image added: %s", filePath.c_str()));
            } else {
                Log::Write(Log::Level::Error, "Marker addition interface is not initialized.");
            }
            // 释放加载的图片数据
            stbi_image_free(imageData);
        }
        return n_ok;
    }

    // 初始化平面检测
    void InitializePlaneTracking() {
        Log::Write(Log::Level::Info, "Initializing Plane Tracking...");

        // 加载平面检测扩展接口
        if (pfnXrRKOpenPlaneTracker == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKOpenPlaneTracker", (PFN_xrVoidFunction*)(&pfnXrRKOpenPlaneTracker)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKOpenPlaneTracker.");
            }
        }

        if (pfnXrRKClosePlaneTracker == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKClosePlaneTracker", (PFN_xrVoidFunction*)(&pfnXrRKClosePlaneTracker)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKClosePlaneTracker.");
            }
        }

        if (pfnXrRKGetPlaneDetectMode == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetPlaneDetectMode", (PFN_xrVoidFunction*)(&pfnXrRKGetPlaneDetectMode)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetPlaneDetectMode.");
            }
        }

        if (pfnXrRKSetPlaneDetectMode == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKSetPlaneDetectMode", (PFN_xrVoidFunction*)(&pfnXrRKSetPlaneDetectMode)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKSetPlaneDetectMode.");
            }
        }

        if (pfnXrRKGetUpdatePlanes == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetUpdatePlanes", (PFN_xrVoidFunction*)(&pfnXrRKGetUpdatePlanes)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetUpdatePlanes.");
            }
        }

        if (pfnXrRKGetPlanePolygon == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetPlanePolygon", (PFN_xrVoidFunction*)(&pfnXrRKGetPlanePolygon)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetPlanePolygon.");
            }
        }

        if (pfnXrRKGetPlaneType == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetPlaneType", (PFN_xrVoidFunction*)(&pfnXrRKGetPlaneType)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetPlaneType.");
            }
        }

        if (pfnXrRKGetPlaneCenterPose == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetPlaneCenterPose", (PFN_xrVoidFunction*)(&pfnXrRKGetPlaneCenterPose)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetPlaneCenterPose.");
            }
        }

        if (pfnXrRKGetPlaneRectangle == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKGetPlaneRectangle", (PFN_xrVoidFunction*)(&pfnXrRKGetPlaneRectangle)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKGetPlaneRectangle.");
            }
        }

        if (pfnXrRKReleasePlane == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKReleasePlane", (PFN_xrVoidFunction*)(&pfnXrRKReleasePlane)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKReleasePlane.");
            }
        }

        if (pfnXrRKCreateArtificialPlane == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRKCreateArtificialPlane", (PFN_xrVoidFunction*)(&pfnXrRKCreateArtificialPlane)))) {
                Log::Write(Log::Level::Info, "Loaded xrRKCreateArtificialPlane.");
            }
        }

        // 启用平面检测功能
        if (pfnXrRKOpenPlaneTracker != nullptr) {
            if (XR_SUCCEEDED(pfnXrRKOpenPlaneTracker(3))) {  // 设置为水平+竖直检测模式
                planeTrackingEnabled = true;
                Log::Write(Log::Level::Info, "Plane Tracking initialized and started.");
            } else {
                Log::Write(Log::Level::Error, "Failed to start Plane Tracking.");
            }
        }
    }

    //处理平面识别数据
    void ProcessPlaneTracking() {
        if (!planeTrackingEnabled || pfnXrRKGetUpdatePlanes == nullptr) {
            return;
        }
        void *nochangePlanes = nullptr, *changePlanes = nullptr, *newPlanes = nullptr, *removePlanes = nullptr;
        uint32_t nochangePlaneSize = 0, changePlaneSize = 0, newPlaneSize = 0, removePlaneSize = 0;


        // 获取平面更新数据
        CHECK_XRCMD(
                pfnXrRKGetUpdatePlanes(0, &nochangePlanes, &changePlanes, &newPlanes, &removePlanes,
                                       &nochangePlaneSize, &changePlaneSize, &newPlaneSize,
                                       &removePlaneSize));

        Log::Write(Log::Level::Info,
                   Fmt("Plane Tracking Data: NoChange: %d, Changed: %d, New: %d, Removed: %d",
                       nochangePlaneSize, changePlaneSize, newPlaneSize, removePlaneSize));

        // 处理新增的平面
        for (int i = 0; i < newPlaneSize; ++i) {
            long plane = ((long *) newPlanes)[i];
            uint32_t type = 0;
            CHECK_XRCMD(pfnXrRKGetPlaneType(plane, &type));

            Log::Write(Log::Level::Info,
                       Fmt("New plane detected. Type: %d, Handle: %ld", type, plane));

            // 获取平面中心点和法线
            void *pose = nullptr, *center = nullptr, *normalVector = nullptr;
            CHECK_XRCMD(pfnXrRKGetPlaneCenterPose(plane, &pose, &center, &normalVector));

            Log::Write(Log::Level::Info,
                       Fmt("Plane center and normal vector acquired for plane: %ld", plane));

        }
    }

    void InitializeActions() {
        // Create an action set.
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy_s(actionSetInfo.actionSetName, "rokidstation");
            strcpy_s(actionSetInfo.localizedActionSetName, "Rokid Station Controller");
            actionSetInfo.priority = 0;
            CHECK_XRCMD(xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
        }

        // Get the XrPath for the left and right hands - we will use them as subaction paths.
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left", &m_input.handSubactionPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right", &m_input.handSubactionPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad", &m_input.gamepadPoseSubactionPath));

        // Create actions.
        {
            // Create an input action getting the left and right hand poses.
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "hand_aim_pose");
            strcpy_s(actionInfo.localizedActionName, "hand_aim_pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.handAimPoseAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "hand_pinch");
            strcpy_s(actionInfo.localizedActionName, "hand_pinch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.handPinchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "hand_grip");
            strcpy_s(actionInfo.localizedActionName, "hand_grip");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.handGripAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "gamepadpose");
            strcpy_s(actionInfo.localizedActionName, "gamepadpose");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gamepadPoseAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "select");
            strcpy_s(actionInfo.localizedActionName, "select");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.selectAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "x");
            strcpy_s(actionInfo.localizedActionName, "x");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.xAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "o");
            strcpy_s(actionInfo.localizedActionName, "o");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.oAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "up");
            strcpy_s(actionInfo.localizedActionName, "up");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.upAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "down");
            strcpy_s(actionInfo.localizedActionName, "down");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.downAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "left");
            strcpy_s(actionInfo.localizedActionName, "left");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.leftAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "right");
            strcpy_s(actionInfo.localizedActionName, "right");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.rightAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "menu");
            strcpy_s(actionInfo.localizedActionName, "menu");
            actionInfo.countSubactionPaths = 1;
            actionInfo.subactionPaths = &m_input.gamepadPoseSubactionPath;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.menuAction));
        }

        XrPath  selectClickPath;
        XrPath  xClickPath;
        XrPath  oClickPath;
        XrPath  upClickPath;
        XrPath  downClickPath;
        XrPath  leftClickPath;
        XrPath  rightClickPath;
        XrPath  menuClickPath;
        XrPath  gamepadePosePath;
        std::array<XrPath, Side::COUNT>  aimPosePath;
        std::array<XrPath, Side::COUNT>  handPinchPath;
        std::array<XrPath, Side::COUNT>  handGripPath;
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/select/click", &selectClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/x/click", &xClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/o/click", &oClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/up/click", &upClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/down/click", &downClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/left/click", &leftClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/right/click", &rightClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/menu/click", &menuClickPath));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/gamepad/input/aim/pose", &gamepadePosePath));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/aim/pose", &aimPosePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/aim/pose", &aimPosePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/pinch/click", &handPinchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/pinch/click", &handPinchPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click", &handGripPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &handGripPath[Side::RIGHT]));


        // Suggest bindings for the Rokid Station Controller.
        {
            const char* gamepadProfilePath = "/interaction_profiles/rokid/gamepad_controller";
            const char* handProfilePath = "/interaction_profiles/rokid/hand_interaction";
            XrPath gamepadInteractionProfilePath;
            XrPath handInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, gamepadProfilePath, &gamepadInteractionProfilePath));
            CHECK_XRCMD(xrStringToPath(m_instance, handProfilePath, &handInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> gamepadbindings{{// Fall back to a click input for the grab action.
                                                                   {m_input.gamepadPoseAction, gamepadePosePath},
                                                                   {m_input.selectAction, selectClickPath},
                                                                   {m_input.xAction, xClickPath},
                                                                   {m_input.oAction, oClickPath},
                                                                   {m_input.upAction, upClickPath},
                                                                   {m_input.downAction, downClickPath},
                                                                   {m_input.leftAction, leftClickPath},
                                                                   {m_input.rightAction, rightClickPath},
                                                                   {m_input.menuAction, menuClickPath}}};

            std::vector<XrActionSuggestedBinding> handbindings{{// Fall back to a click input for the grab action.
                                                                   {m_input.handPinchAction, handPinchPath[Side::LEFT]},
                                                                   {m_input.handPinchAction, handPinchPath[Side::RIGHT]},
                                                                   {m_input.handGripAction, handGripPath[Side::LEFT]},
                                                                   {m_input.handGripAction, handGripPath[Side::RIGHT]},
                                                                   {m_input.handAimPoseAction, aimPosePath[Side::LEFT]},
                                                                   {m_input.handAimPoseAction, aimPosePath[Side::RIGHT]}}};


            XrInteractionProfileSuggestedBinding suggestedGamepadBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedGamepadBindings.interactionProfile = gamepadInteractionProfilePath;
            suggestedGamepadBindings.suggestedBindings = gamepadbindings.data();
            suggestedGamepadBindings.countSuggestedBindings = (uint32_t)gamepadbindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedGamepadBindings));

            XrInteractionProfileSuggestedBinding suggestedHandBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedHandBindings.interactionProfile = handInteractionProfilePath;
            suggestedHandBindings.suggestedBindings = handbindings.data();
            suggestedHandBindings.countSuggestedBindings = (uint32_t)handbindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedHandBindings));
        }

        XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceInfo.action = m_input.handAimPoseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::RIGHT]));

        XrActionSpaceCreateInfo gamepadPoseSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        gamepadPoseSpaceInfo.action = m_input.gamepadPoseAction;
        gamepadPoseSpaceInfo.poseInActionSpace.orientation.w = 1.f;
        gamepadPoseSpaceInfo.subactionPath = m_input.gamepadPoseSubactionPath;
        CHECK_XRCMD(xrCreateActionSpace(m_session, &gamepadPoseSpaceInfo, &m_input.gamepadPoseSpace));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_input.actionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));

        if (pfnXrRecenterPhonePose == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRecenterPhonePose", (PFN_xrVoidFunction*)(&pfnXrRecenterPhonePose)))) {
            }
        }

        if (pfnXrRecenterHeadTracker == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrRecenterHeadTracker", (PFN_xrVoidFunction*)(&pfnXrRecenterHeadTracker)))) {
            }
        }


        if (pfnGetHeadTrackingStatus == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetHeadTrackingStatus", (PFN_xrVoidFunction*)(&pfnGetHeadTrackingStatus)))) {
                Log::Write(Log::Level::Info, Fmt("Found xrGetHeadTrackingStatus is got!!!"));
            }
            else {
                Log::Write(Log::Level::Info, Fmt("app pfnGetHeadTrackingStatus not find"));
            }
        }

        if (pfnXrGetHistoryCameraPhysicsPose == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetHistoryCameraPhysicsPose", (PFN_xrVoidFunction*)(&pfnXrGetHistoryCameraPhysicsPose)))) {
                Log::Write(Log::Level::Info, "Found the PFN_xrGetHistoryCameraPhysicsPose in App");
            }
        }
        if (pfnXrGetCameraPhysicsPose == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetCameraPhysicsPose", (PFN_xrVoidFunction*)(&pfnXrGetCameraPhysicsPose)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrGetCameraPhysicsPose in App");
            }
        }
        if (pfnXrOpenCameraPreview == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrOpenCameraPreview", (PFN_xrVoidFunction*)(&pfnXrOpenCameraPreview)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrOpenCameraPreview in App start.........");
            }
        }
        if (pfnXrCloseCameraPreview == nullptr) {
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrCloseCameraPreview", (PFN_xrVoidFunction*)(&pfnXrCloseCameraPreview)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrCloseCameraPreview in App start.........");
            }
        }
        if(pfnXrGetDistortion==nullptr){
            if(XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetDistortion",(PFN_xrVoidFunction*)(&pfnXrGetDistortion)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrGetDistortion in App start.........");
            }
        }
        if(pfnXrGetFocalLength==nullptr){
            if(XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetFocalLength",(PFN_xrVoidFunction*)(&pfnXrGetFocalLength)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrGetFocalLength in App start.........");
            }
        }
        if(pfnXrGetPrincipalPoint==nullptr){
            if(XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, "xrGetPrincipalPoint",(PFN_xrVoidFunction*)(&pfnXrGetPrincipalPoint)))) {
                Log::Write(Log::Level::Info, "Found the pfnXrGetPrincipalPoint in App start.........");
            }
        }
    }

    void CreateVisualizedSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_ViewSpace));
    }

    void InitializeSession() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_session == XR_NULL_HANDLE);

        {
            Log::Write(Log::Level::Verbose, Fmt("Creating session..."));
            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = m_graphicsPlugin->GetGraphicsBinding();
            createInfo.systemId = m_systemId;
            CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
        }

        CheckDeviceSupportExtentions();
        LogReferenceSpaces();
        InitializeActions();
        CreateVisualizedSpaces();

        {
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
            CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));
        }

        //hand tracking
        if (xrCreateHandTrackerEXT) {
            for (auto hand : {Side::LEFT, Side::RIGHT}) {
                XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                createInfo.hand = (hand == Side::LEFT ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
                CHECK_XRCMD(xrCreateHandTrackerEXT(m_session, &createInfo, &m_handTracker[hand]));
            }
        }
    }

    void InitializeApplication() override {
        m_application->initialize(m_instance, m_session);
    }

    void CreateSwapchains() override {
        CHECK(m_session != XR_NULL_HANDLE);
        CHECK(m_swapchains.empty());
        CHECK(m_configViews.empty());

        // Note: No other view configurations exist at the time this code was written. If this
        // condition is not met, the project will need to be audited to see how support should be
        // added.
        CHECK_MSG(m_options.Parsed.ViewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, "Unsupported view configuration type");

        // Query and cache view configuration views.
        uint32_t viewCount;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, 0, &viewCount, nullptr));
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount, &viewCount, m_configViews.data()));

        // Create and cache view buffer for xrLocateViews later.
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        // Create the swapchain and get the images.
        if (viewCount > 0) {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());
            m_colorSwapchainFormat = m_graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats);

            // Print swapchain formats and the selected one.
            {
                std::string swapchainFormatsString;
                for (int64_t format : swapchainFormats) {
                    const bool selected = format == m_colorSwapchainFormat;
                    swapchainFormatsString += " ";
                    if (selected) {
                        swapchainFormatsString += "[";
                    }
                    swapchainFormatsString += std::to_string(format);
                    if (selected) {
                        swapchainFormatsString += "]";
                    }
                }
                Log::Write(Log::Level::Verbose, Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
            }

            // Create a swapchain for each view.
            for (uint32_t i = 0; i < viewCount; i++) {
                const XrViewConfigurationView& vp = m_configViews[i];
                Log::Write(Log::Level::Info, Fmt("Creating swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d, maxSampleCount=%d", i,
                                                    vp.recommendedImageRectWidth, vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount, vp.maxSwapchainSampleCount));

                // Create the swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCreateInfo.arraySize = 1;
                swapchainCreateInfo.format = m_colorSwapchainFormat;
                swapchainCreateInfo.width = vp.recommendedImageRectWidth;
                swapchainCreateInfo.height = vp.recommendedImageRectHeight;
                swapchainCreateInfo.mipCount = 1;
                swapchainCreateInfo.faceCount = 1;
                swapchainCreateInfo.sampleCount = m_graphicsPlugin->GetSupportedSwapchainSampleCount(vp);
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                Swapchain swapchain;
                swapchain.width = swapchainCreateInfo.width;
                swapchain.height = swapchainCreateInfo.height;
                CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

                m_swapchains.push_back(swapchain);

                uint32_t imageCount;
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
                // XXX This should really just return XrSwapchainImageBaseHeader*
                std::vector<XrSwapchainImageBaseHeader*> swapchainImages = m_graphicsPlugin->AllocateSwapchainImageStructs(imageCount, swapchainCreateInfo);
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

                m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
            }
        }
    }

    // Return event if one is available, otherwise return null.
    const XrEventDataBaseHeader* TryReadNextEvent() {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost));
            }
            return baseHeader;
        }
        if (xr == XR_EVENT_UNAVAILABLE) {
            return nullptr;
        }
        THROW_XR(xr, "xrPollEvent");
    }

    void PollEvents(bool* exitRenderLoop, bool* requestRestart) override {
        *exitRenderLoop = *requestRestart = false;

        if (ExitAppByKey) {
            *exitRenderLoop = true;
            return;
        }

        // Process all pending messages.
        while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
                    Log::Write(Log::Level::Warning, Fmt("XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime));
                    *exitRenderLoop = true;
                    *requestRestart = true;
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                    HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
                    break;
                }
            }
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart) {
        const XrSessionState oldState = m_sessionState;
        m_sessionState = stateChangedEvent.state;

        Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState),
                                         to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
            Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
            return;
        }

        switch (m_sessionState) {
            case XR_SESSION_STATE_READY: {
                CHECK(m_session != XR_NULL_HANDLE);
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = m_options.Parsed.ViewConfigType;
                CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                CHECK(m_session != XR_NULL_HANDLE);
                m_sessionRunning = false;
                CHECK_XRCMD(xrEndSession(m_session))
                break;
            }
            case XR_SESSION_STATE_EXITING: {
                *exitRenderLoop = true;
                // Do not attempt to restart because user closed this session.
                *requestRestart = false;
                break;
            }
            case XR_SESSION_STATE_LOSS_PENDING: {
                *exitRenderLoop = true;
                // Poll for a new instance.
                *requestRestart = true;
                break;
            }
            default:
                break;
        }
    }

    bool IsSessionRunning() const override { return m_sessionRunning; }

    bool IsSessionFocused() const override { return m_sessionState == XR_SESSION_STATE_FOCUSED; }

    void PollActions() override {
        // Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

        static float joystick_x[Side::COUNT] = {0};
        static float joystick_y[Side::COUNT] = {0};
        static float trigger[Side::COUNT] = {0};
        static float squeeze[Side::COUNT] = {0};

        // Get pose and grab action state and start haptic vibrate when hand is 90% squeezed.
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            ApplicationEvent &applicationEvent = m_applicationEvent[hand];
            applicationEvent.controllerEventBit = 0x00;
            // 获取手部pinch状态
            XrActionStateGetInfo getPinchInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.handPinchAction, m_input.handSubactionPath[hand]};
            XrActionStateBoolean pinchValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getPinchInfo, &pinchValue));
            if (pinchValue.isActive == XR_TRUE) {
                if (pinchValue.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_trigger;
                    if (pinchValue.currentState == XR_TRUE) {
                        Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App:  hand pinch pressed !!!!!!!!!!!!!!!!!!!!"));
                        applicationEvent.click_trigger = true;
                    } else {
                        applicationEvent.click_trigger = false;
                    }
                }
            }
            // 获取手部grip状态
            XrActionStateGetInfo getGripInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.handGripAction, m_input.handSubactionPath[hand]};
            XrActionStateBoolean gripValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getGripInfo, &gripValue));
            if (gripValue.isActive == XR_TRUE) {
                if(gripValue.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_menu;
                    if(gripValue.currentState == XR_TRUE) {
                        Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App:  hand grip pressed !!!!!!!!!!!!!!!!!!!!"));
                        applicationEvent.click_menu = true;
                    } else{
                        applicationEvent.click_menu = false;
                    }
                }
            }

            m_application->inputEvent(hand,applicationEvent);
        }
        //=====================================检查按键状态，推送给application========================================
        static std::vector<std::pair<XrAction,std::string_view>> KeypadCheckList{{m_input.selectAction,"select"},
                                                                                 {m_input.menuAction,  "menu"},
                                                                                 {m_input.oAction,     "o"},
                                                                                 {m_input.xAction,     "x"},
                                                                                 {m_input.upAction,    "up"},
                                                                                 {m_input.downAction,  "down"},
                                                                                 {m_input.leftAction,  "left"},
                                                                                 {m_input.rightAction, "right"}
        };
        for(const auto &i:KeypadCheckList){
            XrActionStateGetInfo getSelectInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, i.first, XR_NULL_PATH};
            XrActionStateBoolean selectValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getSelectInfo, &selectValue));
            if ((selectValue.changedSinceLastSync == XR_TRUE) || (selectValue.currentState == XR_TRUE)){
                m_application->keypadEvent(std::string(i.second));
//                infof(("========== Pressed: "+std::string(i.second)).c_str());
            }
        }
        //========================================================================================================

        if(false){ //Rokid 自带的按键响应事件
            // 控制器中间确认键
            XrActionStateGetInfo getSelectInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.selectAction, XR_NULL_PATH};
            XrActionStateBoolean selectValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getSelectInfo, &selectValue));
            if ((selectValue.changedSinceLastSync == XR_TRUE) || (selectValue.currentState == XR_TRUE)) {
                Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: The gamepad select key is pressed !!!!!!!!!!!!!!!!!!!!!"));
            }
            // 控制器侧边MENU键
            XrActionStateGetInfo getMenuInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.menuAction, XR_NULL_PATH};
            XrActionStateBoolean menuValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getMenuInfo, &menuValue));
            if ((menuValue.changedSinceLastSync == XR_TRUE) || (menuValue.currentState == XR_TRUE)) {
                Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: The gamepad menuValue key is pressed !!!!!!!!!!!!!!!!!!!!!"));
            }
            // 控制器O键--重置3Dof射线
            XrActionStateGetInfo getOInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.oAction, XR_NULL_PATH};
            XrActionStateBoolean oValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getOInfo, &oValue));
            if ((oValue.changedSinceLastSync == XR_TRUE) || (oValue.currentState == XR_TRUE)) {
                if (pfnXrRecenterPhonePose != nullptr) {
                    Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: pfnXrRecenterPhonePose................"));
                    CHECK_XRCMD(pfnXrRecenterPhonePose());
                }
            }
            // 控制器X键 -- 退出应用
            XrActionStateGetInfo getXInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.xAction, XR_NULL_PATH};
            XrActionStateBoolean xValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getXInfo, &xValue));
            if ((xValue.isActive == XR_TRUE) && (xValue.changedSinceLastSync == XR_FALSE) && (xValue.currentState == XR_TRUE)) {
                if (pfnXrOpenCameraPreview != nullptr) {
                    if(m_application->needExit()){
                        m_application->exit();
                        Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: Byebye................"));
                        ExitAppByKey = true;
//                CHECK_XRCMD(pfnXrOpenCameraPreview(reinterpret_cast<PFN_xrCameraUpdateCallback*>(OnCameraUpdate)));
                    }
                }
            }
            // 控制器上键
            XrActionStateGetInfo getUpInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.upAction, XR_NULL_PATH};
            XrActionStateBoolean upValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getUpInfo, &upValue));
            if ((upValue.isActive == XR_TRUE) && (upValue.changedSinceLastSync == XR_FALSE) && (upValue.currentState == XR_TRUE)) {
                if (pfnXrRecenterHeadTracker != nullptr) {
                    Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: pfnXrRecenterHeadTracker................"));
                    CHECK_XRCMD(pfnXrRecenterHeadTracker());
                }
            }
        }
        // 我们自己手动判断需不需要退出
        if(m_application&&m_application->needExit()){
            m_application->exit();
            Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: Byebye................"));
            ExitAppByKey = true;
//            CHECK_XRCMD(pfnXrOpenCameraPreview(reinterpret_cast<PFN_xrCameraUpdateCallback*>(OnCameraUpdate)));
        }

        // 获取camera位姿
//        uint64_t timeStamp;
//        if (pfnXrGetCameraPhysicsPose != nullptr) {
//            float position[3];
//            float orientation[4];
//            CHECK_XRCMD(pfnXrGetCameraPhysicsPose(&timeStamp, position, orientation));
//            Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-App: OnCameraUpdate Call pfnXrGetCameraPhysicsPose in App, position=(%f, %f, %f) orientation=(%f, %f, %f, %f), timeStamp=%ld", position[0], position[1], position[2], orientation[0], orientation[1], orientation[2], orientation[3], timeStamp));
//        }

        // 获取camera历史位姿
//        if (pfnXrGetHistoryCameraPhysicsPose != nullptr) {
//            float position[3];
//            float orientation[4];
//            CHECK_XRCMD(pfnXrGetHistoryCameraPhysicsPose(timeStamp-20000, position, orientation));
//            Log::Write(Log::Level::Info, Fmt("zhfzhf1234===: OnCameraUpdate Call pfnXrGetHistoryCameraPhysicsPose in App, position=(%f, %f, %f) orientation=(%f, %f, %f, %f)", position[0], position[1], position[2], orientation[0], orientation[1], orientation[2], orientation[3]));
//        }

        // 获取HMD跟踪状态
//        if (pfnGetHeadTrackingStatus != nullptr) {
//            uint32_t state = -1;
//            CHECK_XRCMD(pfnGetHeadTrackingStatus(&state));
//            Log::Write(Log::Level::Info, Fmt("zhfzhf123: app pfnGetHeadTrackingStatus state = %d", state));
//        }
//        else {
//            Log::Write(Log::Level::Info, Fmt("zhfzhf123: app pfnGetHeadTrackingStatus is null"));
//        }

        //********************** 获取相机图像 Get Camera Image ***************************
        static bool CameraPreviewOpened=false;
        if(!CameraPreviewOpened){
            if(pfnXrOpenCameraPreview==nullptr){errorf("Failed to load xrOpenCameraPreview!");}
            if(pfnXrCloseCameraPreview==nullptr){errorf("Failed to load xrCloseCameraPreview!");}
            if(pfnXrOpenCameraPreview&&pfnXrCloseCameraPreview){ // 开启相机预览
                XrResult result = pfnXrOpenCameraPreview(OnCameraUpdate);
                if(result == XR_SUCCESS){CameraPreviewOpened=true;}
                else{errorf("Failed to open camera preview!");}
                if(pfnXrGetPrincipalPoint&&pfnXrGetFocalLength){ //获取相机内参
                    float PrincipalPoint[]={0,0}; float FocalLength[]={0,0};
                    result=pfnXrGetPrincipalPoint(PrincipalPoint);
                    if(result!=XR_SUCCESS) errorf("Failed to Get Principal Point!");
                    result=pfnXrGetFocalLength(FocalLength);
                    if(result!=XR_SUCCESS) errorf("Failed to Get Focal Length!");
                    //Viewer::Global()->set_camera_matrix(FocalLength[0],FocalLength[1],PrincipalPoint[0],PrincipalPoint[1]);
                    double fx=FocalLength[0],fy=FocalLength[1],cx=PrincipalPoint[0],cy=PrincipalPoint[1];
                    //fx=290.4315,fy=290.2296,cx=317.7864,cy=242.0248;
//                    fx=281.60213015, cx=318.69481832, fy=281.37377039,cy=243.6907021;
                    RokidCameraMatrix=(cv::Mat_<double>(3,3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
                }
                if(pfnXrGetDistortion){ //获取相机外参，对于pinhole相机是[k1, k2, k3, p1, p2]；对于fisheye相机是[alpha, k1, k2, k3, k4]
                    float d[]={0,0,0,0,0};
                    result=pfnXrGetDistortion(d);
                    if(result!=XR_SUCCESS) errorf("Failed to Get Distortion!");
                    //Viewer::Global()->set_dist_coeffs(d[0],d[1],d[2],d[3],d[4]);
                    double k1=d[0],k2=d[1],k3=d[2],p1=d[3],p2=d[4]; //k1=-0.21495,k2=0.03745,p1=-0.000124,p2=-0.000435,k3=0.001493;
                    RokidDistCoeffs=(cv::Mat_<double>(1,5) <<k1, k2, p1, p2, k3);
                    RokidDistCoeffs=(cv::Mat_<float>(1, 4) << 0.11946399, 0.06202764, -0.28880297, 0.21420146);
                }
            }
        }
        //*************************************************************
    }


    void RenderFrame() override {
        CHECK(m_session != XR_NULL_HANDLE);
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
        if (frameState.shouldRender == XR_TRUE) {
            if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews, layer)) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
            }
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_options.Parsed.EnvironmentBlendMode;
        frameEndInfo.layerCount = (uint32_t)layers.size();
        frameEndInfo.layers = layers.data();
        CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    bool RenderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews, XrCompositionLayerProjection& layer) {
        XrResult res;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = (uint32_t)m_views.size();
        uint32_t viewCountOutput;
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_options.Parsed.ViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());
        CHECK_XRRESULT(res, "xrLocateViews");
        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 || (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        CHECK(viewCountOutput == viewCapacityInput);
        CHECK(viewCountOutput == m_configViews.size());
        CHECK(viewCountOutput == m_swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        // Hand Aim 手部射线方向
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(m_input.aimSpace[hand], m_appSpace, predictedDisplayTime, &spaceLocation);
            CHECK_XRRESULT(res, "xrLocateSpace");
            if (XR_UNQUALIFIED_SUCCESS(res)) {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
//                    Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-app: handtype=%d, hand aimSpace : pose(%f %f %f)  orientation(%f %f %f %f)", int(hand),
//                                                     spaceLocation.pose.position.x, spaceLocation.pose.position.y, spaceLocation.pose.position.z,
//                                                     spaceLocation.pose.orientation.x, spaceLocation.pose.orientation.y, spaceLocation.pose.orientation.z, spaceLocation.pose.orientation.w));
#ifdef USE_HAND_AIM
                    m_application->setControllerPose(int(hand), spaceLocation.pose);
#endif
                }
            }
        }

        // Controller Aim - 获取控制器射线方向
        {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(m_input.gamepadPoseSpace, m_appSpace, predictedDisplayTime, &spaceLocation);
            CHECK_XRRESULT(res, "xrLocateSpace");
            if (XR_UNQUALIFIED_SUCCESS(res)) {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 ||
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
//                    Log::Write(Log::Level::Info, Fmt("RK-Openxr-hand-app: gamepad pose(%f %f %f)  orientation(%f %f %f %f)",
//                                                     spaceLocation.pose.position.x, spaceLocation.pose.position.y, spaceLocation.pose.position.z,
//                                                     spaceLocation.pose.orientation.x, spaceLocation.pose.orientation.y, spaceLocation.pose.orientation.z, spaceLocation.pose.orientation.w));
#ifndef USE_HAND_AIM
                    m_application->setControllerPose(int(0), spaceLocation.pose);
#endif
                }
            }
        }


        //hand tracking start --- 获取手部骨骼点数据
        XrHandJointLocationEXT jointLocations[Side::COUNT][XR_HAND_JOINT_COUNT_EXT];
        XrHandTrackingAimStateFB* MetaAim[Side::COUNT];
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
            locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
            locations.jointLocations = jointLocations[hand];

            XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
            locateInfo.baseSpace = m_appSpace;
            locateInfo.time = predictedDisplayTime;
            XrResult res = xrLocateHandJointsEXT(m_handTracker[hand], &locateInfo, &locations);
            if (res != XR_SUCCESS) {
                Log::Write(Log::Level::Error, Fmt("m_pfnXrLocateHandJointsEXT res %d", res));
            }
        }
        m_application->setHandJointLocation((XrHandJointLocationEXT*)jointLocations);
        RokidHandPose::instance()->set((XrHandJointLocationEXT*)jointLocations);
        //hand tracking end


        XrSpaceVelocity velocity{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &velocity};
        res = xrLocateSpace(m_ViewSpace, m_appSpace, predictedDisplayTime, &spaceLocation);
        CHECK_XRRESULT(res, "xrLocateSpace");

        XrPosef pose[Side::COUNT];
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            pose[i] = m_views[i].pose;
        }

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const Swapchain viewSwapchain = m_swapchains[i];
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t swapchainImageIndex;
            CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = pose[i];
            projectionLayerViews[i].fov = m_views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width, viewSwapchain.height};

            const XrSwapchainImageBaseHeader* const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

            m_graphicsPlugin->RenderView(m_application, projectionLayerViews[i], swapchainImage, m_colorSwapchainFormat, i);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
        }

        layer.space = m_appSpace;
        layer.viewCount = (uint32_t)projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }

    virtual void ProcessFrame() override
    {
        m_application->processFrame();
    }

    private:
        const Options m_options;
        std::shared_ptr<IPlatformPlugin> m_platformPlugin;
        std::shared_ptr<IGraphicsPlugin> m_graphicsPlugin;
        XrInstance m_instance{XR_NULL_HANDLE};
        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_appSpace{XR_NULL_HANDLE};
        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

        std::vector<XrViewConfigurationView> m_configViews;
        std::vector<Swapchain> m_swapchains;
        std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
        std::vector<XrView> m_views;
        int64_t m_colorSwapchainFormat{-1};

        // Application's current lifecycle state according to the runtime
        XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
        bool m_sessionRunning{false};

        XrEventDataBuffer m_eventDataBuffer;
        InputState m_input;

        XrSpace m_ViewSpace{XR_NULL_HANDLE};

        std::shared_ptr<IApplication> m_application;

        //hand tracking
        PFN_DECLARE(xrCreateHandTrackerEXT);
        PFN_DECLARE(xrDestroyHandTrackerEXT);
        PFN_DECLARE(xrLocateHandJointsEXT);
        XrHandTrackerEXT m_handTracker[Side::COUNT] = {0};

        ApplicationEvent m_applicationEvent[Side::COUNT] = {0};

        bool ExitAppByKey = false;
    };
}  // namespace

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<OpenXrProgram>(options, platformPlugin, graphicsPlugin);
}
