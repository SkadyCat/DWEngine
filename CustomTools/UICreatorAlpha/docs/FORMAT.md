# 工程与存储协议 v1

浏览器中的交换格式是 `schema: uicreator-alpha, version: 1`：

```text
Project
  id, name, viewport
  folders: { folderId: { id, name, kind: svg|prefab|motion|sound, parentId } }
  assets:  { assetId:  { id, name, folderId, source, viewBox, scaleMode, slice } }
  sounds:  { soundId: { id, version, name, folderId, source: WAV data URL,
                       duration, sampleRate, channels, bytes, volume, pitch, cooldown } }
  motions: { motionId: 版本化时间轴与事件绑定 }
  prefabs: { prefabId: { id, name, folderId, width, height, root } }
  root: WidgetNode
  provenance: 导入来源与能力缺口

WidgetNode
  id, name, type, visibility, locked
  slot: anchors, offsets, alignment, autoSize, zOrder,
        padding, horizontalAlignment, verticalAlignment, sizeRule, fill
  layout: width, height, padding, gap, stretch, scale, scrollOffset...
  style: fill, stroke, radius, opacity, fontSize, color, textAlign...
  svgAsset: 资产 ID
  brushes: normal, hovered, pressed, disabled 的 SVG 资产 ID
  prefabId, overrides
  text, value, interactable, children
  motion: { assetId }
  audio: { enabled, volume, pitch, events: { 事件: soundId } }
```

不将实例展开节点保存进工程。单内容容器只能有一个源子节点；叶节点和 PrefabInstance 不保存源 children。实际展开后的派生树可包含实例子节点。

## 磁盘结构

```text
data/projects/<project-id>/
  project.json                       # 当前 revision、名字、manifest 指针
  revisions/<hash>.manifest.json      # 工程元数据、分类及内容引用
  scenes/<hash>.scene.json            # 主场景源树
  assets/<asset-id>/<hash>.svg        # 真正独立的 SVG 源文件
  prefabs/<prefab-id>/<hash>.prefab.json
```

内容文件不可变，按 SHA-256 寻址。相同 SVG 在后续保存中复用同一文件。先完成全部内容写入，再原子替换 project.json；失败不会让当前指针指向半套工程。旧 revision 内容保留，未执行自动清理。

文件夹分类保存在 manifest 的 folders 和资源 folderId 中。组织路径可改名和移动，不参与物理内容地址，不会破坏引用。

## 本地 API

| 方法 | 路径 | 内容 |
| --- | --- | --- |
| GET | `/api/health` | 应用与渲染器标识 |
| GET | `/api/projects` | 工程摘要 |
| GET | `/api/projects/:id` | 组装的完整工程、revision、updatedAt |
| PUT | `/api/projects/:id` | 验证并保存完整交换文档 |

PUT 使用 `If-Match: "<revision>"`；新工程为 `"0"`。同一工程写入串行，过期 revision 返回 409，避免两个窗口互相覆盖。服务只绑定 127.0.0.1，拒绝跨源写入；不提供任意本机路径访问。

完整工程导出为单文件，便于携带与再次导入。导入文件时分配新的工程 ID，避免覆盖原本保存的工程；资产与模板 ID 保持稳定。

## 限制

可选的 `interfaceCatalog` 为 `{ id, title, prefabId, group, source }` 数组。`id` 在目录内唯一，`prefabId` 必须引用当前工程模板。它随项目 metadata 保存，独立模板仍使用原有的 `.prefab.json` 存储。目录引用参与删除保护及文档验证；编辑和预览的界面选择不会修改项目默认场景。

单次保存最大 16 MB；单 SVG 上限 1 MB；源节点总数 10000；源层级 64；展开节点 20000。画布及预制体设计尺寸为 16–16384。复杂 SVG 样式表应优先使用原生 presentation attributes；基础 class 与 inline 样式会在导入时归一化。
