# RenderingGlass 渲染结果导出功能

## 功能简介

为 RenderingGlass 渲染系统添加了**渲染结果导出**功能，可以将实时渲染的画面保存为图像文件。

## 修改内容

### 已修改的文件

1. **`app/src/main/cpp/RenderingGlass/RenderClient.h`**
   - 添加了 `exportRenderResult()` 方法：手动导出单帧渲染结果
   - 添加了 `enableAutoExport()` 方法：启用/禁用自动定期导出
   - 添加了相关的私有成员变量

2. **`app/src/main/cpp/RenderingGlass/RenderClient.cpp`**
   - 实现了 `exportRenderResult()` 方法的功能：
     * 使用 `glReadPixels` 读取帧缓冲区
     * 图像垂直翻转处理（OpenGL坐标系转换）
     * RGBA到BGR颜色格式转换
     * 使用OpenCV的 `cv::imwrite` 保存图像
   - 实现了 `enableAutoExport()` 方法
   - 在 `Update()` 方法中添加了自动导出逻辑

### 新增的文档

1. **`EXPORT_RENDERING_GUIDE.md`** - 详细的功能指南
2. **`USAGE_EXAMPLE.md`** - 快速使用示例
3. **`RENDER_EXPORT_README.md`** - 本文档

## 核心功能

### 1. 手动导出

```cpp
bool exportRenderResult(const std::string& outputPath);
```

**功能**：将当前渲染的帧保存为图像文件

**参数**：
- `outputPath`: 图像文件的完整路径（支持 .png, .jpg 等格式）

**返回值**：
- `true`: 导出成功
- `false`: 导出失败

**使用示例**：
```cpp
Rendering->exportRenderResult("/storage/emulated/0/Download/screenshot.png");
```

### 2. 自动导出

```cpp
void enableAutoExport(bool enable, int intervalFrames = 30);
```

**功能**：启用或禁用自动定期导出功能

**参数**：
- `enable`: true启用，false禁用
- `intervalFrames`: 导出间隔（多少帧导出一次，默认30帧）

**使用示例**：
```cpp
// 启用自动导出，每30帧导出一次
Rendering->enableAutoExport(true, 30);

// 禁用自动导出
Rendering->enableAutoExport(false);
```

## 快速开始

在 `scene_AppVer2.cpp` 的 `initialize()` 方法中添加：

```cpp
virtual bool initialize(const XrInstance instance, const XrSession session){
    _eng = construct_engine();
    
    std::string dataDir = _eng->appData->dataDir;
    Rendering = createModule<RenderClient>("RenderClient");
    
    auto frameData = _eng->frameData;
    Rendering->Init(*_eng->appData.get(), *_eng->sceneData.get(), frameData);
    
    // ★ 添加这一行启用自动导出 ★
    Rendering->enableAutoExport(true, 30);  // 每30帧导出一次
    
    _eng->start();
    
    return true;
}
```

## 技术实现细节

### 导出流程

1. **读取像素数据**：使用 `glReadPixels()` 从当前绑定的帧缓冲区读取RGBA像素数据
2. **创建OpenCV Mat**：将像素数据封装为 `cv::Mat` 对象
3. **垂直翻转**：使用 `cv::flip()` 翻转图像（因为OpenGL坐标系Y轴向上，图像坐标系Y轴向下）
4. **颜色转换**：使用 `cv::cvtColor()` 将RGBA转换为BGR格式（OpenCV标准）
5. **保存文件**：使用 `cv::imwrite()` 保存为图像文件

### 自动导出机制

- 在每次 `Update()` 调用时检查帧计数器
- 当达到设定的间隔帧数时，自动触发导出
- 文件名自动添加毫秒级时间戳，避免覆盖
- 默认保存路径：`/storage/emulated/0/Download/render_[timestamp].png`

### 图像规格

