# Xinle_Player - 基于 Qt6 + FFmpeg + OpenGL 的本地视频播放器

## 概述

本项目是一个独立的本地视频播放器演示工程，参考 olive（基于 Olive 0.1 深度定制的视频编辑器）的音视频处理与 GLSL 特效管线思路，针对 **Qt 6.9.1 + MSVC 2022 + OpenGL 3.3 Core Profile** 环境重新实现。

与 olive 的主要差异：

- olive 使用 Qt 5.12.1 + MSVC 2015 + **传统 OpenGL**（`glBegin/glEnd`、固定管线、内置 shader 变量）。
- Xinle_Player 使用 Qt 6.9.1 + MSVC 2022 + **现代 OpenGL Core Profile**（VAO/VBO、`QOpenGLShaderProgram`、显式 vertex attribute）。
- 音频输出从 olive 的 `QAudioOutput`（Qt 5）改为 `QAudioSink`（Qt 6）。
- GLSL shader 语法从 `#version 120` 风格迁移到 `#version 330 core`。

## 项目结构

```
demos/Xinle_Player/Xinle_Player/
├── main.cpp              # 程序入口
├── player.h/.cpp         # 主窗口：UI 布局、播放控制、特效选择
├── glwidget.h/.cpp       # OpenGL 视频显示控件，负责纹理上传与特效渲染
├── videodecoder.h/.cpp   # FFmpeg 7.x 音视频解码器
├── effect.h/.cpp         # 通用 GLSL 特效封装
├── effectregistry.h/.cpp # 从 effects.json 动态加载特效配置
├── effects/
│   └── effects.json      # 特效列表与参数定义
├── shaders/              # GLSL shader 文件
│   ├── common.vert       # 统一顶点 shader
│   ├── pass_through.frag # 直接采样
│   ├── invert.frag       # 反色特效
│   ├── boxblur.frag      # 盒式模糊（2 pass）
│   └── mask.frag         # 多边形遮罩
└── Xinle_Player.vcxproj        # Visual Studio 工程文件
```

## 架构流程

```
┌─────────────────┐     文件路径      ┌──────────────────┐
│   Player 窗口   │ ────────────────▶ │  VideoDecoder    │
│                 │                   │  (QThread)       │
└─────────────────┘                   └────────┬─────────┘
       │                                        │
       │  按帧率取帧                            │ 解码视频包
       │                                        ▼
       │                               ┌──────────────────┐
       │                               │   RGBA 视频帧队列 │
       │                               └────────┬─────────┘
       │                                        │
       ▼                                        ▼
┌─────────────────┐     RGBA 帧       ┌──────────────────┐
│    GLWidget     │ ◀──────────────── │  解码音频包       │
│  (QOpenGLWidget)│                   │  S16 立体声 PCM   │
└────────┬────────┘                   └────────┬─────────┘
         │                                      │
         │ 纹理上传                              │ 写入
         ▼                                      ▼
┌─────────────────┐                   ┌──────────────────┐
│   OpenGL 纹理   │                   │    QAudioSink    │
└────────┬────────┘                   │   （声卡输出）   │
         │                            └──────────────────┘
         ▼
┌─────────────────┐
│  FBO ping-pong  │
│  + GLSL 特效    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     屏幕显示     │
└─────────────────┘
```

## 音视频解码

`VideoDecoder` 继承 `QThread`，在独立线程中循环读取 FFmpeg 数据包：

1. 使用 `avformat_open_input` / `avformat_find_stream_info` 打开文件。
2. 通过 `av_find_best_stream` 分别查找视频流和音频流。
3. 视频包：解码 → `sws_scale` 转换为 RGBA → 压入线程安全队列。
4. 音频包：解码 → `swr_convert` 重采样为 S16 立体声 48kHz → 写入 `QAudioSink`。
5. 文件结束时回环播放。

主线程通过 `QTimer` 按视频帧率（`1000 / fps` 毫秒）从队列取帧，交给 `GLWidget` 显示。

### 同步策略

- 音频由声卡缓冲自然驱动播放节奏。
- 视频按固定帧率显示，与音频不完全同步但足够流畅。
- 解码线程全速运行；当视频队列满时丢弃多余视频帧，但**不会阻塞音频解码**。

## 音频播放

使用 Qt 6 的 `QAudioSink`：

```cpp
QAudioFormat format;
format.setSampleRate(48000);
format.setChannelCount(2);
format.setSampleFormat(QAudioFormat::Int16);

QAudioDevice device = QMediaDevices::defaultAudioOutput();
QAudioSink* sink = new QAudioSink(device, format);
sink->setBufferSize(format.bytesForDuration(200000));  // 约 200ms 缓冲
QIODevice* io = sink->start();
io->write(pcmData);
```

