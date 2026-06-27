# 特效扩展指南（demos/Xinle_Player）

## 概述

本文档仅适用于 `demos/Xinle_Player` 这个独立的 Qt6 + OpenGL 播放器演示工程，**不适用于主工程 olive**。

- Xinle_Player 使用 `effects.json` + `.frag` 的极简特效描述方式。
- olive 主工程沿用了 Olive 0.1 的 XML 特效描述（`effects/shaders/*.xml` + `*.frag`），从文件系统加载，**不需要修改 `.qrc` 文件**。

在 Xinle_Player 中，给下拉框添加一个新特效通常只需要改纯文本文件；但如果使用 `"setup"` 字段触发 C++ 初始化（如遮罩），则需要同步修改 `player.cpp`。

一个特效由两部分组成：

| 组件 | 位置 | 格式 |
|------|------|------|
| Fragment Shader | `Xinle_Player/shaders/*.frag` | GLSL `#version 330 core` |
| 特效描述 | `Xinle_Player/effects/effects.json` | JSON |

顶点 shader 固定为 `Xinle_Player/shaders/common.vert`，特效作者只关心 fragment shader。

---

## 扩展方式：资源路径 vs 文件系统路径

Xinle_Player 支持两种特效加载方式，根据你的需求选择：

| 方式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **资源路径**<br>`:/Player/shaders/xxx.frag` | shader 编译进可执行文件，分发时无需额外文件 | 每次改特效都要重新编译 | 内置特效、正式发布 |
| **文件系统路径**<br>`shaders/xxx.frag` | 运行时修改 shader 立即生效，无需重新编译 | 分发时需要带上 shaders/ 目录 | 开发调试、快速迭代 |

内置的反色/模糊/遮罩用的是**资源路径**（已在 `player.qrc` 中注册）。你扩展新特效时，**推荐用文件系统路径**（更灵活），这样可以避免改动 `player.qrc`。

---

## 第一步：编写 .frag 文件

在 `Xinle_Player/shaders/` 下新建文件，格式为 `#version 330 core`。

### 系统注入的 uniform

以下 uniform 由引擎自动设置，**你可以直接用，但不能定义同名变量**：

```glsl
uniform sampler2D image;    // 输入帧纹理，已绑定在纹理单元 0
uniform vec2 resolution;    // 帧宽高（像素），例如 (1920.0, 1080.0)
uniform int  iteration;     // 多 pass 时的当前 pass 索引，从 0 开始
uniform mat4 mvp;           // 顶点变换矩阵
uniform float flipY;        // 是否翻转 Y 轴纹理坐标（FBO 来源纹理时通常为 1.0）
```

顶点 shader 传入的变量：

```glsl
in vec2 vTexCoord;    // 归一化纹理坐标 [0, 1]，经过 flipY 处理
```

### 用户自定义 uniform

你在 `effects.json` 里声明的参数会自动作为同名 uniform 设置到 shader。需要**在 `.frag` 中声明对应的 uniform 变量**，否则 `setUniformValue` 调用会失败。

### 最小示例

```glsl
// shaders/my_effect.frag
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float intensity;   // 来自 effects.json 的参数

void main() {
    vec4 color = texture(image, vTexCoord);

    // 示例：降低饱和度。
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float i = intensity * 0.01;                  // 映射 [0,100] 到 [0,1]
    vec3 desaturated = mix(color.rgb, vec3(gray), i);
    FragColor = vec4(desaturated, color.a);
}
```

### 使用 gl_FragCoord

在某些特效中（如遮罩、模糊），需要知道当前像素在帧中的物理位置，可使用 `gl_FragCoord`：

```glsl
vec2 pixelCoord = gl_FragCoord.xy;              // 像素坐标，原点左下角
vec2 uv = pixelCoord / resolution;              // 归一化 [0,1]
uv.y = 1.0 - uv.y;                              // 翻转为左上角原点
```

注意：`gl_FragCoord.y` 的 0 在左下角，如果你的特效逻辑需要左上角原点，请用 `1.0 - uv.y` 翻转。

### 语法对照表

如果你从 olive 的 legacy GLSL（`#version 120`）迁移特效到 Xinle_Player，按以下对照修改：

