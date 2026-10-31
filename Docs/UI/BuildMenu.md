# Studio3D BD 快捷使用面板

原问题：UE 右侧 BD 入口指向旧 suite-pages.js 示例页。SvgHUD 的适配器另将流派写成六类、每页三个配装；这既不是当前 Studio3D 的可用 BD 列表，也不是引擎版本问题。

本轮恢复右侧快捷使用面板：300px 宽、可滚动流派列表、流派内全部档位、行内配装详情、已拥有/已装备、应用后保留菜单和已应用标记、返回和 Esc。流派按 Studio3D 的 groupBuildPresets 同源生成，当前 23 流派、27 配装；余烬回响显示全部五档。配装应用仍调用现有 UOpusNative::ApplyPreset，不改变规则、预设数值或原装备保留行为。

布局、按钮四态和文字样式源为 UICreator 项目 opus-studio3d-build-menu；UIAuthoring/BuildMenu.uialpha.json 可编辑。原网页 .b3-card 的 backdrop-filter: blur(6px) 由原生 UBackgroundBlur 接收；强度从现有 CSS 生成。

生成：node Tools/UI/Author-BuildMenu.mjs。该步骤从 Content/Rules/Presets/index.json 和各配装读取展示元数据，生成 BuildMenuCatalogue.as；JS 仅用于离线 UI 制作。UE 运行时为 AS + 原生规则。冻结导出页为 CustomTools/UICreatorAlpha/examples/build-menu-export.html；部署包写入独立 /Game/UICreatorBuilds。

验收：Tools/UI/Verify-BuildMenu.ps1。使用独立瞬态存档，点击实际右栏和行内 UButton，检查23流派/27配装、五档全部可用、展开详情、实际应用I和V档、原物品保留、Esc，以及测试前后正式Profile哈希。

范围说明：本轮替换的是右侧可用 BD 快捷面板。原“构筑原理与词条追求”入口仍保留既有构筑详情页；没有把该详情页宣称为本次网页逐像素重建成果。

最终原生验收：23流派、27预设、五档、详情、原生应用I/V档、装备保留和Esc均通过。背景模糊实际为300×506、强度6，前景绘制在模糊之后；最终图已人工检查。

存档检查：首轮隔离运行前后SHA256相同；最后并行正常游戏时原生套件exit0，但严格脚本因正式档被正常预览更新返回1。保留该限制，未恢复旧存档。详见Saved/Verification/BuildMenu/verification.json。
