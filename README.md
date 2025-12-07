# 应用验证2
该项目是中国国家重点研发计划“基于微服务架构的增强/混合现实应用开发引擎研发及应用验证”中的应用验证部分。项目通过AR/MR交互方式，辅助飞机驾驶员学习如何操控大型飞机。

## 开发人员

- 江敬恩(山东大学):项目主体框架与模块集成
- 李子锐(山东大学):渲染模块
- 古亦平(山东大学):渲染模块
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
|-- templ_2.json                    # 重定位模板文件2(目前使用)
|-- marker_2.docx                   # 重定位模板文件2对应的marker打印文件
|-- templ_3.json                    # 重定位模板文件3(目前使用)
|-- marker_3.pdf                   # 重定位模板文件3对应的marker打印文件
```
## 编译步骤

1. 根据上述需求下载资源文件，并将其放入 Rokid 眼镜的主存中。
2. 下载并安装 Android Studio。
3. 使用 Android Studio 打开项目，第三方库会通过cmake自动从github上面拉取，请确保VPN是稳定的。
4. 点击编译运行。


## 谷歌网盘下载连接:
AppVer2Data下载：[AppVer2Data.zip](https://drive.google.com/uc?export=download&id=1vk_Bwio-3JN8-eiqQd9wS8amkkf2D3hy)

如果文件有问题，或者下载存在限制，可以联系我，邮件：<xiaowuga@gmail.com>

## Acknowledgements
这项工作得到了中国国家重点研发计划（2022YFB3303200）——基于微服务架构的增强/混合现实应用开发引擎研发及应用验证项目的支持。
