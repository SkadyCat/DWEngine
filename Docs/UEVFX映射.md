# UE 特效映射

主工程中的 `/Game/WebVFX/DA_UEVFXMap` 是可编辑的 DataAsset。`Effects` 是 `Map<Name, DWUEVFXEntry>`：每项指定 Niagara 系统、相对缩放/旋转/偏移、是否跟随现有载体，以及单次表现的最长播放时间。可选 `UserFloats`、`UserColors`、`UserBools` 使用完整的 `User.*` 参数名，在实例激活前设置；没有填写时使用资源默认值。Niagara 组件池复用时由引擎清除上一个实例的参数覆盖。

当前有九项：

| Key | 系统 | 生命周期 |
| --- | --- | --- |
| `ice_lance.flight` | `/Game/IceSkill_VFX/Niagara/Portal_Spear/NS_IceSpear_Projectile` | 跟随原规则冰锥，载体消失即回收 |
| `ice_lance.impact` | `/Game/IceSkill_VFX/Niagara/Projectile_Small/NS_Ice_Hit_Small` | 一次命中只播放一次，完成后回收，最长 1 秒 |
| `whirlwind` | `/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind` | 跟随已有旋风投射物，载体消失即回收 |
| `whirlwind@0` … `whirlwind@5` | 同上，六组独立的颜色参数 | 对应火、冰、雷、物理、毒、虚空地面染色 |

冰锥两项只覆盖霜冻元素表现；旋风通过现有的基础预制体 ID 和六个元素变体接入。其他类型保持当前 WebVFX 默认渲染。命中只是一段表现，不会新增范围伤害或改变碰撞、伤害、穿透等规则。

2026-10-25：从独立特效仓库重新筛选了 8 个冰系 hit 候选。原 `NS_Ice_Explosion_Spear` 的地面爆炸改为 `NS_Ice_Hit_Small` 短促碎冰，缩放 0.5，在实际命中位置播放，移除旧爆炸专用的 Z=-100 偏移。库中原始 Niagara 资产不做修改。所选资源的原生注册表依赖闭包为 14 个资产，其中 13 个已存在，仅新增一个 Niagara 系统；复制文件的 SHA-256 见 `Docs/IceHitMigration.json`。保留飞行配置、其他映射项和实例预算。

`DWRuleWebVFXAdapter` 为已有载体和瞬时表现提供稳定 ID；`DWWebVFXComponent::Draw` 先尝试 Map，未配置、引用为空/不可用、尚未准备好、没有稳定来源或超过预算时，继续原默认绘制。传入稳定来源 ID 时，通用预制体 ID 也可作为 Map Key；冰锥使用上述明确的上下文 Key。

初始化一次解析 Map 并加载引用，Draw 不做资产 IO。Niagara 实例使用 UE 的 ManualRelease 组件池；飞行更新已有实例，命中事件使用完成标记防止每帧重播。当前总预算为 128 个同时存活的 Niagara 系统，可在 DataAsset 修改。

Map 查询是小型哈希表查找；Niagara 自身仍有模拟、渲染、粒子和透明叠加成本。缓存/组件池减少查找、加载和创建成本，不能把特效播放成本降为零。

诊断启动参数 `-DWOVFXDefault` 跳过覆盖表，用于对照原默认表现。`-DWOVFXVerify` 记录首次实例激活、位移更新和回收结果到 `Saved/Verification/UEVFX/runtime.json`，不影响普通启动。

资源由独立 `CustomTools/特效` 工程迁入：2 个 Niagara 系统及依赖共 30 个资产，约 19 MB，原路径和 SHA-256 见 `Docs/IceVFXMigration.json`。没有迁移示例角色、展示关卡、玩法蓝图或 UI。

## 风特效资源接入

来源为 `G:/Baiduyun/Tornado and Wind VFX with SFX 5.2 - 5.7.rar`。先解包到 `Saved/AssetImports/TornadoWind`，通过 `Tools/VFX/stage_wind_library.py` 将 266 个资源文件放进独立特效库，排除 DEMO。`audit_wind_library.py` 使用 UE 资产注册表检查依赖；游戏工程只复制选中系统的 33 个依赖资产（约 17.2 MiB），保留原包路径，SHA-256 记录在 `Docs/WindVFXMigration.json`。原 Niagara、材质、网格均未修改。

`Tools/VFX/map_wind.py` 写入七项旋风覆盖，保留冰锥和总实例预算。映射仅决定表现，原有旋风投射物的施放、多重、镜像复制、速度、碰撞、伤害、寿命及地面染色继续由已有规则提供。相同资源的七个变体共享资产加载，颜色按实例设置。

批量旋风关闭每实例的动态灯光和音效，启用资源提供的优化参数。总上限仍为 128 个 Niagara 系统，超出预算继续基础渲染。首次使用需要 UE 编译 Niagara 和材质；系统未就绪时仍使用基础渲染，编译完成后自动使用映射。

验证：3 项原生映射/回退检查通过，33 个迁入文件 SHA-256 一致。两次独立 `-DWOTransient -DWOHeroArena -DWOUEVFXTest -DWOPreset=whirlwind_return -Deterministic -FPS=60` 实战对照（基础版增加 `-DWOVFXDefault`）都成功施放 5 次，固定靶生命均为 1000 → 706.400024。高级版生成 5 个 Niagara 实例、更新位置 1258 次，5 个全部回收，清理后存活 0。七种配色已从保存的映射读取并渲染检查。结果见 `Docs/WindVFXVerification.json`，实战图为 `Saved/Verification/WindVFX/whirlwind-target.png`；运行 `python Tools/VFX/verify_wind.py` 可复核资产、日志与报告。该检查不是满载帧率基准。

## 初次接入验证（原爆炸资源）

- 原生 Map/默认回退检查通过。当前机器上 100,000 次缓存未命中查询为 0.358 ms；该数字只代表 Map 查询，不包含 Niagara 播放成本。
- 实际游戏世界内用现有冰锥规则命中固定靶：10 次冰锥、10 次命中爆炸，首次生成均确认 `IsActive=true`，持续更新位置 103 次。
- 20 个实例全部归还池中，清理后存活数为 0。
- 启用 Map 与 `-DWOVFXDefault` 两次独立临时会话均施放 10 次，靶子生命从 1000 变为 658，伤害结果一致。
- 迁移后再次校验 30 个资产的 SHA-256，全部一致。结果见 `Docs/UEVFXVerification.json`，实机截图见 `Saved/Verification/UEVFX/ice-target.png`。

固定靶只在 `-DWOUEVFXTest -DWOTransient` 下创建，不读取或写入玩家正式存档，不参与普通开局。

## 小型命中替换验证

使用现有 `-DWOHeroArena` 测试空场和仅诊断生效的 `-DWOUEVFXWarmup=5`：10 次冰锥、10 次 Small 命中，20 个实例全部回收，清理后存活 0。与默认特效的独立临时会话对照，两边目标生命均为 1000 → 658。14 个依赖文件与仓库 SHA-256 全部一致；映射读回确认飞行和其他项未改。汇总见 `Docs/IceHitVerification.json`，选择依据见 `Docs/冰锥命中特效筛选.md`。运行 `python Tools/VFX/verify_ice_hit.py` 可复核这些验收产物和资源文件。
