# MDC1200 RF 实验入口（按用户后续请求开放）

本次开放的是可编译、可选择的 **GD-77 实验 RF 后端**。正常构建仍不发送
MDC，设置默认仍为 Off。实验构建菜单显示 `PTT:MDC1200 EXP`，选中后只有
有效普通 FM 手动 PTT 的正常释放才尝试一帧。没有改变频段限制；业余频段
使用说明不替代物理层验证。**真实 RF 解码验收 Not Run，不能称为可用/已修复。**

## 启用与构建

```sh
PATH=/tmp/gd77-toolchain/root/usr/bin:$PATH python3 tools/build_gd77.py \
  --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc \
  --newlib /tmp/gd77-toolchain/root/usr \
  --out /tmp/gd77-mdc-rf-verified --experimental-mdc-rf --link
```

该参数加入 `GD77_EXPERIMENTAL_MDC_RF=1` 和独立 GPL-2.0 编码器对象。
默认构建不加入该对象，其他平台拒绝启用。AMBE donor 不下载/入库。
实验对象编译通过不意味着成功链接；现有 newlib 固定段链接问题仍在，
没有可刷写固件产物。不得用部分对象/失败 ELF 刷电台。
在完整 IDE 工程中也须显式定义该宏，添加 `tools/mdc1200` include 路径和
`mdc1200_codec.c`，确保全部新增源参与同一目标构建。

这次请求允许尝试实验方案，因此原“没有 RF 后端”的阻断已转为显式实验
路径；主机模块带 GPL-2.0 标记及 COPYING，组合发布的许可兼容性并未解决，
不能据此声称组合固件可统一按某种许可证分发。

## 与旧 MDC/现有 APRS 的区别

