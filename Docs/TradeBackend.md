# 交易后端与地图掉落池

主工程的独立后端放在 `CustomTools/TradeBackend`。选型为 Go 1.27.3 + PostgreSQL 18.6；本机 API 地址 `http://127.0.0.1:5188`，数据库只监听回环地址 55488。启动、停止、接口合同、部署与验证说明见 [后端 README](../CustomTools/TradeBackend/README.md)。

本次提供：账号/会话、物品实例、冻结上架、购买/撤单、整件和堆叠物品转移、订单、双边流水、事务 outbox，以及跨实例幂等请求。发放资产和入账只允许独立的游戏服务端凭据。现阶段 UE 接入地图掉落候选池；游戏账号登录、在线背包同步和交易页面尚未接入，不接受本地存档作为在线资产凭证。

地图创建或核心变更时，`Script/Opus/TowerGame.as` 调用 `UOpusDropService` 异步请求 `/v1/maps/{mapCoreId}/drop-pool`。后端读取同一份 `Content/RulesTower` 定义，返回内容版本、地图 T 阶和装备/道具模板候选。UE 校验版本及 ID 后，DWRules 宿主将目录视图交给原有掉落算法。没有修改 DWRulesCore 的抽取算法、概率、品质或数值；商人逻辑和灵石奖励沿用原实现。

首次 HTTP 在 HUD 同步构造结束后的组件首帧发送。未取到池时不能开战；更新核心时暂停规则模拟等待新池；网络失败重试三次后保持准备状态，手动开始可再次请求。重置或跨层旅行都创建新请求，销毁旧地图会取消旧回调。后端无需逐次击杀请求数据库。

验证包含：

- PostgreSQL 实测 100 个请求在两个 API 实例竞争一件装备，只成交一次。
- 50 次同键购买重试只扣款一次；余额不足与越权发物/入账被拒绝。
- 堆叠物品部分转移与撤单、订单重读、成交和 outbox 同事务提交。
- 真实成交负载 100 QPS，持续 10 秒，共 1000 笔全部提交成功；本机 4 个 Go 调度线程、回环 HTTP 和本机 PostgreSQL，非云服务器容量承诺。延迟数据保留在 [压测结果](../CustomTools/TradeBackend/verification/purchase-100qps.json)。
- UE 原生 HTTP 联调：完整池 71 种装备 + 18 种道具；受限池仅霜棘法杖和下品灵石，真实 Boss 击杀只从该池掉落；更换核心与重置后再次请求；服务不可达时保持准备状态。

主工程记录见 `Docs/TradeBackendVerification.json`，UE 原始日志在 `Saved/Verification/TradeBackend/`。没有新增 UI，也没有新增游戏运行时 JS。

## 地图奇遇快照

同一个地图掉落池响应携带 `events`：由 `Content/MapEvents/events.json` 校验并冻结的地图奇遇配置。配置变化只更新 poolVersion，不改变 catalogVersion，也不改变普通装备或通货数值。每张地图实例固定一份配置；奇遇不增加第二次 HTTP 请求。容器配置只读挂载此文件，MAP_EVENTS_FILE 可以替换来源。UE 会校验奇遇结构后才将地图准备状态标为成功。

当前提供百炼法坛、封印宝库、传奇三选一和万相合成祭坛。具体游戏流程与本地装备锁在 `Docs/MapEvents.md`，不由网站客户端直接上传物品来铸造资产。
