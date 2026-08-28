# Silsph Engine — v0.1.0

纯 C++ 最小 3D 引擎（Win32 + OpenGL 3.3 Core），**零第三方依赖**：
窗口 / WGL 上下文（手写加载器 30 个 GL 函数）/ GLSL shader / VAO-VBO 网格 / 手写 4x4 矩阵。

## 命名与读音

**Silsph （**sales**（/seɪlz/））

## 运行

```
silsph_demo.exe        # 旋转彩色立方体（Qraft 主题色），标题栏实时 FPS，ESC/关闭退出
```

## 结构

```
src/
├── silsph.h      # 引擎头：Vec3/Mat4、Window、Shader、Mesh、GL 函数指针声明
├── silsph.cpp    # 实现：WGL 上下文创建、函数加载、shader 编译、mesh 上传
└── main.cpp      # demo：WinMain + 主循环（渲染旋转立方体）
```

## 构建（w64devkit / MinGW GCC 16）

```
g++ -O2 -std=c++17 -o silsph_demo.exe src\main.cpp src\silsph.cpp -lopengl32 -lgdi32 -luser32
```

## 技术要点（踩坑记录）

- GL 3.3 Core 上下文：`wglCreateContextAttribsARB`，`WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x1`
- GL 1.1 核心函数（glViewport/glClear/glDrawElements）直接从 opengl32.dll 链接，**不要**用 wglGetProcAddress
- 2.0+ 函数（shader/VAO/VBO）在 3.x context current 之后用 wglGetProcAddress 加载
- 像素格式：PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER，RGBA 24+24
- 对比 JQt：C++ 直达 GPU；JQt 目前只有 QPainter 软件渲染（无 OpenGL/Vulkan 绑定）
- 黑屏排查：WGL profile bit=0x1、1.1 函数走 opengl32 fallback、行主序+transpose=0+行向量、perspective 的 m[11]/m[14] 位置（详见 QraftRenderDuel/README.md）

## v0.1.1 优化（2026-08-28）

- **光照系统**：Lambert 漫反射（ambient 0.22 + diffuse 0.78），顶点法线属性（location 2），光方向 uniform
- **背面剔除**：GL_CULL_FACE + 立方体面绕序修正为 CCW
- **轨道相机**：Mat4::lookAt，相机绕 Y 缓慢环绕，观察原点
- **网格地面**：GL_LINES 16x16 网格（silsph::Mesh 支持绘制模式参数）
- **垂直同步**：wglSwapIntervalEXT（可选扩展）
- 实测：光照立方体 + 网格地面，vsync 下 ~1790 FPS

## soft/ — 软件渲染管线（自研，纯 C，零依赖）

目标：OPG(OpenGL) / VK(Vulkan) 的同品（自研图形渲染栈）第一步。

`soft/silsph_soft.dll`：即时模式软渲染器（迷你 OpenGL 风格 API）：
`sp_create/sp_clear/sp_begin/sp_vertex3f/sp_end/sp_perspective/sp_look_at/sp_rotate`。

管线（全部 CPU 自研）：MVP 变换 → 近平面裁剪（Sutherland-Hodgman）→ 图元装配 → 背面剔除（CCW）→ edge-function 光栅化 → 透视校正插值（颜色/深度）→ 深度测试（LESS）→ RGBA8 帧缓冲。

- 支持三角形（SP_TRIANGLES）与线段（SP_LINES，DDA）
- 近平面裁剪：clip 空间 f(p)=z_clip+w>=0（GL 语义；**不能裁剪 w=0 平面**——交点 w≈0 投影 ±1e6 导致 edge function 浮点崩溃，近平面交点 w=near 数值稳定）
- 矩阵行主序存储、列向量约定（注意：与 src/ 引擎的行向量约定相反，透视矩阵 m[11]/m[14] 位置不同，勿混用）
- 构建：`soft/build.ps1`（w64devkit gcc，生成 DLL + 导入库 + demo）
- 验证：`crop_test.exe` 近平面裁剪（穿近平面地面 0→41 万像素）；`demo_soft.exe` 与 GL demo 同场景（Qraft 主题立方体 + Lambert 光照 + 网格 + 轨道相机），输出 frame0-5.bmp
- 对照结果：背景 #10131A、网格 #59668C、各面光照色与 GL shader 公式（0.22+0.78·diff）逐项一致；6 帧旋转动画面色变化正确

性能基准（960x640 单线程 -O2，`perf_soft.exe`）：
- demo 立方体场景：0.46 ms/帧（≈2184 FPS 等效）
- 全屏三角形：7.7 ms/帧，~79 M像素/s（含深度测试+透视校正插值）

画质/帧率控制（`demo_soft.exe` v0.2 实测）：
- `sp_depth_test` / `sp_cull_face`：深度测试与背面剔除开关（默认开）
- 300 随机三角形重度重叠（960x640）：全开 35.6ms vs 剔除关 44.0ms（省 19%）vs 全关 44.8ms
- 帧限速器：目标 60 FPS → 实测 59.5；目标 30 → 实测 29.7
- 小场景（12 三角形）：~0.65 ms/帧（瓶颈在 clear，非光栅化）

硬件信息（`sp_get_sysinfo`，纯 Win32+注册表，零第三方依赖）：
CPU 品牌/主频/核心线程（注册表 + GetLogicalProcessorInformation）、内存（GlobalMemoryStatusEx）、
GPU 名称/显存（显示适配器类注册表）、OS 版本（RtlGetVersion）。
实测（i5-11320H / 8GB / Iris Xe / Win10 19045）全部正确，集成显卡显存标注"共享"。

下一步：x/y/far 视锥裁剪、纹理采样、混合、错误码体系、SIMD/多线程、数学库 Rust 化。

## 跨平台（Mac / Linux）

- **核心管线**（矩阵/光栅化/裁剪/深度/插值/BMP 输出）：纯 C11，三平台一致
- **平台层**（`#ifdef` 隔离）：
  - 计时/睡眠：Windows QPC+Sleep；Linux/macOS `clock_gettime`+`nanosleep`（`sp_now_ms`/`sp_sleep_ms`）
  - 硬件信息 `sp_get_sysinfo`：Windows 注册表+系统 API；Linux `/proc/cpuinfo`+`/proc/meminfo`+`/sys/class/drm`+uname；macOS sysctl（统一内存 SoC 标注 integrated）
- **构建**：Windows `soft/build.ps1`（MinGW gcc）；Linux/macOS `soft/build.sh`（gcc/clang，产物 `libsilsph_soft.so`/`libsilsph_soft.dylib`）
- 状态：Windows 已实测；Linux/macOS 分支已写好待真机验证（`sh soft/build.sh` 后运行 `soft/demo_soft`）

## 仓库

GitHub: https://github.com/DeepSeek-Work-In-SilentStudio/Silsph （主目录 demo：`silsph_demo.exe` 为软渲染版，旧 GL 版备份为 `silsph_demo_gl.exe`）

## 日志

程序输出写入 `silsph.log`（后台/无控制台环境可用）。
