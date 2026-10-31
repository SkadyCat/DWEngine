# 九宫格接缝修复 · 2026-10-25

故障发生在 `src/assets/svg.js` 的 `assetGraphic()`：九个独立 SVG 视口分别裁切，93×23 按钮的上下 12 单位边角缩到 11.5 像素。拼片边界的半透明像素按 source-over 合成，形成低于原色透明度的横竖接缝，并被 PNG 导出保留。

修复将共享切分边界对齐到最终输出像素网格，使用控件的全局位置与累计缩放换算。不扩大拼片、不重复叠色，外边界及圆角仍保留抗锯齿。编辑器、运行时、离线 HTML 四态和原生 WBP 导出使用相同逻辑。

验证：

- `npm run check` 通过；`npm test` 54 项通过。
- `examples/verify.html` 9 项浏览器验证通过。
- `examples/nine-slice-regression.html` 40 项 SVG → PNG 像素验证通过，覆盖 93×23、180×30、45×11、93×23.5、小数位置、0.72/1/1.25/2 倍缩放、不透明及半透明填充。
- 完整 HUD 导入包：`exports/wbp-73960404-811d-4997-8e42-d5cb253f26ad/bundle.json`，2665 节点、179 SVG、2798 缓存、43 状态。源工程定义保持不变。
- UE 在独立进程重新加载保存的 WBP、核对层级并渲染；`native/reload-receipt.json` 为成功。
- UE 实际渲染的地图页签内部原拼缝采样点与正常填充 RGBA 完全相同，见导入包中的 `native/map-tab-pixel-check.json`。

对照图：`exports/wbp-nine-slice-fe900c5a-7002-489e-a686-6a1d9a0fa281/reference/comparison.png`。

部署前资产备份位于各 UE 工程的 `Saved/UIRepairBackups/nine-slice-20261025-125410/`，逐文件复制校验记录在新导入包的 `deployment-receipt.json`。
