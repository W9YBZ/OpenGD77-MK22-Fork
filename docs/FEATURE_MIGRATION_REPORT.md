# GD-77 R20260131 功能迁移与固件交付报告

**旧版 8f64d48 固件用户实机启动失败：FLASH MEM ERROR，验收状态为 Fail。**
已修正外部 Flash 位操作的时序隐患并加入失败 ID 显示；是否解决该台电台故障
仍待复测。现在 `--link` 一条命令自动生成 CPS ZIP，donor 必须单独选择。
没有自动刷机或操作电台存储。详见 [构建与故障复测流程](BUILD_GD77.md)。

新候选输出：`artifacts/gd77-flash-fix/OpenGD77-MDC1200-EXP.zip`。
下文旧产物哈希和空间表保留为历史证据，不能作为新候选的验收结果。

固件源代码提交：`8f64d486466b1ee53c036d9c6b1a7e4b15ca3637`，构建时 tracked
工作区干净。分支为 `feature/gd77-ptt-talk-permit-mdc1200-v2`；`main` 保持
原始 `72825f2`，未 push。原有未跟踪任务说明未修改或加入提交。

## 旧产物（已报告启动失败，仅留作排查）

| 文件 | 位置/说明 |
|---|---|
| `OpenGD77-MDC1200-EXP.zip` | 同目录，238829 字节；CPS 首选包，根目录 OpenGD77.bin + 18 个 GLA |
| `OpenGD77-MDC1200-EXP.bin` | `artifacts/gd77-mdc1200-exp/`，502812 字节，供 CPS 选择 |
| `GD-77_V4.3.6.sgl` | 同目录，从用户 ZIP 只提取指定 donor；供 CPS 的 donor 选项 |
| ELF / map / manifest / README | 同目录，调试、布局、哈希与人工安装说明 |

BIN SHA256：`58fac6a50d938546db9275dc4553ebf98be382b66857d4dd752a2961891b7150`。
两个独立目录重建的 BIN 逐字节一致；GCC 日期/时间宏使用源码提交 epoch。
ZIP SHA256：`927a4e751f0e77c635785771ad41827f348262faee64dbe952ce4287d063fae4`。
ZIP CRC、唯一根目录 BIN 和所有 18 个匹配 GLA 已验证；donor 不放入 ZIP。
打包证据见 [cps-zip-package.json](validation/linked-release/cps-zip-package.json)。

BIN 保留两个 FF 编解码占位段，**需要 CPS 合并 donor**。在现有
CPS E2026.05.26.01 的 Extras / Firmware Loader 选择 GD-77，先选择
`GD-77_V4.3.6.sgl` 为 donor，再选择上述 ZIP 为 OpenGD77 固件包（包内 BIN 与已校验固件逐字节相同）。
不要把 donor 当成待刷固件，也不要把占位 ELF/BIN 经原始 SWD 直接刷入。
构建脚本不会调用刷机程序。

用户提供的 ZIP 中 V4.3.6 member 已通过 ZIP CRC、长度、SGL 签名和固定
SHA256 检查。Donor SHA256：
`d8bdfcc4c5fbb5e1f58e2d6f243381c673d6d09d4718b1ce9178e50fab01706f`。
官方其他版本、CPS 安装器和频段解锁文件均未使用。整个产物目录受 Git-ignore
保护，受限 donor 和生成的二进制均未入库。

## 已实现行为

* Sound Options 追加 PTT Tone、MDC1200 ID、Talk Permit，原项目和 QuickKey
  编号不变。默认 Off / 非零 DMR low16 ID / On。ID 为 0001..FFFF 的四位
  十六进制；原确认/取消/保存机制保留。新增标题用英文回退与字母语音提示，
  不改变 CPS 语言表格式。
* 设置复用 GD-77 原结构偏移 60/62 的两个未用字，以 tag/version/选项/CRC-8
  保护。不增加结构长度或存储地址、不改变 magic 0x4784；有效 ID 不跟随
  后续 DMR ID 静默变化。目标默认 enum ABI 为 112 字节，完整 enum 为 116；
  全部 57 个字段在两种 ABI 下均与基线一致。
