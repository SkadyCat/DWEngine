# 技能图标圆角裁剪 · 2026-10-25

技能槽底板是圆角，但 `hud-ability-flame` 图片节点原来为 `clip:false, radius:0`。铺满槽位的毒瓶 SVG 中，光效和底色仍能绘制到矩形四角，因此问题在 UICreator 画布中就已存在。导出只是保留了这个结果。

修复只在图片节点设置 `clip:true, radius:11.5`。不裁剪技能槽父级，避免截掉独立的选中光晕。运行时切换 tile / glyph 时同步切换裁剪参数。Opus 动态图片绑定读取同一份来源属性，使用原生 RoundedBox 纹理 Brush；换图、缩放和取消裁剪都更新 Brush。

源工程为 `hud-darksteel-20261024` revision 22，只有共享技能图标的 `clip`、`radius` 两个来源字段变化。旧字段分别是 false、0。SVG、动效、音效和布局均保持一致。

- 浏览器：`examples/skill-icon-clip-regression.html`，6 项通过，覆盖四个槽位、glyph 切换和原方角像素。边角原 alpha 255，修复后只留下 alpha 29 的独立光晕。
- PNG：全部 2798 个缓存尺寸符合协议；技能图 PNG 四角 alpha 均为 0，中心为 255。
- UE：导入后用独立进程重新加载，2665 节点、43 状态、层级校验通过。
- 导入包：`exports/wbp-8662510d-a366-488d-9881-e28753c77337/bundle.json`。
- sourceHash：`a51b2bd2730a1f659af26c957218123ca8d642ef8432598f08781778e20dda4f`。
- 部署：Opus 和本工具内 SvgWbpLab 各更新 2 个画布包、补入 5 个新缓存。其余 2793 个缓存复用，未删除旧资源。备份位于各工程 `Saved/UIRepairBackups/skill-clip-20261025-131052/`。

运行时验证脚本：Opus 根目录 `Tools/Verify-SkillIconClip.ps1`；加 `-Render` 检查真实游戏截图。回归测试从保存的 DA 读取图标参数，再验证换图、半倍布局缩放、取消裁剪和空槽。

原生编译通过；`DungeonWorldOpus.UI.DynamicImageClip` 自动化测试通过。1600×900 实际游戏截图已经检查，技能图的圆角显示正常。验收记录为 Opus 根目录 `Docs/SkillIconClip.json`，截图为 `Docs/SkillIconClipBrowser.png`、`Docs/SkillIconClipRuntime.png`。
