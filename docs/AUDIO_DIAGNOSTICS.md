# GD-77 公共提示音诊断与隔离交付

## 实机现状与结论边界

2026-11-08 用户反馈：前次交付固件正常启动；CPS E2026.05.26.01 读取、写入、
设置保存正常；FM 接收语音及麦克风发射语音正常。DMR 暂无测试条件。
Prompt=Beep、Beep Volume=0 dB、Talk Permit=On、PTT Tone=Beep，普通 FM 信道
允许 Beep，但按键音、Talk Permit 和 PTT RF 尾音均无声。
原始反馈见 [hardware-feedback.json](validation/audio/hardware-feedback.json)。
官方 R20260131 保持为用户实机稳定回退基线。

**已经证实的是源代码中的故障处理缺陷；尚未证实该台电台无声的具体根因，
新候选的实际声音与 RF 验收均为 Not Run。** FM 语音正常说明至少部分功放、
扬声器及 AT1846 音频路径可工作，不能证明 C6000 本机 PCM 播放正常。
PTT RF Beep 是独立 AT1846 路径，不能直接归因于公共本机提示音故障。

## 已检查的链路与修改

| 环节 | 证据、修正与限制 |
|---|---|
| keyBeepHandler → soundSetMelody | 测试从真实 main.c 提取原函数，验证非扫描的按键释放会调用真实 setter；扫描抑制保留。新增 Key 入口计数。 |
| melody_play 更新 | setter 和原 channel/no-key/voice gating 保留；记录请求、接受、拒绝次数，测试确认真实指针更新。 |
| soundTickMelody | 真实函数测试确认频率、时长、音符推进、功放和 FM mux；音符启动时记录 GPIO 电平和 mute 状态。 |
| xTaskCreate | 原代码忽略返回值且总是 Running=true；现按 pdPASS 更新 Running/Handle/AliveCount，并记录成功/失败。测试注入分配失败。 |
| task / PIT | 单步函数由原 task 调用，记录服务次数和 timer_beeptask 重装次数；保留 PIT0 和 vTaskDelay(0) 的原生节奏。测试执行真实 task 外循环，模拟 PIT 递减。实机是否调度由计数增长确认。 |
| OpenMusic | 原代码只重试 -1，失败仍置 beep=true；现检查所有非 kStatus_Success 结果，成功写入并回读 bit1 后才置 open。失败按后续服务 tick 重试，不在屏蔽 PORTC 时 yield。 |
| FIFO 读取 | 原 tmp_val 未初始化且失败后继续使用；现仅在读成功后解释 FIFO，记录读失败、FIFO busy 与原始值。 |
| PCM 写入 | 原代码忽略最终失败；现检查 SDK 结果，失败不提交 phase；连续 10 次失败中止旋律并关闭本机 amp 归属，保留关闭重试。 |
| 关闭模式 | 失败时不假装已关闭；closePending 保留，恢复成功后再接受新的播放。恢复 PORTC 原先的使能状态。 |
| 样本 | 使用局部 16 位样本，移除与 SAI 路径共用 swapper 的风险；频率=0 时写零 PCM，修正原代码静音间隔重复非零相位样本的问题。保留原正弦表、频率换算和音量规则。 |
| FM 新功能 | 仅增加 Permit / Tail 尝试与成功计数，保留 MIC 静音、寄存器恢复、1000 Hz / 150 ms 尾音与排除逻辑；本轮没有改 MDC 编码、调制或 RF 时序算法。 |

SPI 的 kStatus_Success 只证明 MCU 驱动完成了传输，SPI 没有从机 ACK，PCM 计数
不能代替听音或示波器测量。FIFO 一直为 FF 时也不能假称“芯片正常忙碌”。
下层原生 DSPI 阻塞驱动未被改为带超时驱动；硬件总线卡死仍需实机定位。

## 构建、基础差异与内存检查

四个包由同一个 `tools/build_audio_matrix.py` 串行生成，使用同一 GCC 10.3.1、
Cortex-M4 hard-float、-Os -DNDEBUG、newlib-nano、全局 LTO 与固定链接布局。
源码提交和每个包的哈希见 [summary.json](validation/audio/summary.json)。

