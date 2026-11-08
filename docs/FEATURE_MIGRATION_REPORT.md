# GD-77 R20260131 三功能迁移交付报告

后续按用户请求已开放 **MDC RF 实验构建入口**：`--experimental-mdc-rf`，
界面为 `PTT:MDC1200 EXP`。板级发送器/中断/清理已实现并有主机假硬件测试，
默认构建仍关闭；真实 RF 解码为 Not Run，完整固件链接仍 Blocked。
现状、启用命令和相位模型失败证据见 [RF 实验补充](MDC1200_RF_EXPERIMENT.md)。
以下阶段 1–5 数据是第一次交付的归档证据，关于“无 RF 后端”的描述已由此补充取代。

首次交付完成了 PTT Tone/Talk Permit 的源码集成、独立设置记录、受控 FM 尾音、
主机协议编码/连续相位 PCM 及测试。**首次交付时 MDC1200 RF 保持关闭；没有完整
链接成功的可刷写固件，没有 RF/电台/CPS 实机 Pass。** 不能声称旧 MDC 已修复。

工作分支：`feature/gd77-ptt-talk-permit-mdc1200-v2`，基线 `72825f2`。
`main` 保持原 SHA；未 push、未自动刷机、未读取/写入任何电台或用户备份。
用户原有未跟踪的任务说明文件保持原样，未加入提交。

## 分阶段提交

| 阶段 | 提交 | 内容 |
|---|---|---|
| 1 | `0ce9651` | 架构/存储/独立协议审计 |
| 2 | `9f13f8f` | Sound Options、设置记录、本地 Talk Permit |
| 3 | `8b52a91` | 非阻塞 FM RF Beep、释放锁定与恢复 |
| 4 | `c6afe71` | 独立 golden、主机编码器/PCM、外部解码器验证 |
| 5 | 本报告所在提交（`git log -1`） | ARM 编译/ABI/故障注入、最终修正及验收文档 |

全部变更路径见 [changed-files.txt](validation/changed-files.txt)。原版旧仓库只读
参考 SHA `57aed1c76ae4fd95363ec36e9c2d59c591f5bc8b`，没有 cherry-pick、
复制旧驱动/菜单整文件或旧 MDC 算法。旧 RF 失败根因仍未定位，不能推测为
单一 CRC/位序/PIT 问题。已证实旧 100 us 的 tick 方案不能用于新版 1 ms PIT；
本分支没有改变全局 PIT。

## 实现边界

* GD-77 Sound Options 末尾追加三项，原菜单和已有 QuickKey 编号不变。
  PTT Tone 默认 Off，另有 Beep、`MDC1200*`（星号表示 RF 不可用）；ID 为
  4 位十六进制、0001..FFFF 循环增减；Talk Permit 默认 On。
  使用原确认/取消/QuickKey 超时保存，新增标题以拼读方式输出语音提示，
  没有改变语言表/CPS 语言结构；标题目前为英文回退文本。
* 配置复用有空间证明的 GD-77 UNUSED_1/2：ID + tag/version/选项 + CRC-8。
  不增加设置长度、magic、存储地址或 Codeplug 字段。加载时仅在 RAM 初始化，
  沿用现有保存机制；有效 ID 不跟随后续 DMR ID 静默变化。默认/损坏记录
  回到 Off/On/非零 DMR low16。断电保存/CPS/回退仍需硬件验证。
* Talk Permit 用现有 int16_t melody 和声音仲裁，940/40、间隔 10、共三声。
  先确认 FM PA/硬件 TX 状态；播放前断开 MIC，结束增加 10 ms 缓冲排空保护，
  最迟 250 ms 结束本机控制流程。释放/错误/新 1750 或 DTMF 动作提前清理。
  不覆盖正在播放的声音或语音，遵守 silent/no-beep。
* Beep 使用 AT1846 tone1 → RF 通路，1000 Hz/150 ms；不以本机喇叭代替 RF。
  独立状态机不忙等，继承 PTT 释放锁定，每次最多一次；既有 1750/DTMF 先
  收尾再获取尾音快照。保存/恢复 35/41/79/3A，读取 TX 频段音调校准值，
  不改功率、CSS、滤波或校准数据。失败有有限重试及原 TX→RX 清理出口。