- **分辨率**：1920 x 1080（可在RenderClient.h中修改 width/height）
- **颜色格式**：BGR（OpenCV标准）
- **像素深度**：8位/通道
- **支持格式**：PNG、JPG、BMP等OpenCV支持的格式

## 性能考虑

### 性能影响

- `glReadPixels` 会导致GPU管线停顿，等待渲染完成
- 图像格式转换和文件I/O会占用CPU资源
- 1920x1080 RGBA图像约占用8MB内存

### 性能优化建议

1. **不要每帧导出**：建议间隔至少10-30帧
2. **使用JPG格式**：如果不需要无损质量，JPG比PNG快
3. **降低分辨率**：可以只读取部分区域或降低分辨率
4. **异步保存**：可以考虑在后台线程保存文件（需要额外实现）

## 使用场景

### 调试和测试
- 验证渲染效果
- 对比不同版本的渲染质量
- 记录bug现场

### 内容创作
- 截取精彩瞬间
- 生成宣传素材
- 制作教程截图

### 数据采集
- 生成训练数据集
- 记录测试序列
- 性能基准对比

### 视频制作
- 连续导出多帧
- 使用ffmpeg合成视频

## 故障排查

### 常见问题

**1. 导出的图像是黑色的**
- **原因**：在渲染之前调用了导出函数
- **解决**：确保在 `mPbrPass->render()` 之后调用

**2. 找不到导出的文件**
- **原因**：应用没有存储权限或路径不正确
- **解决**：检查 `AndroidManifest.xml` 中的存储权限，确保目标目录存在

**3. 导出失败，返回false**
- **原因**：可能是OpenGL错误或OpenCV保存失败
- **解决**：查看logcat日志，搜索 "RenderClient" 查看错误信息

**4. 导出速度太慢，影响帧率**
- **原因**：PNG压缩和文件I/O耗时
- **解决**：增大导出间隔帧数，或使用JPG格式

### 调试命令

查看日志：
```bash
adb logcat | grep RenderClient
```

查看导出的文件：
```bash
adb shell ls -lh /storage/emulated/0/Download/
```

拉取文件到电脑：
```bash
adb pull /storage/emulated/0/Download/render_*.png ./
```

清理导出的文件：
```bash
adb shell rm /storage/emulated/0/Download/render_*.png
```

## 高级用法

### 导出特定区域

修改 `glReadPixels` 参数：
```cpp
// 只导出左上角 640x480 区域
glReadPixels(0, height - 480, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
```

### 导出不同格式

只需更改文件扩展名：
```cpp
Rendering->exportRenderResult("/path/to/output.jpg");  // JPEG格式
Rendering->exportRenderResult("/path/to/output.bmp");  // BMP格式
```

### 添加图像处理

在保存前添加处理：
```cpp
// 在 cv::imwrite 之前添加
cv::GaussianBlur(bgrImage, bgrImage, cv::Size(5, 5), 0);  // 模糊
// 或
cv::resize(bgrImage, bgrImage, cv::Size(960, 540));  // 缩小到一半
```

## 依赖项

本功能使用了以下库（项目中已包含）：

- **OpenGL ES**：读取帧缓冲区（glReadPixels）
- **OpenCV**：图像处理和保存（cv::Mat, cv::imwrite等）
- **C++标准库**：文件I/O、时间戳等

无需添加额外依赖。

## 后续扩展建议

1. **异步导出**：在后台线程保存图像，避免阻塞渲染
2. **视频录制**：直接编码为视频流
3. **可配置参数**：通过配置文件设置导出参数
4. **批量导出管理**：自动管理导出文件，删除旧文件
5. **压缩质量控制**：添加JPG质量参数
6. **帧缓冲对象支持**：支持从自定义FBO导出

## 许可和贡献

本功能为项目的一部分，遵循项目原有的许可协议。

---

**创建日期**：2025-11-21  
**版本**：1.0  
**作者**：Cursor AI Assistant
