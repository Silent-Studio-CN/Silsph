# Silsph 路线图（三档）

> (C) SilentStudio — All Rights Reserved. Proprietary license.

## 第一档：编辑器能用的渲染器（最急）

1. ✅ **纹理系统**（2D 采样/过滤；mipmap 待补）——透视校正 UV、双线性/最近邻、repeat/clamp、纹素×光照
2. ✅ **混合/透明**（alpha 混合 src_alpha/1-src_alpha，纹理 alpha；半透明排序由应用层 painter 保证）
3. ✅ **完整视锥裁剪**（6 平面 Sutherland-Hodgman x±w/y±w/z±w，快速路径免裁剪）
4. ✅ **正交投影 + 更多图元**（`sp_ortho`；三角形条带/扇、点、线带；prim_test 全过）
5. ✅ **场景图/变换层级**（`sp_push/pop_matrix` 栈 + `sp_translate/sp_scale`；太阳-地球-月亮 scene_test 质心验证全过）
6. ✅ **资源加载**（OBJ 模型：v/vt/vn/f+四边形拆分；BMP 纹理 24/32bpp；obj_test/bmp_test 全过）——PNG/glTF 待补
7. ✅ **拾取（picking）**（ID 缓冲：`sp_load_id`/`sp_pick_id`/`SP_ID`；深度遮挡语义正确；demo 窗口点击拾取）
8. ✅ **Gizmo/网格/坐标轴/选中描边**（三色坐标轴+锥头 always-on-top；亮黄线框描边；demo 点击选中联动；gizmo_test 全过）
9. ✅ **离屏渲染 + 确定性回归基准**（`sp_save_bmp` 离屏输出；regress 黄金图像比对套件：3 场景差异像素=0，双渲染逐字节自检）
10. ✅ **Python ctypes + Rust FFI 绑定**（pysilsph.py 零依赖绑定；sp_rust_demo.rs LoadLibrary 动态加载；**三语言与 C 黄金图逐字节一致**——跨语言确定性验证）

## 第二档：真游戏引擎渲染器（定位关键）

- 🔶 **PBR 材质**（albedo/metallic/roughness/normal/ao/emissive）+ 灯光（方向/点/聚光）+ **阴影贴图**（方向光阴影已实现：光空间深度捕获+片元比较）
- 天空盒/环境光/IBL（间接光照）
- 后处理：tonemapping、伽马校正、泛光、SSAO、MSAA/FXAA
- 实例化、视锥剔除、遮挡剔除、LOD
- 动画（骨骼蒙皮）
- 粒子、billboard、文本渲染（字体图集）、UI 覆盖层
- 🔶 **渲染器架构**：命令缓冲（`sp_cmd_begin/end` 统一 API：后端无关记录+回放，已实现） / 渲染图（render graph）/ 批次合并

## 第三档：生产级成熟引擎

- 🔶 **统一渲染 API + 多后端**（命令缓冲已落地，VK 后端待做）——软渲染兜底
- 🔶 软渲染性能：**多线程分块光栅化已实现**（tile 并行，串行/并行逐位一致）；SIMD 增量式尝试后回退（浮点累积误差+无收益）
- 数学库完善：Quat/Vec4/矩阵栈 + Rust 化（对齐 Rust ECS）
- 渲染线程模型：主线程+渲染线程分离、固定时间步+插值、帧节流
- 调试/校验层：错误码体系、日志、着色器编译校验、GPU 调试标记
- 确定性回归测试：黄金图像比对（"每帧可复现"基础已具备，做成正式测试套件）
- 三平台真机验证（Linux/macOS 已写未跑）

## 建议开工顺序（第一档内部）

纹理 → 混合 → 视锥裁剪 → 正交/图元 → 场景图/矩阵栈 → 资源加载 → 拾取 → Gizmo → 离屏渲染 → 绑定