* 新增效果只接普通有效 FM 手动会话，排除 DMR、TOT/错误、VOX-enabled、
  APRS PTT Beacon、卫星；自动 TX 的既有入口不接新增效果。VOX-enabled
  时即使手按 PTT 也禁用新增效果，这是保守隔离策略。
* AT1846 缓存失败写入时必须失效，否则恢复旧值会被错误缓存跳过；已加实际
  驱动假总线测试。持续 I²C 硬件故障无法保证寄存器恢复，必须实测 PA 安全
  关闭。软件测试不等于实机故障安全证明。

## 验证矩阵

| 验收项 | 状态 | 证据/限制 |
|---|---|---|
| GD-77 全部目标 C 单元编译 | Pass | ARM GCC 10.3.1，128/128；新增固件逻辑在原实际源单元内 |
| GD-77 完整链接/可刷写产物/最终空间 | Blocked | 基线和本分支均在现有 newlib 固定编解码段前溢出 |
| 其他 MK22 平台语法隔离 | Pass | GD77S/DM1801/RD5R 对五个修改单元做主机语法检查；非目标实机未验证 |
| 设置 ABI、槽位空间、所有原字段 | Pass | 目标编译器比较全部 57 字段，两种 enum ABI 均与基线一致 |
| 设置编码/范围/损坏恢复/稳定 ID | Pass | 393210 种有效组合、32 种单比特损坏、默认/后续 DMR ID 变化 |
| 菜单实机显示、确认/取消、QuickKey、断电保存 | Not Run | 已复用原实现，需按 RF_ACCEPTANCE 手工逐项检查 |
| Beep 一次/150 ms/中止/计时回绕状态机 | Pass | 主机真实控制器测试；不含 RF 时长测量 |
| Talk Permit MIC gate/Beep RF 路由/清理/失败重试 | Pass | 直接编译实际辅助函数，假寄存器/假 I²C，UBSan 无异常 |
| RF Beep/本地三声频率时长/不串入 RF | Not Run | 无 GD-77、SDR、音频仪器连接 |
| DMR/TOT/RX-only/禁发/VOX/APRS/卫星/快速重按完整实机回归 | Not Run | 软件有隔离与控制器测试，尚无完整电台状态机测试 |
| 实机 MIC/CSS/功率/滤波/音频恢复 | Not Run | 新逻辑不修改后四者，快照恢复有假总线验证 |
| CPS E2026.05.26.01 实际读写/Codeplug/校准保持 | Not Run | 协议/原字段/校准写入代码未改；不能由静态检查推出实机 Pass |
| MDC 外部协议参考与可复现 golden | Pass | 独立 Kaufman 固定 commit，3 组 CRC/FEC/交织/完整帧/音调比特 |
| POST-ID 商业设备语义确认 | Not Run | 01/00 与 sdrtrunk ANI/EOT 一致；与 00/80 网页预设冲突已记录 |
| 新纯编码器所有层与外部向量一致 | Pass | 0001/1234/FFFF、长度/空指针/容量、1000 随机外部 CRC 比较 |
| 新 PCM 被原版独立解码器接收 | Pass | 三 ID 的 op=01/arg=00/ID 正确；坏 CRC 拒收、单比特 FEC 恢复 |
| 48 kHz PCM 的符号率/两音/连续相位 | Pass | 40 样本/符号；8320 样本/帧；分块独立性和解析波形检查；仅主机 |
| MDC 实际硬件音频路径/DMA/抖动/RF 调制 | Blocked | 没有经过证明的 RF 音频输入；拒绝用 I²C 逐符号切音补丁 |
| MDC RF POST-ID/Unit ID/CRC 和重复成功率 | Not Run | 无真实 IQ/WAV/商用接收机记录；固件选 MDC 时不发实验帧 |
| MDC 编码器进入固件链接 | Blocked | 主机 GPL 模块隔离；许可证与物理通路均需解决 |

## 构建命令、日志及空间

仓库没有 MCUXpresso .project/.cproject 或 Makefile。新增
`tools/build_gd77.py` 是重建的 GNU 验证入口，不冒充原版 IDE 工程。
工具链在 `/tmp/gd77-toolchain/root`，通过 `apt-get download` 下载并
`dpkg-deb -x` 解包，没有全局安装：gcc-arm-none-eabi 15:10.3-2021.07-4、
binutils-arm-none-eabi 2.38-3ubuntu1+15build1、newlib 3.3.0-1.3。

