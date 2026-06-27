# Xinle_Player 添加特效实战指南

本文档记录了一次完整的 Xinle_Player 特效添加过程（以“我的特效”为例），用于后续快速复现。

## 前置条件

- 目标工程：`demos/PLAYER/Xinle_Player/Xinle_Player.vcxproj`。
- 开发环境：Qt 6.9.1 + MSVC 2022 + OpenGL 3.3 Core Profile。
- 先运行 `scripts/setup-deps.bat`，将 FFmpeg 头文件和导入库复制到 `Xinle_Player/include/` 和 `Xinle_Player/lib/`。
- 本文档描述的“不修改 `res/player.qrc`”方式，指 **`.frag` 文件不编译进资源**，而是从文件系统加载。

## 添加一个特效的三步法

### 第一步：编写 .frag 文件

在 `demos/PLAYER/Xinle_Player/res/shaders/` 目录下新建一个 fragment shader 文件，例如 `my_effect.frag`。

文件头固定为 `#version 330 core`，并包含以下标准输入输出：

```glsl
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
```

引擎还会自动注入以下 uniform，**不要在 .frag 中重新定义同名变量**：

| uniform | 类型 | 含义 |
|---------|------|------|
| `image` | `sampler2D` | 输入帧纹理，已绑定在纹理单元 0 |
| `resolution` | `vec2` | 帧宽高（像素） |
| `iteration` | `int` | 多 pass 时的当前 pass 索引，从 0 开始 |
| `mvp` | `mat4` | 顶点变换矩阵 |
| `flipY` | `float` | FBO 纹理采样时是否翻转 Y 轴 |

如果需要在 `effects.json` 中暴露可调参数，必须在 `.frag` 中声明对应的 uniform，例如：

```glsl
uniform float intensity;
```

#### 可直接复制的模板

```glsl
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float intensity;   // 在 effects.json 中声明的参数

void main() {
    vec4 color = texture(image, vTexCoord);
    float i = intensity * 0.01;   // 将 [0,100] 映射到 [0,1]

    // 在这里写特效逻辑。
    // 示例：简单的亮度增强。
    vec3 brightened = color.rgb * (1.0 + i * 0.5);
    FragColor = vec4(brightened, color.a);
}
```

### 第二步：在 effects.json 中注册特效

编辑 `demos/PLAYER/Xinle_Player/res/effects/effects.json`，在 `"effects"` 数组末尾添加一个新对象。

**关键：使用文件系统路径，而不是资源路径。**

```json
{
  "name": "我的特效",
  "frag": "res/shaders/my_effect.frag",
  "iterations": 1,
  "params": [
    {
      "name": "intensity",
      "label": "强度",
      "type": "float",
      "default": 80,
      "min": 0,
      "max": 100
    }
  ]
}
```

字段说明：

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 下拉框中显示的名称。 |
| `frag` | 是 | `.frag` 文件的相对路径。相对于 VS 调试工作目录（默认是 `demos/PLAYER/Xinle_Player/`）。 |
| `iterations` | 否 | 渲染 pass 数，默认 1。多 pass 时可在 shader 中用 `iteration` 区分。 |
| `params` | 否 | 参数数组，每个参数会作为同名 uniform 传入 shader。 |

参数字段：

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 必须与 `.frag` 中的 uniform 名完全一致。 |
| `type` | 是 | 支持 `float`、`int`、`bool`、`vec2`、`vec3`、`vec4`。 |
| `default` | 否 | 默认值。 |
| `min` / `max` | 否 | 仅对第一个 `float` 参数有效，用于滑块范围。 |

**注意**：当前 UI 只有一个滑块，并且只绑定第一个 `float` 参数。`int`、`bool`、`vec2/3/4` 只能使用默认值。

### 第三步：编译运行

1. 在 Visual Studio 中重新编译 `Xinle_Player` 项目。
   - 必须重新编译，因为 `effects.json` 本身通过 `res/player.qrc` 嵌入到了可执行文件中。
   - `.frag` 文件不嵌入，所以后续只改 shader 逻辑时**不需要**重新编译。
2. 运行程序，打开一个视频文件。
3. 在特效下拉框中选择新添加的特效名称。
4. 观察画面变化，拖动滑块测试参数是否生效。

## 路径问题说明

`"frag": "res/shaders/my_effect.frag"` 使用的是**相对路径**，解析依赖于程序运行时的当前工作目录。

- **在 Visual Studio 中调试运行**：默认工作目录是项目目录 `demos/PLAYER/Xinle_Player/`，因此 `res/shaders/my_effect.frag` 会正确指向 `demos/PLAYER/Xinle_Player/res/shaders/my_effect.frag`。
- **直接双击运行生成的 .exe**：如果 .exe 位于 `demos/PLAYER/Xinle_Player/bin/x64/Debug/Xinle_Player.exe`，当前工作目录是 `bin/x64/Debug/`，此时 `res/shaders/my_effect.frag` 会找不到文件。

如果需要在 output 目录直接运行，可将 `res/shaders/` 文件夹复制到 .exe 同级目录，或者保持使用资源路径（`:/Player/shaders/my_effect.frag`）并将文件加入 `res/player.qrc`。

## 本次实战示例：怀旧褐色调

已添加的文件：

- `demos/PLAYER/Xinle_Player/res/shaders/my_effect.frag`
- `demos/PLAYER/Xinle_Player/res/effects/effects.json`（新增一个条目）

未修改的文件：

- `demos/PLAYER/Xinle_Player/res/player.qrc`

预期效果：画面整体偏黄褐色，滑块可调节强度。

## 常见问题

| 现象 | 可能原因 | 解决方法 |
|------|---------|---------|
| 下拉框没有新特效 | `effects.json` JSON 语法错误 | 检查逗号、括号、引号；可用 JSON 校验工具检查。 |
| 选择特效后画面没变化 | `.frag` 编译失败或 uniform 名不匹配 | 查看 VS 输出窗口的 `[Effect]` 日志。 |
| 选择特效后程序崩溃 | `.frag` 文件路径错误，导致 shader 加载失败 | 确认运行时的当前工作目录，或检查路径是否正确。 |
| 滑块没有绑定参数 | 参数类型不是 `float`，或者不是第一个 `float` 参数 | 当前只支持第一个 `float` 参数绑定滑块。 |
| 画面上下翻转 | FBO 纹理坐标方向问题 | 通常不需要在 shader 中手动翻转 `vTexCoord.y`，引擎已通过 `flipY` 处理。 |

## 推荐的工作流程

1. 先复制一份现有简单特效（如 `invert.frag`）作为起点。
2. 改名为新文件，放到 `res/shaders/` 目录。
3. 先把它改成纯色输出（如 `FragColor = vec4(1.0, 0.0, 0.0, 1.0);`），验证通路正确。
4. 逐步替换为真实特效逻辑。
5. 在 `effects.json` 中添加配置条目，使用文件系统路径 `"frag": "res/shaders/xxx.frag"`。
6. 编译运行验证。