| Legacy (#version 120) | Modern (#version 330 core) |
|-----------------------|---------------------------|
| `varying vec2 vTexCoord;` | `in vec2 vTexCoord;` |
| `texture2D(image, coord)` | `texture(image, coord)` |
| `gl_FragColor = ...` | `out vec4 FragColor;` 然后 `FragColor = ...` |
| `gl_FragCoord` | `gl_FragCoord`（相同，但注意 Y 轴方向） |
| `uniform sampler2D myTexture;` | 统一改为 `uniform sampler2D image;` |

---

## 第二步：放置 shader 文件

在 `Xinle_Player/shaders/` 下创建你的 `.frag` 文件。

---

## 第三步：在 effects.json 中声明

编辑 `Xinle_Player/effects/effects.json`，在 `"effects"` 数组末尾添加一项。

### 使用文件系统路径（推荐，无需改 qrc）

```json
{
  "name": "我的特效",
  "frag": "shaders/my_effect.frag",
  "iterations": 1,
  "params": [
    {
      "name": "intensity",
      "label": "强度",
      "type": "float",
      "default": 50,
      "min": 0,
      "max": 100
    }
  ]
}
```

> **注意**：`"frag"` 字段写相对路径（相对于可执行文件）。运行时程序会从文件系统加载，修改 `.frag` 后重新运行即可看到效果，**无需重新编译**。
>
> 但 `effects.json` 本身当前通过 `player.qrc` 嵌入到可执行文件，所以修改 `effects.json` 后仍需要重新编译。

### 使用资源路径（需改 qrc，编译进程序）

如果你想把 shader 编译进可执行文件：

1. 编辑 `Xinle_Player/player.qrc`，加入：
   ```xml
   <file>shaders/my_effect.frag</file>
   ```

2. effects.json 中用资源路径：
   ```json
   {
     "name": "我的特效",
     "frag": ":/Player/shaders/my_effect.frag",
     ...
   }
   ```

3. 重新编译。shader 会嵌入到 .exe 中，分发时无需额外文件。

---

## 第四步：验证（字段说明）

#### 顶层字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | 是 | 下拉框中显示的特效名称（中文）。 |
| `frag` | string | 是 | `.frag` 文件路径。`""` 表示"无特效"。可以是资源路径（`":/Player/shaders/xxx.frag"`）或相对文件系统路径（`"shaders/xxx.frag"`）。 |
| `iterations` | int | 否 | 渲染 pass 数，默认 1。典型用法：盒式模糊=2（先水平再垂直）。 |
| `params` | array | 否 | 参数列表，空数组 `[]` 表示无参数。 |
| `setup` | string | 否 | 特殊初始化标识。需要 C++ 代码配合时填写（见"特殊 setup"章节）。 |

#### 参数字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `name` | string | 是 | 对应 GLSL uniform 变量名，必须与 `.frag` 中的声明一致。 |
| `label` | string | 否 | 参数显示名称，当前未使用（预留）。 |
| `type` | string | 是 | 数据类型，见下表。 |
| `default` | 对应类型 | 否 | 默认值，如果未提供则为 0/false/零向量。 |
| `min` | float | 否 | 滑块最小值（仅 `float` 有效）。 |
| `max` | float | 否 | 滑块最大值（仅 `float` 有效）。 |

#### 支持的参数类型

| type | GLSL uniform 声明 | 默认值示例 | UI 绑定 |
|------|-------------------|-----------|---------|
| `float` | `uniform float x;` | `50.0` | 第一个 `float` 参数绑定到滑块（范围由 min/max 指定） |
| `int` | `uniform int x;` | `5` | 无 UI，默认值直接写入 |
| `bool` | `uniform bool x;` | `true` | 无 UI，默认值直接写入 |
| `vec2` | `uniform vec2 x;` | `[0.5, 0.5]` | 无 UI，默认值直接写入 |
| `vec3` | `uniform vec3 x;` | `[1.0, 0.0, 0.0]` | 无 UI，默认值直接写入 |
| `vec4` | `uniform vec4 x;` | `[0.0, 0.0, 0.0, 1.0]` | 无 UI，默认值直接写入 |

> **当前只有一个滑块**。只有第一个 `float` 参数绑定到滑块；`int`/`bool`/`vec2`/`vec3`/`vec4` 只能用默认值。如果需要更多交互控件，可扩展 `applyEffectParams` 逻辑。

---

## 第五步：运行验证

重新编译运行后，在下拉框中应该看到你添加的特效名称。

### 先测通路

第一次写新特效时，先把 `.frag` 改成纯色输出：

```glsl
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);  // 全红
}
```

编译运行，选择你的特效，如果画面变红：
- 特效加载正确
- FBO 管线工作正常
- uniform 绑定无误

然后把纯色替换为真实特效逻辑。

### 常见问题

| 现象 | 原因 | 检查 |
|------|------|------|
| 下拉框没有我的特效 | `effects.json` 解析失败 | 检查 JSON 语法（逗号、引号、括号） |
| 选特效后画面没变化 | (1) `.frag` 编译失败 (2) uniform 名不匹配 (3) 参数类型写错 | 查看 VS 输出窗口的 `[Effect]` 日志 |
| 画面上下翻转 | 从 FBO 采样时 flipY 方向错 | 检查是否在 shader 里做了 `1.0 - vTexCoord.y`，一般不需要手动翻 |
| 画面缩小偏移 | viewport 问题 | 不要在 shader 里改 `gl_FragCoord` 的坐标，只读 |

---

## 完整示例

### 示例 1：单 pass 带滑块（反色）

`Xinle_Player/shaders/invert.frag`：

```glsl
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float amount;

void main() {
    vec4 c = texture(image, vTexCoord);
    float a = amount * 0.01;
    vec3 col = c.rgb + ((vec3(1.0) - c.rgb - c.rgb) * vec3(a));
    FragColor = vec4(col, c.a);
}
```

`effects.json` 条目：

```json
{
  "name": "反色",
  "frag": ":/Player/shaders/invert.frag",
  "iterations": 1,
  "params": [
    { "name": "amount", "label": "强度", "type": "float",
      "default": 100, "min": 0, "max": 100 }
  ]
}
```

### 示例 2：多 pass 特效（盒式模糊）

`Xinle_Player/shaders/boxblur.frag`（节选）：

```glsl
uniform bool horiz_blur;
uniform bool vert_blur;
uniform float radius;

void main() {
    // 第 0 pass 做水平模糊，第 1 pass 做垂直模糊。
    if (iteration == 0 && horiz_blur) {
        // 水平方向采样...
    } else if (iteration == 1 && vert_blur) {
        // 垂直方向采样...
    }
}
```

`effects.json` 条目：

```json
{
  "name": "盒式模糊",
  "frag": ":/Player/shaders/boxblur.frag",
  "iterations": 2,
  "params": [
    { "name": "radius", "label": "半径", "type": "float",
      "default": 10, "min": 0, "max": 100 },
    { "name": "horiz_blur", "type": "bool", "default": true },
    { "name": "vert_blur", "type": "bool", "default": true }
  ]
}
```

### 示例 3：复杂参数 + 代码初始化（遮罩）

遮罩需要传入多边形顶点数组，JSON 无法直接表达，因此用 `"setup": "mask"` 触发 C++ 代码生成顶点数据。

`effects.json` 条目：

```json
{
  "name": "遮罩",
  "frag": ":/Player/shaders/mask.frag",
  "iterations": 1,
  "params": [
    { "name": "mask_blur", "label": "羽化", "type": "float",
      "default": 10, "min": 0, "max": 100 }
  ],
  "setup": "mask"
}
```

对应的 `setupMaskEffect()` 函数（在 `Xinle_Player/player.cpp` 中）负责设置 `numPoints`、`pointData`、`isHasChromakey` 等 shader 需要的 uniform。

如果你的特效也需要代码初始化：
1. 在 `effects.json` 里放一个 `"setup": "my_setup"`。
2. 在 `player.cpp` 的 `onEffectChanged()` 中的 `setupMaskEffect` 分支旁增加你的逻辑。
3. 如果参数超出 JSON 表达范围（如数组），在 setup 函数里直接 `params["xxx"] = ...`。

---

## 文件清单

给 Xinle_Player 加一个新特效需要编辑/创建的文件，取决于你使用的加载方式：

**文件系统路径（推荐）：**

```
Xinle_Player/
├── shaders/
│   └── my_effect.frag        ← 新建，写 GLSL
└── effects/
    └── effects.json          ← 编辑，加 JSON 条目
```

> 修改 `effects.json` 后需要重新编译，因为它当前通过 `player.qrc` 嵌入到可执行文件。

**资源路径（编译进程序）：**

```
Xinle_Player/
├── shaders/
│   └── my_effect.frag        ← 新建，写 GLSL
├── effects/
│   └── effects.json          ← 编辑，加 JSON 条目
└── player.qrc                ← 编辑，加 <file> 行
```

以上步骤完成后重新编译即可。未使用 `"setup"` 字段时，无需修改任何 `.cpp`/`.h` 文件。

---

## 参考

- 内置 shader 源码：`Xinle_Player/shaders/invert.frag`、`boxblur.frag`、`mask.frag`
- 引擎实现：`Xinle_Player/effect.cpp`（uniform 注入）、`Xinle_Player/glwidget.cpp`（FBO ping-pong 管线）
- 特效配置加载：`Xinle_Player/effectregistry.cpp`
