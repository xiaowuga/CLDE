# RenderingGlass 渲染结果导出指南

## 概述

已为 RenderingGlass 添加了渲染结果导出功能，可以将渲染的画面保存为图像文件。

## 新增功能

### 1. 手动导出单帧

可以在任何时候调用 `exportRenderResult()` 方法导出当前渲染结果。

**方法签名：**
```cpp
bool exportRenderResult(const std::string& outputPath);
```

**参数：**
- `outputPath`: 输出图像的完整路径（支持 .png, .jpg 等格式）

**返回值：**
- `true`: 导出成功
- `false`: 导出失败

**使用示例：**
```cpp
// 在 scene_AppVer2.cpp 中使用
std::shared_ptr<RenderClient> Rendering = ...;

// 导出单帧到指定路径
bool success = Rendering->exportRenderResult("/storage/emulated/0/Download/screenshot.png");
if (success) {
    // 导出成功
}
```

### 2. 自动定期导出

可以启用自动导出功能，每隔指定帧数自动导出一次渲染结果。

**方法签名：**
```cpp
void enableAutoExport(bool enable, int intervalFrames = 30);
```

**参数：**
- `enable`: 是否启用自动导出
- `intervalFrames`: 导出间隔帧数（默认30帧）

**使用示例：**
```cpp
// 在初始化后启用自动导出，每30帧导出一次
Rendering->enableAutoExport(true, 30);

// 如果FPS是30，相当于每秒导出一张图片
// 如果FPS是60，相当于每0.5秒导出一张图片

// 禁用自动导出
Rendering->enableAutoExport(false);
```

## 完整使用示例

### 示例1：在场景初始化时启用自动导出

在 `scene_AppVer2.cpp` 的 `initialize()` 方法中：

```cpp
virtual bool initialize(const XrInstance instance, const XrSession session) {
    _eng = construct_engine();
    
    std::string dataDir = _eng->appData->dataDir;
    Rendering = createModule<RenderClient>("RenderClient");
    
    auto frameData = _eng->frameData;
    Rendering->Init(*_eng->appData.get(), *_eng->sceneData.get(), frameData);
    
    // 启用自动导出，每60帧导出一次
    Rendering->enableAutoExport(true, 60);
    
    _eng->start();
    
    return true;
}
```

### 示例2：通过按键触发手动导出

如果你的应用有按键输入处理，可以添加截图功能：

```cpp
// 在某个按键处理函数中
void onKeyPress(KeyCode key) {
    if (key == KEY_SCREENSHOT) {
        // 生成带时间戳的文件名
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        std::string filename = "/storage/emulated/0/Download/screenshot_" + 
                             std::to_string(timestamp) + ".png";
        
        if (Rendering->exportRenderResult(filename)) {
            Log::Info("截图保存成功: " + filename);
        }
    }
}
```

### 示例3：导出特定帧

如果你想在特定条件下导出（比如检测到某个事件）：

```cpp
// 在 Update 方法中
int RenderClient::Update(AppData& appData, SceneData& sceneData, FrameDataPtr frameDataPtr) {
    // ... 正常的渲染代码 ...
    
    // 检测特定条件
    if (sceneData.someSpecialEvent) {
        std::string filename = "/storage/emulated/0/Download/event_capture.png";
        exportRenderResult(filename);
        sceneData.someSpecialEvent = false; // 重置标志
    }
    
    return STATE_OK;
}
```

## 技术细节

### 导出的图像格式
- 默认分辨率：1920x1080
- 支持的格式：PNG（推荐）、JPG、BMP等OpenCV支持的格式
- 颜色空间：BGR（OpenCV标准格式）

### 性能考虑
- `glReadPixels` 会阻塞GPU管线，可能影响帧率
- 建议不要每帧都导出，使用自动导出时建议间隔至少10-30帧
- 保存大图像可能需要较长时间，建议在非关键路径上使用

### 存储位置
- 默认导出到 `/storage/emulated/0/Download/` 目录
- 确保应用有存储权限
- 自动导出会生成带时间戳的文件名，避免覆盖

## 故障排查

### 导出失败的常见原因

1. **权限问题**
   - 确保应用有存储权限（WRITE_EXTERNAL_STORAGE）
   - 检查目标目录是否存在且可写

2. **OpenGL错误**
   - 确保在渲染完成后调用 `exportRenderResult()`
   - 检查日志中的OpenGL错误码

3. **内存不足**
   - 1920x1080的RGBA图像需要约8MB内存
   - 确保设备有足够的可用内存

### 调试日志

导出过程会输出以下日志（使用 `LOGI`）：
- "开始导出渲染结果到: [路径]"
- "渲染结果导出成功: [路径]" 或 "保存图像失败: [路径]"
- "glReadPixels错误: [错误码]"
- "导出渲染结果时发生异常: [异常信息]"

使用 `adb logcat` 查看这些日志：
```bash
adb logcat | grep RenderClient
```

## 高级用法

### 自定义分辨率导出

如果需要导出不同分辨率，可以修改 `RenderClient.h` 中的 `width` 和 `height` 值，或在初始化时设置。

### 导出特定区域

可以修改 `glReadPixels` 的参数来只读取部分区域：

```cpp
// 只读取左上角 640x480 的区域
glReadPixels(0, height - 480, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
```

### 批量导出

可以在一个循环中连续导出多帧：

```cpp
for (int i = 0; i < 100; i++) {
    // 渲染第 i 帧
    // ...
    
    // 导出
    std::string filename = "/storage/emulated/0/Download/frame_" + 
                         std::to_string(i) + ".png";
    Rendering->exportRenderResult(filename);
}
```

## 注意事项

1. 导出功能会在渲染完成后立即读取帧缓冲区
2. 图像会自动垂直翻转（因为OpenGL和图像坐标系Y轴相反）
3. 颜色通道会从RGBA转换为BGR（OpenCV标准格式）
4. 自动导出的文件名包含毫秒级时间戳，确保唯一性
5. 建议在实际设备而非模拟器上测试，以获得最佳性能

## 文件修改清单

本功能修改了以下文件：

1. `app/src/main/cpp/RenderingGlass/RenderClient.h` - 添加了导出方法声明
2. `app/src/main/cpp/RenderingGlass/RenderClient.cpp` - 实现了导出功能

无需修改其他文件即可使用此功能。
