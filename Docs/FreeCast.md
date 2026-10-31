# 手动按鼠标位置释放技能

普通手动输入不再要求鼠标位置存在存活怪物。左键或空格按鼠标地面位置释放，开启自动索敌时手动输入也优先使用鼠标位置。每次动作记录是否为自动索敌：自动攻击的目标失效仍取消待释放动作，手动施法不因瞄准怪物死亡而取消。

Ready 和 Cleared 阶段继续原生技能模拟，允许手动空放、投射物运动、资源恢复与冷却推进。Ready 不生成怪物、不开始波次、不增加挑战计时；清层时结算弹窗仍暂停输入，点击真实“返回拾取”按钮关闭后可继续施法。准备资源、暂停、失败、死亡、硬直和 UI 输入保护继续生效。Ready 中若因自伤死亡，也进入正常失败状态。

扣费与效果仍由原源动画 Attack Release 标记提交同一 DWRules，保持技能数值、原生资源消耗、冷却、长按节奏与移动打断，不增加即时释放路径。

验证：`Tools/Verify-FreeCast.ps1 -Headless` 与 `Tools/Verify-Hero.ps1` 均使用独立瞬态原生世界，不读写正式玩家存档。前者覆盖零怪物空放、手动覆盖自动索敌、长按三次真实投射物、实际动画扣费、重复 Notify、暂停、自动索敌、结算返回后空放与楼层状态；后者覆盖真实移动、八方向根运动、动作打断、目标死亡后的释放、原生扣费、受击死亡与重试。

本轮结果：`Saved/Verification/FreeCast/receipt.json`，日志为同目录 `native.log` 和 `Saved/Verification/Hero/integration.log`。旧 `NoTargetNoCast` / `TargetLostBeforeRelease` 限制及对应历史记录已退役。
