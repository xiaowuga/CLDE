# RenderingGlass 导出功能使用示例

## 快速开始

### 方法1：启用自动导出（推荐用于测试）

在 `scene_AppVer2.cpp` 中，找到 `initialize` 方法，在初始化RenderClient之后添加：

```cpp
virtual bool initialize(const XrInstance instance,const XrSession session){
    _eng=construct_engine();
    
    std::string dataDir = _eng->appData->dataDir;
    Rendering = createModule<RenderClient>("RenderClient");
    
    auto frameData = _eng->frameData;
    Rendering->Init(*_eng->appData.get(), *_eng->sceneData.get(), frameData);
    
    // ★★★ 添加这一行，启用自动导出 ★★★
    // 每30帧导出一次渲染结果到 /storage/emulated/0/Download/ 目录
    Rendering->enableAutoExport(true, 30);
    
    _eng->start();
    
    return true;
}
```

### 方法2：手动导出单帧

如果你想在特定时机导出（比如某个事件发生时），可以直接调用：

```cpp
// 在任何可以访问 Rendering 对象的地方
Rendering->exportRenderResult("/storage/emulated/0/Download/my_screenshot.png");
```

例如，在 `renderFrame` 方法中添加条件判断：

```cpp
virtual void renderFrame(const XrPosef &pose,const glm::mat4 &project,
                        const glm::mat4 &view,int32_t eye){
    auto frameData=std::make_shared<FrameData>();
    
    if (_eng) {
        // ... 现有的渲染代码 ...
        
        Rendering->Update(*_eng->appData.get(), *_eng->sceneData.get(), frameDataPtr);
        
        // ★★★ 添加导出逻辑 ★★★
        // 例如：每隔一段时间导出一次
        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter == 100) {  // 第100帧导出
            Rendering->exportRenderResult("/storage/emulated/0/Download/frame_100.png");
            frameCounter = 0;  // 重置计数器
        }
    }
}
```

## 实际应用场景

### 场景1：调试渲染效果

在开发过程中，想要查看某个特定时刻的渲染结果：

```cpp
// 在特定条件下导出
if (sceneData.debugMode) {
    Rendering->exportRenderResult("/storage/emulated/0/Download/debug_render.png");
    sceneData.debugMode = false;
}
```

### 场景2：记录关键帧

在动画播放到关键帧时自动截图：

```cpp
if (animationFrame == keyFrame) {
    std::string filename = "/storage/emulated/0/Download/keyframe_" + 
                          std::to_string(keyFrame) + ".png";
    Rendering->exportRenderResult(filename);
}
```

### 场景3：定时截图生成视频素材

启用自动导出，然后用ffmpeg合成视频：

```cpp
// 启用自动导出，每帧都导出（假设30fps）
Rendering->enableAutoExport(true, 1);

// 运行一段时间后，在命令行使用ffmpeg合成：
// ffmpeg -framerate 30 -i /storage/emulated/0/Download/render_%d.png -c:v libx264 output.mp4
```

## 注意事项

1. **确保有存储权限**：应用需要 `WRITE_EXTERNAL_STORAGE` 权限
2. **路径必须有效**：确保目标目录存在
3. **性能影响**：导出会影响帧率，建议不要每帧都导出
4. **文件格式**：建议使用 `.png` 格式以获得最佳质量

## 验证导出是否成功

1. 查看日志：
```bash
adb logcat | grep RenderClient
```

2. 检查文件是否生成：
```bash
adb shell ls -l /storage/emulated/0/Download/
```

3. 拉取文件到电脑查看：
```bash
adb pull /storage/emulated/0/Download/render_*.png ./
```

## 常见问题

**Q: 导出的图片是黑屏？**
A: 确保在渲染完成后调用导出函数，不要在渲染之前调用。

**Q: 找不到导出的文件？**
A: 检查应用是否有存储权限，以及路径是否正确。

**Q: 导出速度太慢？**
A: PNG格式压缩需要时间，可以改用JPG格式：`"image.jpg"`

**Q: 如何更改导出分辨率？**
A: 在 `RenderClient.h` 中修改 `width` 和 `height` 的值（默认1920x1080）。