* `base-audio`：仓库基础提交 `72825f2`，没有自定义 FM 菜单/设置/发射处理，
  显式覆盖 GNU 链接/runtime、Flash 启动修复、EEPROM 总线保护、AT1846 缓存修复、
  本轮公共音频修正及诊断。覆盖文件与 SHA256 在其 isolation.json。
  **它是有明确覆盖项的诊断基础构建，不是官方 R20260131 发布二进制。**
* `ptt-permit`：当前分支，未编译实验 MDC 后端，用于先验证本机声音和两个 FM 功能。
* `full-lto`：当前完整实验版；MDC 默认 Off，本轮保持 PTT Tone=Beep。
* `full-audio-no-lto`：与 full-lto 相同源码、相同其余参数，仅 sound.c、spi.c、
  fsl_dspi.c 使用 -fno-lto，且输出 -fstack-usage，用于隔离这三个编译单元。
  **这不是全固件关闭 LTO，也不能据此排除其他单元的 LTO 回归。**

原生 SPI0/SPI1、DSPI、GPIO、I2S、PIT、HR-C6000 初始化源码与仓库基础提交一致，
见 [unchanged-common-drivers.json](validation/audio/unchanged-common-drivers.json)。
前轮 sound.c 除 Talk Permit 常量表外与基础代码相同。故“基础代码 + 本次 GNU
构建”也必须独立验证，不应把官方发布固件成功启动等同于本地 GNU 构建音频正常。
当前没有支持“LTO 已确认为根因”的实机证据。

FreeRTOS heap_4 仍为 20480 字节，beep 任务栈仍为 1000 字节，main/HR-C6000
任务各 5000 字节；newlib 的独立 4096 字节堆及 ISR 栈布局不改。
新增诊断不调用 malloc、不打印日志、不在每个样本中更新计数。
启用 FreeRTOS 原生栈高水位查询，仅在诊断页刷新时扫描 beep 栈。
no-LTO 静态 .su 记录 task=56、beepSPI=8、SPI0 数组写入=176、DSPI 阻塞传输=56
字节：单条直接调用链约 296 字节，**不包括更深调用、FPU/中断/RTOS 上下文，
不能代替实际高水位或宣称无栈溢出**。任务创建失败与运行时余量需看实机计数。

Flash 校验同时检查两个窗口。当前 full-lto 在 codec2 前仅余 1008 字节；
full-audio-no-lto 仅余 404 字节。所有包在保守上传末尾 0x7F000 前余 996 字节。
没有移动 codec 地址、启动区或上传上限来容纳代码。

复现（工具链参数与原构建相同）：

