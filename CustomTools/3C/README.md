# Opus 3C 实验工程

运行 `Start-Lab.cmd` 即可观看 **100 个人形单位、3 阵营（34/33/33）** 自主混战，默认使用稳定控制。双击 `ThreeCLab.uproject` 打开已保存的 100 人场景，点击 Play 开始。

工程包含 Opus 当前 15 种人形怪物的原模型和 91 个映射动作，305 个依赖文件按原路径迁入并逐文件校验。所有运动、战斗展示、相机与诊断逻辑均在 `Script/*.as`。训练伤害用于触发攻击、受击、死亡及复活，不调用或修改 Opus 规则引擎、玩法数值与存档。

100 个单位均已配装：95 个外部武器/盾牌部件，19 个单位沿用网格内自带的弓、弩或法书。装备是外观挂载，不参与属性、伤害和碰撞计算；编辑器保存场景与运行时均可见。配装和握持校准见 `Config/equipment.json`、`Script/Equipment.as` 与 `Docs/怪物装备.md`。

## 操作

| 操作 | 功能 |
|---|---|
| V | 总览 / 跟随 / 自由相机 |
| Tab 或点击单位 | 选择观察对象 |
| C | 稳定控制 / 原始问题复现，两种模式切换 |
| P | 暂停或继续 |
| R | 重置 100 个单位并退出冻结姿态 |
| 自由相机 WASD、Q/E、右键拖动 | 移动、升降、观察 |

`Start-Lab.ps1 -Mode controlled` 启动默认的稳定控制；`-Mode opus` 仅用于复现原始控制问题；`-CameraTour` 自动巡看模型；`-Editor` 打开编辑器。

稳定控制保持当前交战目标，锁定攻击对象和动作期间的朝向，合并连续受击的表现请求，并使用接近距离与移动速度缓冲，避免动作反复重启和左右抽动。伤害照常结算；动画与动作计时使用相同的真实帧时长。

动作之间通过 UE Montage Slot 做 0.22 秒姿态混合，动作末帧保持到下一动作接管。追击直接衔接跑动，短暂连击间隙保留恢复姿态，避免播完后硬切 idle。详见 `Docs/姿态过渡.md`。

走路与跑步分别标定步速，并用实际位移同步播放倍率；停住时冻结步态，避让时跟随实际行进方向。默认开启，结果和源片段限制见 `Docs/步态同步.md`。

## 复现异常

以下命令控制当前实验窗口，指令通过本工程 `Saved/control.txt` 传给 AS。启动时忽略旧指令。

```powershell
.\Tools\Control.ps1 -Action pose -Model swordsman -Clip death -Phase 0.875 -Angle 1.97
.\Tools\Control.ps1 -Action pose -Model axeman -Clip death -Phase 0.875 -Angle 0.4
.\Tools\Control.ps1 -Action pose -Model cloth_grunt -Clip idle -Phase 0
.\Tools\Control.ps1 -Action mode -Mode controlled
.\Tools\Control.ps1 -Action tour
.\Tools\Control.ps1 -Action resume
```

冻结姿态使用原片段的归一化时间；相机同时考虑身体包围盒和头部位置。它保留 100 个单位，暂停并隐藏未观察的 99 个。

## 验证与证据

- `Tools/Verify-Lab.ps1`：两种模式各运行 40 秒，检查 100 个单位、阵营数、15 种模型和动作覆盖，再生成 `Docs/analysis.json`。
- `Tools/Verify-Stability.ps1`：原始 60 FPS、稳定 60 FPS 和稳定 15 FPS 三次实跑，核验受击重启、动作中断、攻击中换目标、转向和攻击滑移，结果见 `Docs/stability-analysis.json`。
- `Tools/Verify-PoseBlend.ps1`：15 种模型、30 个对照用例，检查实际骨骼中间姿态及过渡连续性，结果见 `Docs/pose-blend-analysis.json`。
- `Tools/Verify-Gaits.ps1`：14 种落地步态模型、25 种运动条件、50 个前后对照，比较世界坐标中的脚部水平运动，结果见 `Docs/gait-analysis.json`。
- `Tools/Verify-Blocks.ps1`：墙体与拥挤胶囊阻挡、障碍移开后的恢复，60/15 FPS 共 24 个原控制/受阻等待对照，结果见 `Docs/block-analysis.json`。
- `Tools/Verify-Equipment.ps1`：15 种模型、75 个动作条件，检查原生骨骼动画求值后的握持误差、挂载关系及禁用碰撞，结果见 `Docs/equipment-analysis.json`。
- `Tools/verify_map.py`：重新加载保存的地图，核验 100 个唯一编号、34/33/33 阵营、15 种模型及模型引用，结果见 `Docs/map-reload.json`。
- `Docs/形态分析.md`：结论、证据和适用边界。
- `Docs/抽搐修复.md`：连续受击重启、攻击中换目标、左右抽动与低帧率时序修复及对照结果。
- `Docs/受阻稳定.md`：贴近阻挡物时走停反复切换、碰撞微动引起的朝向变化，以及停稳与放行机制。
- `Docs/source-audit.json`、`pose-audit.json`、`compression-audit.json`：资源、原始姿态和压缩姿态检查。
- `Docs/Recordings/battle-100/`：90 秒独立窗口录像及完整解码验证；实录约 19.42 FPS。
- `Saved/Diagnostics/*.csv`：每 0.1 秒采样，记录动作、位置、速度、转向、事件和骨骼信息。交互运行默认记录前 120 秒；验证运行记录全程。

相机、灯光和训练碰撞属于实验场配置。对照模式同时更改移动约束、缓转向和碰撞/RVO，结果用于定位控制因素，不代表单一修改在正式爬塔中的收益。
