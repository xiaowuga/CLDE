# 驾驶舱布局设计评估
该项目是中国国家重点研发计划“基于微服务架构的增强/混合现实应用开发引擎研发及应用验证”中的应用验证部分。项目通过AR/MR交互方式进行驾驶舱布局设计评估。

## 开发人员

- 江敬恩(山东大学):项目主体框架与模块集成
- 李子锐(山东大学):渲染模块
- 古亦平(山东大学):渲染模块
- 杨善浩(山东大学):光照估计模块
- 李晶(西安交通大学):交互模块
- 王怡雯(西安交通大学):交互模块
- 袁右文(西安交通大学):交互模块
- 任凯文(中科院计算所):手部关节点识别模块
- 李世威(中科院自动化所):相机重定位模块

## 测试平台
- 操作系统: Windows 11
- 处理器: Intel(R) Core i9-13900K (24 cores, 32 threads)
- 内存: 64GB DDR4
- 显卡: NVIDIA GeForce RTX 4090
- IDE: Android Studio 2024.3.2 Patch 1
- Android SDK: Version 36.1.0
- Android NDK: Version 29.0.14033849 rc4
- AR 眼镜: Rokid Max Pro Enterprise

## 资源文件

将 `appData.appDir` 赋值为文件夹 "AppVer2Data" 的路径后，请将眼镜连接到电脑，并将文件夹 "AppVer2Data" 放置到眼镜主存储的顶层路径中，与文件夹 "Download" 同级。具体路径为 `/storage/emulated/0/`。
您可以在眼镜中的应用商店下载并使用 `CX 文件管理器` 来查看和管理文件。

```
AppVer2Data (appData.appDir)
|-- CameraTracking                  # 存放与相机追踪相关的文件
|-- font                            # 存放字体文件
|-- hand                            # 存放手部相关的文件
    |-- handNode_0.fb
    |-- handNode_1.fb
    |-- ...
|-- handNode                        # 存放手部节点的文件
|-- Image                           # 存放图像文件
|-- Manual                          # 存放手册文件
|-- Models                          # 存放模型文件
|-- shaders                         # 存放着色器文件
|-- textures                        # 存放纹理文件
|-- HDRSwitch                       # 存放光照估计资源文件
|-- Animation.json                  # 动画相关的JSON配置文件(旧)
|-- config.ini                      # 配置文件，存放基本设置
|-- InstanceInfo.json               # 存放实例信息的JSON文件(旧)
|-- InstanceState.json              # 存放实例状态的JSON文件(旧)
|-- InteractionConfig.json          # 存放交互配置的JSON文件(旧)
|-- CockpitAnimationAction.json     # 动画相关的JSON配置文件(新)
|-- CockpitAnimationState.json      # 动画相关的JSON配置文件(新)
|-- log.txt                         # 日志文件，记录系统运行日志
|-- templ_1.json                    # 重定位模板文件1()
|-- marker_1.docx                   # 重定位模板文件1对应的marker打印文件
|-- templ_2.json                    # 重定位模板文件2(目前使用)，用于重定位相机位姿
|-- marker_2.docx                   # 重定位模板文件2对应的marker打印文件
|-- templ_3.json                    # 重定位模板文件3(目前使用)，
|-- marker_3.pdf                   # 重定位模板文件3对应的marker打印文件
```
## 编译步骤

1. 根据上述需求下载资源文件，并将其放入 Rokid 眼镜的主存中。
2. 下载并安装 Android Studio。
3. 使用 Android Studio 打开项目，第三方库会通过cmake自动从github上面拉取，请确保VPN是稳定的。
4. 点击编译运行。

## 关闭部分模块

在`src/main/cpp/app/scene_AppVer2.cpp` 中 `construct_engine`函数中控制模块开启/关闭

| 模块名称 | 代码中的类 (Class) | 状态建议 | 说明 |
| :--- | :--- | :--- | :--- |
| **基础输入** | `ARInputs` | **开启** | 处理传感器、按键等基础输入，系统运行的基础，通常必须保留。 |
| **眼镜SLAM** | `Location` | **开启** | 开启表示使用眼镜的SLAM定位。 |
| **光照估计**  | `HDRSwitch` | **关闭** | 代码中原本已注释，开启表示使用光照估计。 |
| **自动化所SLAM** | `CameraTracking` | **关闭** | 代码中原本已注释，开启表示使用自动化所的SLAM定位 |
| **姿态估计** | `PoseEstimationRokid` | **开启** | 开启表示使用Rokid的关节点识别。 |
| **手势交互** | `GestureUnderstanding` | **开启** | **(交互核心)** 识别用户的手势操作。关闭后将无法通过手势控制应用。 |
| **碰撞检测** | `CollisionDetection` | **开启** | **(交互核心)** 处理虚拟物体与手或环境的物理碰撞。关闭后无物理反馈。 |
| **动画播放** | `AnimationPlayer` | **开启** | 播放 3D 动画。
| **日志上传** | `InteractionLogUpload` | **关闭** | **(日志核心)** 负责将交互日志数据通过 Socket 发送到服务器。关闭后将停止数据上传。 |

### 💡 操作提示

在 C++ 代码中，“关闭”某个模块的操作通常是将对应的 `modules.push_back(...)` 行**注释掉**（在该行最前面加 `//`）。


### 云端服务器连接配置

请在 `src/main/cpp/app/scene_AppVer2.cpp` 文件中，定位到 `SceneAppVer2::initialize` 函数，执行以下修改：

1. **取消注释**：找到代码行 `_eng->connectServer("YOUR SERVER IP", portID);` 并取消注释。
2. **配置参数**：将 `"YOUR SERVER IP"` 和 `portID` 替换为您实际的服务器 IP 地址及端口号。

> **依赖说明**：
> * 以下模块依赖云端服务器连接：**光照估计 (HDRSwitch)**、**自动化所 SLAM (CameraTracking)** 以及 **日志上传 (InteractionLogUpload)**。
> * **注意**：通常情况下，日志上传服务会使用独立的服务器地址，与 HDR 或 SLAM 模块的服务器不同，请根据实际情况配置。

**本仓库不包含云端服务的代码。**

## 谷歌网盘下载连接:
AppVer2Data下载：[AppVer2Data.zip](https://drive.google.com/uc?export=download&id=1vk_Bwio-3JN8-eiqQd9wS8amkkf2D3hy)

如果文件有问题，或者下载存在限制，可以联系我，邮件：<xiaowuga@gmail.com>

## Acknowledgements
这项工作得到了中国国家重点研发计划（2022YFB3303200）——基于微服务架构的增强/混合现实应用开发引擎研发及应用验证项目的支持。