为了吸收解码波动，缓冲设置为约 200ms。写入时处理部分写入，避免丢数据。

## OpenGL 渲染

### 现代 Core Profile

由于 OpenGL 3.3 Core 已移除固定管线，所有渲染使用：

- **VAO/VBO**：全屏四边形，包含位置和纹理坐标。
- **显式 vertex attribute**：`aPos`（location=0）、`aTexCoord`（location=1）。
- **`QOpenGLShaderProgram`**：编译/链接/绑定 shader。
- **`QOpenGLFramebufferObject`**：特效 ping-pong 渲染。

### 渲染管线

无特效时：

```
视频纹理 ──▶ pass-through shader ──▶ 屏幕
```

有特效时：

```
视频纹理 ──▶ pass-through ──▶ FBO[0]
FBO[0]   ──▶ effect shader ──▶ FBO[1]
FBO[1]   ──▶ effect shader ──▶ FBO[0]   (多 pass 时继续交换)
...
FBO[n]   ──▶ pass-through ──▶ 屏幕
```

### FBO 翻转处理

`QOpenGLFramebufferObject` 纹理在作为来源渲染时存在固有的上下翻转。统一通过顶点 shader 中的 `flipY` uniform 处理：

| 渲染步骤 | `flipY` | 说明 |
|---------|--------|------|
| 无特效直接渲染视频 | 0 | 视频纹理方向正确 |
| 视频 → FBO[0] | 0 | 拷屏不产生额外翻转 |
| FBO → FBO（特效 pass） | 1 | 抵消 FBO 来源纹理的翻转 |
| FBO → 屏幕 | 1 | 抵消 FBO 来源纹理的翻转 |

## GLSL 特效系统

### 通用特效类 `Effect`

`Effect` 封装了一个 vertex shader + fragment shader 组合，提供：

- `load(vertPath, fragPath)`：加载 shader。
- `setUniform(name, value)`：设置任意 uniform，支持 `float`/`int`/`bool`/`QVector2D/3D/4D`/`QMatrix4x4`/float 数组。
- `setResolution(w, h)` / `setTime(t)` / `setIteration(i)`：设置标准 uniform。

### 编写自定义 .frag

只需提供 fragment shader，顶点 shader 固定使用 `shaders/common.vert`：

```glsl
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;    // 输入帧纹理（已绑定在单元 0）
uniform vec2 resolution;    // 帧宽高
uniform int iteration;      // 多 pass 时的迭代索引

void main() {
    vec4 color = texture(image, vTexCoord);
    // 自定义特效逻辑
    FragColor = color;
}
```

在 C++ 中加载并设置参数：

```cpp
m_glWidget->loadEffect("shaders/my_effect.frag");
m_glWidget->setEffectParam("myUniform", 1.0f);
m_glWidget->setEffectIterations(1);
```

### 示例特效

- **invert.frag**：反色，强度由 `amount` 控制。
- **boxblur.frag**：2 pass 盒式模糊，强度由 `radius` 控制。
- **mask.frag**：多边形遮罩，通过 `pointData[100]` 和 `numPoints` 传入顶点。

## 与 olive 的对应关系

| 功能 | olive | Xinle_Player |
|-----|---------|--------|
| 视频解码 | `rendering/cacher.cpp` | `videodecoder.cpp` |
| 音频输出 | `QAudioOutput` | `QAudioSink` |
| OpenGL 显示 | `ui/viewerwidget.cpp` | `glwidget.cpp` |
| 特效基类 | `effects/effect.cpp` | `effect.cpp` |
| Shader 加载 | `effects/effectloaders.cpp` + XML | 直接加载 `.frag` + 固定 `common.vert` |
| FBO 渲染 | `rendering/renderfunctions.cpp` | `glwidget.cpp` 内 FBO ping-pong |
| 特效 XML 定义 | `effects/shaders/*.xml` | 当前简化为硬编码参数，后续可扩展 XML/JSON 描述 |

## 如何扩展

见 [EFFECTS.md](EFFECTS.md)（特效扩展指南），包含完整的 GLSL 编写规范、effects.json 字段说明、参数类型对照、示例和排错指导。

## 注意事项

- 当前视频/音频同步采用简单策略，适合演示和轻量播放；如需精确同步，可引入基于音频时钟的同步机制。
- 高分屏（DPR > 1）下，viewport 保存/恢复必须发生在 FBO 渲染前后，避免画面缩放/偏移。
- 所有 OpenGL 操作必须在 `GLWidget` 的 context current 状态下执行，因此 FBO 创建放在 `paintGL` 内完成。