* Talk Permit 通过既有 int16_t melody/音频仲裁，本机三声 940 Hz ×40 ms，
  中间 10 ms；播放时断开 MIC，正常结束加 10 ms 排空保护，控制流程最迟
  250 ms 退出。遵守 silent/no-beep，不覆盖已有语音或提示音。
* Beep 用 AT1846 tone1 → TX 调制，1000 Hz/150 ms 非阻塞尾音，每次最多一次。
  保持 PTT 释放锁定；先收尾已有 1750/DTMF，再获取寄存器快照。
* MDC 实验构建显示 `PTT:MDC1200 EXP`，显式选择后才尝试正常手动 FM 释放
  POST-ID；默认仍 Off。编码 op=01/arg=00，独立 golden 与外部 PCM 解码通过。
  PIT1/PIT2 与逐字节 I²C IRQ 控制时序，忙/超期/NACK/取消/TOT 不重发整帧；
  故障直接关闭 PA，再经原 TX→RX 流程恢复快照。未改 PIT0 或 APRS 的 FTM1。
* 新效果排除 DMR、错误/TOT、VOX-enabled、APRS PTT Beacon、卫星和其它自动
  TX 入口。VOX-enabled 时手按 PTT 也保守禁用。设置、CSS、功率、校准和
  信道原始数据不作新增永久修改。
* 为异步 I²C 补充原子总线占用、真实 busy 返回及失败写缓存失效；EEPROM 仅
  将原临界区移到 owner 检查前，不改变地址/页写/数据或 CPS 命令。
  详细 RF 路径与测量限制见 [RF 实验说明](MDC1200_RF_EXPERIMENT.md)。

## 链接修复与空间证据

此前失败源于重建 GNU 构建入口不完整，并非证明基线源码无法构建：

1. 用 `-nostdlib` 消除 GCC 隐式完整版 libc，只用链接脚本的 newlib-nano；
   使用一致的 nano headers、`-Os -DNDEBUG` 和显式 LTO。
2. startup 不参与 LTO，避免 SystemInit/main 被内联进 1 KB 启动窗口。
   calibration/menuCalibration 也不参与 LTO，以保留目标原 `firmware.ld` 的
   高位代码布局策略。newlib 脚本显式收集高位语言/校准代码和 ARM 元数据。
3. 编解码段在汇编中明确为 ALLOC，防止“链接通过但段未真正占位”的伪成功；
   增加固定地址/大小/载入区/堆栈 ASSERT。**没有移动固定编解码地址。**
4. 4 KB stack 变为真实 NOLOAD 保留区；GNU-only bounded `_sbrk` 使用预留
   4 KB heap，而非 libnosys 在上部 BSS 后无限扩展堆。
5. ELF/BIN 后处理逐段检查，验证通过才发布最终 BIN 文件名。

| 位置/资源 | 最终结果 |
|---|---|
| 启动区 / SP / Reset | 0x4000，SP=0x20000000，Reset=0x41C9 |
| codec1 | 0x4400..0x5300，0xF00 字节，ALLOC/LOAD |
| codec2 | 0x54000..0x7C090，0x28090 字节，ALLOC/LOAD |
| 高位应用及异常表结束 | 0x7EC1C |
| CPS 保守上传上限 | 0x7F000；BIN 尚余 **996 字节**，后续新增代码必须重查 |
| newlib heap / MSP stack | 各 4096 字节，互不重叠 |
| GNU size text / data / bss | 461124 / 38504 / 60608；text 含 codec 占位，bss 含 NOLOAD 保留区 |

这不是可任意使用的剩余堆/任务栈统计；运行时峰值和 watchdog 响应仍须实机测量。
Bootloader 的 FlashConfig 被应用链接脚本明确排除；所有实际载入地址均处于
应用 Flash 区域，未侵入 bootloader 或保留区。

构建与完整人工安装命令见 [BUILD_GD77.md](BUILD_GD77.md)。原始完整证据见
[linked-release](validation/linked-release/manifest.json)：编译/链接/命令/size、
实际 ELF/BIN 布局、哈希、控制构建与独立重建一致性。

## 当前验收矩阵