```sh
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_audio_matrix.py \
  --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc \
  --newlib /tmp/gd77-toolchain/root/usr \
  --out artifacts/gd77-audio \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

每个目录都有唯一 CPS ZIP、BIN、ELF、map、命令、清单和独立 donor。
CPS 先选择 GD-77 和 `GD-77_V4.3.6.sgl` donor，再选择相应 ZIP。
ZIP 内为 OpenGD77.bin + 18 个 GLA，donor 不入 ZIP/Git。构建不操作电台。
此前 Flash 修复、CPS、Codeplug/校准格式和 57 字段设置 ABI 保留；不需清空数据。

## 电台上查看诊断

进入 **Firmware Info / 固件信息**，按下键经过版本及原 credits 页面后，
会出现 **Audio 1/3、2/3、3/3**，每 500 ms 刷新。普通计数显示低 16 位十六进制，
65536 后回绕；完整 32 位结构 `soundDiagnostics` 可由调试器读取。
页面只读，没有调试串口或 CPS 命令扩展。导航本身也会产生正常按键事件。

| 页面/字段 | 如何判断 |
|---|---|
| 1 Create / Fail | 正常应为 1 / 0；失败则先看 Heap/Min。 |
| 1 Tick / Arm | 空闲也应持续增长；不增长说明 task/timer 链需继续查。 |
| 1 Key / Set / Acc / Note | Key 增长但 Set 不增长：按键事件/gating；Set 增长但 Acc 不增长：拒绝条件；Acc 增长但 Note 不增长：旋律调度。 |
| 1 Ch / VP | Ch 合并 channel/no-key 拒绝，VP 为 voice-playing 拒绝。 |
| 2 Open / E | 进入 OpenMusic 成功及失败次数；E 增长先看 S、M。 |
| 2 ReadE / Busy | 状态读失败次数及 FIFO 忙次数；Busy 一直增长而 PCM 不增长时，记录 FIFO 原始值。 |
| 2 PCM / E | 驱动成功写入的 32 字节块数及失败次数，不能直接证明扬声器有声。 |
| 2 Close / E | 退出 OpenMusic 的成功/失败次数；失败会保留重试。 |
| 2 S / FIFO / M | 最近 SPI 状态（0 成功、-1 占用或 SDK 正错误码）、最近 FIFO 及成功回读的模式寄存器。M bit1 为 OpenMusic；它不是当前实时寄存器。 |
| 3 Amp / Mux / Mut | **最近音符启动时**的实际 GPIO 读取及 mute 状态，不是停声后的电平。正常普通 FM 本机提示应为 Amp1 Mux0 Mut0。 |
| 3 Heap / Min | FreeRTOS 当前及历史最小剩余堆，十进制字节；不代表 newlib 堆。 |
| 3 Stack | beep 任务历史最小剩余栈，十进制字节；0 或很小需要继续查。 |
| 3 Permit / Tail | 成功/尝试计数；先看按键音，再观察相应 PTT 前后增量。基础包这些值恒零。 |

建议先用 `ptt-permit`，仅测试按键；若无声，记录三个 Audio 页的数据。
若 PCM 增长且 Amp1/Mux0/Mut0 仍无声，应进一步测量 C6000 输出、mux 输入输出、
模式/音量寄存器，而不是绕过 gating。若 FIFO=FF 长期不变，先核对 SPI 波形及
芯片状态。必要时比较 base-audio 与 full-lto / full-audio-no-lto：

* 基础包也失败：公共音频或构建/驱动路径仍有问题。
* ptt-permit 成功而 full-lto 失败：继续隔离完整实验构建差异，保持 MDC 菜单关闭。
* full-lto 失败而音频单元 no-LTO 包成功：才有实机依据继续缩小 LTO 相关单元。
* 四者都成功：说明候选可播放，但仍不能从中推断旧固件确切失败的那一步。

必要的逻辑分析仪检查：原生 C6000 SPI0 为 D0=CS、D1=CLK、D2=MOSI、D3=MISO，
配置 CPOL=0 / CPHA=1（mode 1）、MSB first、目标 3 MHz；这是接口源码配置，
实际频率仍需测量。查看 page04/reg06 OpenMusic bit1、page04/reg88 FIFO，及
page03/reg00 的 32 字节 big-endian PCM 写入。不得把它与外部 Flash 的 mode 0 混淆。

## 新功能独立人工验收

本轮软件未操作电台，下列物理测量全部 **Not Run**。

1. 公共提示音：关闭扫描，保留用户已确认的 Beep/0 dB/信道允许配置，先测普通按键，
   保存三页诊断增量。测试多个按键、菜单进出及提示音结束后 FM 接收恢复。
2. Talk Permit：仅在公共按键音正常后，普通手动 FM、Talk Permit=On，验证本机三声
   标称 940 Hz、每声 40 ms、间隔 10 ms；旋律总长 140 ms。用本机录音测量，
   原生查表换算有量化误差。独立接收设备确认这三声没有进入 RF；在提示期间讲话
   验证 MIC 静音，提示排空后讲话验证恢复。再测松键中止和 On/Off 对比。
3. PTT Beep：普通 FM、PTT Tone=Beep、MDC 不启用，独立接收机/SDR录下松开 PTT 后
   仍保持载波的 1000 Hz / 150 ms 尾音。建议测量容差频率 ±2%、持续时间 ±10 ms，
   记录接收机音频与载波结束时间，确认每次仅一次、之后 RX 恢复。Permit 与尾音
   分别开关，确认互不依赖。无需更改或重置任何校准数据。
4. 如果 Tail 尝试=0，检查会话资格/VOX/APRS/TOT 等排除条件；有尝试但成功=0，
   继续定位寄存器读取、快照、校准查询、写入及回读。成功增长但 RF 无尾音，则需
   测量 AT1846 音调/调制路径及偏移，不能用成功计数判定 RF 合格。
5. 按原 [RF_ACCEPTANCE.md](RF_ACCEPTANCE.md) 测试 TOT、快速重按、RX-only、DMR 等
   回归；CPS 再读回设置/Codeplug，核对既有备份。DMR 语音仍待有测试条件时验收。
   官方 R20260131 是稳定回退基线，诊断 base-audio 不能替代它。

## 软件验证证据

* [host.log](validation/audio/host.log) 与 [host-optimized.log](validation/audio/host-optimized.log)：
  实际按键函数、setter、旋律、worker、任务外循环、正/负错误、失败恢复、GPIO/PCM；
  使用模拟 RTOS/GPIO/SPI，UBSan，另以 -O2/-flto 运行。原功能/Flash/EEPROM 主机回归保留。
* [syntax.log](validation/audio/syntax.log)：GD77/GD77S/DM1801/RD5R，包含新增诊断页面。
* [settings-abi.log](validation/audio/settings-abi.log)：两种 enum ABI 下 57 字段不变。
* [mdc-unchanged.log](validation/audio/mdc-unchanged.log)：原独立参考解码、CRC/FEC/PCM 测试通过；
  只作未修改算法的回归，不能代替真实 RF 验收。
* 各构建目录：ELF/BIN/ZIP 检查、实际 task 反汇编、清单；两个完整构建另有镜像变异拒绝测试。
  no-LTO 的 .su 文件保存于 [validation/audio](validation/audio)。保留旧有编译器警告。

## 交付清单与 SHA256

构建源提交：`40f5034a8e9208585c68a525ae9c7da71eb982ac`（三个功能分支构建 tracked clean）。
基础构建的 manifest 按实际记录 `72825f2` + dirty；覆盖来源为上述提交，所有文件
哈希在 isolation.json，不把它假装成原封不动的官方固件。
全部 BIN 长度为 502812 字节。以下 ZIP 位于 `artifacts/gd77-audio/<构建>/`：

| 构建 | ZIP 文件 | codec2 前 / 上传末尾余量 |
|---|---|---|
| `base-audio` | `OpenGD77-PTT-TalkPermit.zip` | 5692 / 996 字节 |
| `ptt-permit` | `OpenGD77-PTT-TalkPermit.zip` | 3568 / 996 字节 |
| `full-lto` | `OpenGD77-MDC1200-EXP.zip` | 1008 / 996 字节 |
| `full-audio-no-lto` | `OpenGD77-MDC1200-EXP.zip` | 404 / 996 字节 |

**base-audio**

BIN SHA256：`70899d18335a133bc08a2eb6445c40b9e08d26ca4279d50163b1d199c2da2252`。

ZIP SHA256：`9adf9bcd9b0bc1276d5cc8356ae7a602b0c2dfd8be2bcd786ecb7105b905d269`。

**ptt-permit**

BIN SHA256：`a16cbd1e246f38bb85c472904d269ec5a5866bc6bb8aba48924363a07d765962`。

ZIP SHA256：`0e7a077d6f705df15d5f0ae93758f399edac0c268973814974fbf73dbf5e13d2`。

**full-lto**

BIN SHA256：`5f2dc6be4c46c2cdca3ec3b8ca2eebcbf9ccfe30a38bcfa3493c95a77f9773a7`。

ZIP SHA256：`71f2cdc376d2648461fe7a412a834bf475ff43565a621b59d79ac9e13bf5fdbb`。

**full-audio-no-lto**

BIN SHA256：`54574105ec9a84951bfb3f2aaf4872034afe105bb0e830315a2f37b5f6e0953a`。

ZIP SHA256：`1f9bd6507fc352af0927be112e25563dbefdded35303db2e9372f8934ce5baf7`。

阶段提交：`e9b1347` 公共音频错误处理与故障测试；`3687eb7` 电台诊断页和隔离构建；
`a5431ec` 基础对照保留 EEPROM/AT1846 安全修复；`40f5034` 保存失败模式回读。
修改文件清单见 [changed-files.txt](validation/audio/changed-files.txt)。
新候选正常启动、CPS 读写、按键/Talk Permit/RF 尾音、DMR、实际栈余量均须实机复验，
此前用户确认通过的旧固件项目不能直接搬成新构建的 Pass。
