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


## 本轮最小功能改动

功能提交 `20a47b2` 只有五个固件文件增量：

* `functions/sound.c/.h`：增加 int16_t 旋律常量，940 Hz ×40 ms，间隔 10 ms，三声；
  原 soundSetMelody、soundTickMelody、beep task、GPIO/PCM/SPI 后端完全保留。
* `functions/settings.h`：将官方 bank 0 bit 2 的 BIT_UNUSED_1 别名为 GD-77 的
  Talk Permit Off 位；设置结构没有增加字段，两个 UNUSED 字均不动。官方恢复默认
  将该 bank 清零，所以新默认 On。读取时不写存储，菜单确认/取消/保存沿用原机制。
* `menuSoundOptions.c`：仅追加 Talk Permit，不改变旧菜单/QuickKey 编号，不扩展 GLA
  语言结构。新标题用英文及原字符串语音接口回退，值使用原 On/Off。
* `uiTxScreen.c`：只在普通手动 FM 已允许并进入 TX 后播放一次；排除 VOX、卫星、
  APRS PTT、静音/no-beep、已有音频与 DMR。借用 0x3a MIC 选择位，写入后回读；
  旋律结束后留 10 ms 排空，250 ms 上限，松键/TOT/按键音调抢占/离开菜单均恢复。
  I²C 恢复失败则关闭 PA 并结束 TX，不靠无限重试或延长发射处理。

未带入此前 MDC、PTT RF 尾音、音频诊断页、newlib runtime、LTO 构建脚本或驱动修补。
原样官方 baseline 也保留了官方 SPI Flash 驱动；这次须在官方工具链下验证，
不能拿前轮 GNU 产物替代，也不能假称旧 Flash 问题的具体原因已解决。

存储证明：官方 BIT_UNUSED_1 只有枚举定义，没有业务使用；默认 bank 全清零。
只复用已属于原 settingsStorageWrite 范围内的 bank0 bit2，不变更 magic 0x4784、
存储地址、字段排列或结构长度。旧官方代码忽略该位；恢复默认会重新启用 Talk Permit。
此前实验使用的 UNUSED_1/2 在新分支不解释、不重置。

## 已执行的软件验证

```sh
python3 tests/talk_permit/run.py
python3 tests/talk_permit/check_official.py \
  --source-zip artifacts/official-r20260131/OPENGD77_MK22_20260131.zip
python3 tests/talk_permit/check_settings_abi.py \
  --baseline /tmp/gd77-official-r20260131/OPENGD77_20260131 \
  --cc /tmp/gd77-toolchain/root/usr/bin/arm-none-eabi-gcc \
  --newlib /tmp/gd77-toolchain/root/usr
```

真实 TX helper 的主机测试、UBSan，以及 -O2/-flto 优化对照测试通过；这些优化只用
于主机测试，未加到官方工程。覆盖 MIC 快照/恢复、10 ms 排空、250 ms 超时、
松键/TOT/菜单离开、13 种排除条件、I²C 失败及关闭 PA。四平台语法检查通过。
同一 target ABI 探针比较官方源码与新分支的 57 个设置字段，两种 enum ABI 均一致。
该探针仅作布局比较，**不是使用 MCUXpresso/Redlib 构建固件**。

[host.log](validation/talk-permit/host.log)、
[settings-abi.log](validation/talk-permit/settings-abi.log)、
[official-diff.json](validation/talk-permit/official-diff.json) 为证据。
实际链接/Flash 空间检查/CPS ZIP：**Blocked，缺 MCUXpresso/Redlib**。
官方 native baseline 重建及本机按键音：**Not Run**。
新 Talk Permit 本机声音/MIC 静音与恢复/RF 不串音/CPS 保存回归：**Not Run**。

## 安装与 native 构建的下一步

安装已经获得用户授权；不需要再询问安装许可。当前缺失的是登录后可下载的文件，
而不是编译权限。NXP 下载入口：
https://www.nxp.com/webapp/swlicensing/sso/downloadSoftware.sp?catid=MCUXPRESSO

下载 `mcuxpressoide-24.12.148.x86_64.deb.bin` 到本机 Downloads 后，检查安装包及其
安装选项，再安装匹配 IDE、编译器、Redlib。若可从包内展开独立 IDE，则优先使用
用户目录；系统 sudo 当前需要密码，不索要或记录用户密码。当前没有安装完成的 IDE。

1. 使用 `aac8ae0` 的独立工作树作为 baseline，当前分支作为功能版，分别导入独立
   MCUXpresso workspace（两者原生 project name 都是 firmware）。
2. 先准备 `firmware/linkerdata` 的空 codec 段，使用原 tools/codec_cleaner.Linux -C，
   仅处理本地文件。构建选择官方 **GD-77** 配置，使用 firmware.ld 和 Redlib；
   不选择 HW_Debugger/NMEA/其他平台配置，不替换为 newlib/LTO 配方。
3. 检查 ELF/map、向量/固定 codec 地址、Flash/RAM/栈/上传上限，核对 post-build
   cleaner 的结果，再打包 OpenGD77.bin 和匹配 GLA。仍由 CPS 单独选择 V4.3.6 donor。
   原工程的 FirmwareLoader 命令是注释；不执行任何刷机工具。
4. 先人工验证 baseline 启动、CPS、原按键音、FM 语音；这一步失败就继续定位构建链，
   不能归因于 Talk Permit。只有 baseline 按键音正常后才验收功能版。
5. 功能版先按键音，再设 Talk Permit=On；测本机三声和 MIC 的静音/恢复。独立接收
   设备确认三声不进入 RF；测试 Off、松键中止、TOT、VOX/APRS/卫星/DMR 排除。
   比较 CPS/保存/取消/断电设置，保留原 Codeplug 和校准备份，不清空或重置电台。