* 编码仍来自已用独立 golden 验证的新模块，不导入旧 MDC 文件。
* RF 源为 AT1846 tone1 → TX 调制。新版 APRS 已使用此入口；
  [AT1846S Programming Guide 1.4](https://www.opengd77.com/downloads/datasheets/AT1846S/AT1846S-ProgrammingGuide-1.4.pdf)
  也给出 tone1 的 TX 路由和频率寄存器单位。没有把本机喇叭当作 RF 路径。
* 不复用 APRS 的 FTM1，不改 PIT0 的 1 ms。独占空闲 PIT1 作为符号事件，
  PIT2 作为总线时钟计数器；退出恢复两个通道及 NVIC 优先级/使能状态。
  资源已占用时拒绝启动；未启用宏时不定义这些中断处理器。
* 根据实际 BusClk 使用整数余数累计器，符号截止点为
  `start + floor(n * BusClk / 1200)`。重装定时器扣除已消耗时间，避免逐位
  相对延迟累积。正常帧为 208 符号/173.333 ms；UI 总截止时间 250 ms。
* 不在符号 ISR 内调用阻塞 I²C/SDK nonblocking 启动函数/abort 辅助函数。
  新字节 IRQ 引擎只依次写地址、寄存器、频率高/低字节，收到 ACK 后推进；
  不轮询、不睡眠、不分配内存。只有音调变化才写 35h。
* 400 kbit/s 四个 ACK 字节最低约 90 us；完成写入超过 200 us 报错，
  尚未完成写入跨入下一符号也报错（最晚下一 833 us 边界处理丢失 IRQ）。
  符号 IRQ 迟到超过 25 us、NACK、仲裁失败、总线忙、时钟变化或 TX 标志
  丢失均中止，不补发/重试整帧。25/200 us 是实验工程阈值，非标准容差。
* 错误中断直接拉低 VHF/UHF PA GPIO，清 `txPAEnabled`；仍保留
  `trxTransmissionEnabled`，让原界面执行完整 TX→RX 收尾。取消/TOT 也走
  同一清理出口，不等待界面来关故障 PA。持续硬件总线故障仍需实机故障验收。
* 进入 RF 前复用已有 35/41/79/3A 快照，设 tone1=1800 Hz，使用 TX 频段
  的音调偏移校准值。MIC 选择改为 tone1，不动本机 amp/mux。退出先停 IRQ、
  使原寄存器缓存失效，再恢复快照。保留原 TX CSS 和 FM 滤波，不写校准、
  功率、信道原始数据；两音幅度、滤波和 CSS 影响须实测。

共享 I²C 的检查/占用必须原子化。AT1846 接口的极短 PRIMASK 检查/占用适用于
任务和原 APRS ISR，传输期间不长时间屏蔽中断；busy 返回真实失败，避免调用者
使用未初始化读取结果。EEPROM 只将原有临界区入口移至所有权检查前，堵住
“检查空闲后被 MDC IRQ 抢占，再覆盖 owner”的竞争；地址、格式、页写流程和
CPS 命令均未改变。冲突时返回失败，不静默假报保存成功。该改动有实际 EEPROM
包装函数的竞争注入测试，尚未在 CPS/电台上验证。

## 测试及诊断

```sh
python3 tests/feature_migration/run_host.py
python3 tests/feature_migration/test_mdc.py --reference /tmp/mdc-reference --output /tmp/mdc-evidence
python3 tests/feature_migration/test_rf_channel_model.py /tmp/mdc-reference
```

主机测试直接编译实际板级 ISR 源码，替换成假 MK22 寄存器；覆盖三 ID 全部
208 符号、四字节 ACK 序列、重入拒绝、NACK、资源占用、总线冲突、超期、
时钟变化、TX 丢失、取消及资源恢复。另直接测试实际 UI MDC 释放辅助函数：
仅一次尝试、完成、禁用场景、TOT、250 ms 截止和启动失败恢复。
调度器覆盖四种 BusClk（含不能整除 1200 的情况）及计数回绕。UBSan 无异常。

`mdcRFDiagnostics()` 提供 Unit ID、BusClk、已过符号数、成功调音写入次数、
最大 IRQ 迟到、最大写入耗时及首个原因；时间单位为 BusClk tick。reason：
2=总线 ACK/仲裁失败或取消未完成写入；3=写入完成超 200 us；4=无法重装
下一截止点；5=符号迟到/上次写入未完成/TX 丢失；6=总线被占用；7=时钟变化；
8=运行中取消。`FM_FEATURE_TRACE` 仍默认关闭，供外部调试器采样。
未提供用 CPS 扩展命令读诊断的接口，避免改动兼容协议。

## 关键限制：相位不能靠推测

独立 Kaufman 解码器接收了“90 us 调音延迟 + 0..25 us 变化、保持连续相位”
的三个合成 ID；“每次变频复位相位”的三个模型全部解码失败。
日志在 `validation/mdc-rf-channel-model.json`，它们是 **假设模型，不是 RF 捕获**。
这明确说明：写正确频率/字节和按时完成 I²C，并不足以证明兼容。

AT1846 的实际变频相位、寄存器生效点、高低字节瞬态、音频滤波延迟和 RF
频偏尚未测量。若 RF 显示相位重置/明显失真，这条实验通路不能作为修复，
必须继续寻找可证实的连续相位硬件通路，不能用更多常量调整宣称成功。
不把已有 APRS 能发声当作 MDC 成功证据。

完整构建恢复后，由操作者决定是否人工测试：先假负载/衰减耦合记录 SDR IQ，
同时记录 SCL/SDA 以对齐 ACK 与实际频率/相位变化，再用独立解码器确认
op=01/arg=00、Unit ID 和 CRC。每 ID 至少 20 次，保留全部失败捕获；
商业兼容接收机还需确认 POST-ID 语义。详见 [人工验收](RF_ACCEPTANCE.md)。
本次没有刷机、没有发射、没有修改真实电台或备份数据。

## 本次结果

| 项目 | 状态 | 结果 |
|---|---|---|
| 实验构建 ARM C 对象 | Pass | 130/130，包含板级后端和独立编码器 |
| 默认关闭构建 ARM C 对象 | Pass | 129/129，不包含 GPL 编码器对象/新 IRQ 处理器 |
| 主机故障/资源/释放/设置/ABI 测试 | Pass | 全部通过；原设置 57 字段布局仍一致 |
| 连续相位延迟模型外部解码 | Pass | 3/3 个 ID，仅模型 |
| 复位相位模型外部解码 | Fail | 0/3 个 ID，暴露待测物理风险 |
| 实验完整固件链接 | Blocked | 现有 newlib 脚本位置 0x5F4A8 已越过固定 0x54000 |
| 真实 RF 波形/解码/CPS 回归 | Not Run | 未连接或操作电台/SDR |

编译/测试/命令/对象空间日志：[validation/mdc-rf](validation/mdc-rf/host-tests.log)。
新增对象空间是未链接统计，不能证明最终 Flash/RAM 可容纳。
新旧构建警告种类/数量一致：原有 4 个 sound.c #warning、14 个菜单 strncat
警告；无新增警告。原阶段 5 的构建日志保留作历史基线。
