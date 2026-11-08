# 从官方 R20260131 重新开始

新分支：`feature/gd77-talk-permit-official-r20260131`，基于 `72825f2`。
此前实验和用户“仍不工作”的反馈保存在旧分支 `feature/gd77-ptt-talk-permit-mdc1200-v2`。
不重写或强推 main。新分支的功能范围只有 Talk Permit。

已直接从 opengd77.com 下载官方源码 ZIP（来源与 SHA256 见 OFFICIAL_SOURCE.json）。
317 个文件中，315 个与仓库基线在 CRLF 归一化后相同，没有源码内容差异。
原 Fork 唯一遗漏的是 `.project`、`.cproject`，现从官方包逐字节恢复并跟踪。
这是完整官方工程的恢复，不是重新手工猜测编译参数。

官方 Readme 要求 MCUXpresso IDE v24.12 或更早，指出更新工具链存在 TX 音频问题，
原因尚不明确。该警告不能用来证明前轮 GNU 构建无声的根因。
原 GD-77 工程使用 Redlib、heap_4、无显式 LTO、官方 firmware.ld，以及多处单文件
优化选项；保留所有原配置。不得将前轮 newlib-nano/LTO 配方标为“官方构建”。

用户授权安装 IDE。目前 NXP 下载入口实际重定向到 login.nxp.com，需用户登录下载
`mcuxpressoide-24.12.148.x86_64.deb.bin`。本机 sudo 需要密码；若包支持解包，将先
安装到用户目录，仅作本地构建，不安装/调用电台刷写驱动。当前尚未安装 IDE。

先保留这个无功能改动的官方工程恢复提交作为 native baseline，再单独提交 Talk
Permit。须用同一 MCUXpresso/Redlib 先构建此基线并验收原按键音，再验证功能版。
编译成功不替代实机声音验证；尚未执行本轮刷机、复位、数据清空或 RF 发射。

官方说明：
https://www.opengd77.com/downloads/releases/MK22/R20260131/sources_and_tools/Readme.md
