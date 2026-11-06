# PixPhysiccc

C 体素受力与破碎原型。**方块体素用于拟合外观和描述材料连接；断裂后的连通体素组成复合刚体，由 Jolt 模拟飞散、旋转、重力和碰撞。**

场景为最初约定的两个：木板箱子爆炸、笔直体素梁受力断裂。当前交互不包含挖掘或材料擦除；爆炸前后的体素材料量保持一致。

## 打开物理场景

~~~powershell
mingw32-make -j4 viewer   # 只构建
mingw32-make view         # 打开场景
~~~

也可使用 make。程序为 build/pixphysiccc-viewer.exe，默认显示完整箱子。

| 输入 | 功能 |
|---|---|
| 1 / 2 | 箱子 / 承重梁 |
| Space | 箱子爆炸 / 梁末端载荷增加 25 |
| 左键点击物体 | 局部冲击：连接断裂与物理冲量 |
| 右键拖动 / 滚轮 | 环绕观察 / 缩放 |
| + / - | 梁的载荷增减 |
| P | 暂停 / 继续物理模拟 |
| R | 重置当前场景 |
| C / G / H | 连通碎片配色 / 体素边线 / 连接损伤与载荷配色 |
| Esc | 退出 |

体素方块相互贴合，碎片内部不会散成独立运动的格子。新断面随连接断开和碎片运动而露出，所有方块跟随所属刚体的真实位置和旋转绘制。暂停只停止时间推进，便于查看断裂结果。

## 构建依赖

需要 64 位 GCC/G++、GNU Make、Python 3.11 或更新版本。显示使用 raylib 5.5，物理使用 Jolt 5.3.0。源码固定到提交并校验 SHA-256；首次显式构建物理查看器时自动获取，Makefile 直接编译，无需 CMake。

应用、体素图、模拟映射和显示均为 C11；Jolt 及其小型 C 接口适配器为 C++17。Windows 已验证 MinGW-w64 GCC16.2 和原生/MSYS 两种 GNU Make。Linux 构建分支存在，本次未在 Linux 上运行验证。

## 无窗口分析

默认 make、make run、make test、make smoke 仍然运行命令行分析，不会打开窗口。它们只构建 C 的连接图与数据导出，不需要图形或物理依赖。

~~~powershell
mingw32-make -j4
mingw32-make run
.\build\pixphysiccc.exe --scene crate --impact 15,7.5,7.5,3,180
.\build\pixphysiccc.exe --scene beam --load 100 --output build/results/beam
~~~

命令行计算连接损伤与结构分裂，实际刚体轨迹由物理模拟模块计算。可用参数：

| 参数 | 作用 |
|---|---|
| --scene crate 或 beam | 箱子 / 梁 |
| --size N | 箱子边长 / 梁长度，2～27，默认16；梁截面3×3 |
| --impact X,Y,Z,R,POWER | 球形作用区的整数连接损伤，不删除体素 |
| --load N | 梁的总末端载荷，分配到仍受支撑的末端体素 |
| --no-default | 不执行默认演示动作 |
| --output DIR | 结果目录，默认 build/results/场景名 |
| --help | 帮助 |

动作可重复，最多64个，按参数顺序执行。没有显式动作时，箱子施加一次局部冲击，梁施加载荷100。

before.json / after.json 保存稳定体素 ID、材料坐标、连接损伤、载荷和结构分组；before.vox / after.vox 保存原生方块数据；report.json 保存统计。分析文件中的材料坐标不随刚体运动改变。不同实验请指定不同输出目录，以免覆盖同名结果。

## 验证

~~~powershell
mingw32-make test          # 无窗口：核心、导出、参数与命令行场景
mingw32-make test-physics  # 无窗口：Jolt、复合碎片、重力、载荷断裂与材料守恒
mingw32-make test-viewer   # 显式打开窗口：验证实际位移/旋转/落下并保存截图
~~~

图形检查保存 build/viewer_crate.png、viewer_blast.png、viewer_beam.png、viewer_settled.png 和 viewer_report.json。检查物理状态的变化，不能用静态分组或显示偏移代替运动。

## 代码

~~~text
src/voxel.c/h           整数连接图、材料槽位、承载近似与连通块
src/simulation.c/h      体素连通块与复合刚体映射、爆炸与受力断裂
src/rigid.cpp/h         Jolt 的小型 C 接口
src/viewer.c           方块表面、刚体变换、交互与显示
src/main.c, cli.h      无窗口分析入口
src/voxel_export.c/h   方块数据与连接图导出
tests/                无窗口回归测试
~~~

## 当前近似范围

强度、损伤和载荷使用整数游戏单位，位置、速度、旋转使用浮点数；模拟固定步长为1/120秒。梁按剩余截面的重量与力臂估算承载，不做弹性弯曲。爆炸的裂纹区域是游戏用的近似规则，刚体运动与碰撞交由 Jolt。未标定牛顿/帕斯卡，也没有通用连续应力场或真实木材本构模型。

依赖版本与许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。