| 项目 | 状态 | 证据/限制 |
|---|---|---|
| MDC 实验目标全编译、链接与 BIN 导出 | Pass | 131 C 单元，固定布局和所有载入区检查通过 |
| 默认 MDC 关闭控制构建 | Pass | 130 C 单元，完整链接并导出检查通过 |
| 启动向量、Flash/RAM、codec 占位、堆栈/上传大小 | Pass | ELF32/PHDR/节/符号及 BIN 逐字节检查 |
| 损坏镜像拒绝 | Pass | 向量/数据/codec 翻转、截断、追加、缺 ALLOC 和重叠均拒绝 |
| newlib heap 边界与极值 | Pass | OOM/收缩/PTRDIFF 极值主机测试，UBSan 无异常 |
| 设置 ABI / 范围 / CRC / 稳定 ID | Pass | 57 字段、393210 种有效组合、32 种单比特损坏 |
| 现有菜单/保存/QuickKey 实机操作与断电保存 | Not Run | 复用原接口；源代码和主机记录测试不代替电台操作 |
| 尾音/permit/实际 MDC IRQ 及释放辅助函数 | Pass | 假硬件/故障注入；一次尝试、完成、TOT、截止、取消与恢复 |
| 其它 MK22 平台隔离 | Pass | GD77S/DM1801/RD5R 修改单元主机语法检查；非目标实机未测 |
| CRC/FEC/交织/帧/位序独立向量 | Pass | 0001/1234/FFFF；外部参考生成 golden，无旧 MDC 算法复用 |
| 外部原版 Kaufman 解码新 PCM | Pass | op/arg/ID 正确，坏 CRC 拒收、单 bit FEC 恢复、1000 CRC 对比 |
| 延迟且连续相位的合成模型 | Pass | 三 ID 可独立解码；仅模型 |
| 变频复位相位的合成模型 | Fail | 三 ID 全失败；明确暴露物理通路风险，不是实际 RF 结果 |
| GD-77 旧固件实际启动 | **Fail** | 用户报告 FLASH MEM ERROR；新候选实机复测 Not Run |
| FM 尾音 / Talk Permit 不串入 RF | Not Run | 启动故障尚待实机闭环 |
| MDC 实际相位/baud/频偏/电平/POST-ID/重复成功率 | Not Run | 实验路径已开放，不能称为修复或已兼容 |
| 完整 DMR/TOT/RX-only/禁发/VOX/APRS/卫星/快速重按回归 | Not Run | 软件隔离与故障测试通过，完整硬件状态机尚未验收 |
| CPS E2026.05.26.01 读写、Codeplug/校准/回退实机 | Not Run | 协议/布局保留；本次没有电台写入或自动刷机 |

原有四个 sound.c #warning、十四个菜单 strncat 警告保留；没有新的未解释警告。
旧失败日志保留作历史，不能替代此处新的成功构建证据。

## 来源、许可和提交

旧仓库只读参考 SHA `57aed1c76ae4fd95363ec36e9c2d59c591f5bc8b`，没有移植
旧 MDC 编码链或 cherry-pick 老提交。旧实际 RF 失败根因仍未被证明。
协议参考版本、冲突、CRC 中间值及完整 golden 见
[MDC1200_PROTOCOL_VALIDATION.md](MDC1200_PROTOCOL_VALIDATION.md)。

独立主机编码模块明确 GPL-2.0-only，固件保留各作者/依赖的原许可；实验本地
构建不构成组合发布许可兼容性的结论。受限 donor 与生成产物不在 Git 中，
不声称合并后可统一按某种单一许可证传播。

| 提交 | 内容 |
|---|---|
| `0ce9651` | 架构/存储/独立协议审计 |
| `9f13f8f` | 设置菜单与本地 Talk Permit |
| `8b52a91` | 受控 FM RF Beep |
| `c6afe71` | 独立 golden、PCM 与外部解码测试 |
| `41e45d3` | ABI/故障测试与第一次交付报告 |
| `dc2a626` | 显式 MDC RF 实验后端和总线/截止保护 |
| `8f64d48` | 本次固件源码：修复 GNU 链接、运行时堆和 BIN 导出检查 |

全部变更路径见 [changed-files.txt](validation/changed-files.txt)。后续由用户
控制人工刷入和 RF 实测，验收/回退步骤见 [RF_ACCEPTANCE.md](RF_ACCEPTANCE.md)。