```sh
python3 tests/feature_migration/run_host.py
python3 tests/feature_migration/test_mdc.py --reference /tmp/mdc-reference --output /tmp/mdc-evidence
python3 tests/feature_migration/generate_golden.py /tmp/mdc-reference > /tmp/mdc-golden-reproduced.json
cmp tests/feature_migration/mdc_golden.json /tmp/mdc-golden-reproduced.json
python3 tests/feature_migration/check_settings_abi.py --baseline /tmp/gd77-baseline --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc --newlib /tmp/gd77-toolchain/root/usr
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_gd77.py --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc --newlib /tmp/gd77-toolchain/root/usr --out /tmp/gd77-arm-final --link
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_gd77.py --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc --newlib /tmp/gd77-toolchain/root/usr --source /tmp/gd77-baseline --out /tmp/gd77-arm-baseline-final --link
```

基线目录来自 `git archive 72825f2`，外部参考 checkout 及复现方法见
[主机实验模块 README](../tools/mdc1200/README.md)。构建共用 flags 包含
`HAS_GPS=1`、Cortex-M4F hard-float、`-Os`、section GC，不强制 enum 大小。
已有 cleaner 复制到临时输出目录后只执行 `-C` 创建空占位段；没有下载 donor。
没有生成 `.bin/.sgl`，失败 ELF 会删除，**没有可供刷入的产物**。

完整原始诊断见 [validation](validation/feature-compile.log)。其中
`*-commands.json` 的 `common + jobs[i]` 就是实际单个编译命令；链接命令、
基线对照、ABI、主机测试、外部解码及 LTO 失败日志均在同目录。
主构建失败：基线位置 `0x5E0D0`、新版本 `0x5EAD8`，脚本要求下一固定段
从 `0x54000` 开始；没有移动固定段或改动 linker scripts 以掩盖问题。
LTO 额外尝试在首个固定段 `0x4400` 也失败；其早期日志中 HAS_GPS include
顺序导致的类型差异不作为可用构建证据，最终主构建统一了此宏。
新旧构建警告分类/数量相同：原有 sound.c 四个 `#warning ERRR NO` 和菜单
十四个 strncat 边界警告；没有新增警告，未做无关全面修复。

在同一目标参数下，五个修改对象总计 **text/rodata +2145 字节、data +0、
bss +30**，逐对象数值见 [arm-object-sizes.json](validation/arm-object-sizes.json)。
这是未链接对象统计，不能代替最终 flash/RAM 使用或证明固件装得下。
目标设置为 112 字节；完整 enum/主机为 116 字节，槽位均在 60/62，
写入范围仍为原结构范围，详见 [存储证明](FEATURE_MIGRATION_PLAN.md)。

## MDC 证据、许可与未完成项

[协议验证文档](MDC1200_PROTOCOL_VALIDATION.md)逐层给出来源、commit、
CRC/FEC/位序/映射、向量、软件证据和冲突。软件成功是新的编码器输出经
外部原版解码器校验，不是新编解码器共用算法的自环。生成的三个合成 WAV
在 `/tmp/mdc-evidence`，SHA256 在 [结果 JSON](validation/mdc-host-results.json)；
它们不是 RF 捕获。没有声称准确定位或修复旧版的实际 RF 解码失败。

GPL-2.0-only 主机模块与固件完全分开编译，附完整许可证；参考库未复制进
固件/仓库。固件保留原作者声明及禁止商用等原条款，不声称可统一按 GPL 或
另一单一许可证发布。没有 AMBE donor 或其他受限二进制入库。

尚缺：官方可复现 IDE/link 配置及成功 map、经仪器确认的 RF 音频输入和
时钟/DMA、RF 电平/滤波/稳态验证、独立真实 RF POST-ID 识别、逐 ID 重复
统计，以及全部电台/CPS 回归。[人工验收与回退步骤](RF_ACCEPTANCE.md)已列明。
后续先恢复基线构建，再逐项补证；MDC RF 在上述边界解除前保持关闭。
