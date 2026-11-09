# 从官方 R20260131 重新开始：仅 Talk Permit

当前分支 `feature/gd77-talk-permit-official-r20260131`。旧实验及用户“仍不工作”的反馈
保存在旧分支，不重写 main。官方工程恢复提交 `aac8ae0`；Talk Permit 实现 `20a47b2`。
两套 native 固件已生成，**本轮没有刷写电台，实机启动/音频/CPS/RF 均为 Not Run**。

## 官方来源和最小代码范围

已直接从 opengd77.com 下载官方 R20260131 源码 ZIP，317 个文件中，315 个与旧仓库
基线在 CRLF 归一化后相同，唯一遗漏的是 `.project`、`.cproject`。现从官方包逐字节
恢复两者。来源与 SHA256 见 [OFFICIAL_SOURCE.json](OFFICIAL_SOURCE.json)。
官方说明要求 MCUXpresso v24.12 或更早，指出新版本存在 TX 音频问题；这条说明
不能作为前轮无声的已证实根因。本轮使用匹配版本，遵循完整官方配置。

功能增量只有五个固件文件：

* `functions/sound.c/.h` 只增加 int16_t 三声旋律，940 Hz ×40 ms、间隔 10 ms；
  setter、旋律调度、beep task、音频 GPIO/PCM/SPI 后端均保持官方原样。
* `functions/settings.h` 将官方 bank0 bit2 的 BIT_UNUSED_1 别名为 GD-77 的
  Talk Permit Off 位。原代码中该位没有业务使用，官方默认 bank 全零，因此默认 On。
  设置结构、magic 0x4784、存储地址、两个 UNUSED 字和所有既有字段均不变。
* `menuSoundOptions.c` 仅追加 Talk Permit On/Off，保留旧菜单/QuickKey 编号、确认/
  取消/保存机制，不扩展 GLA。标题用英文及原字符串语音接口，值用原 On/Off。
* `uiTxScreen.c` 在普通手动 FM 已允许并进入 TX 后播放一次，排除 VOX、卫星、
  APRS PTT、DMR、静音/no-beep、已有音频。借用并回读验证 0x3a MIC 选择位，旋律
  结束后留 10 ms 排空，250 ms 上限，松键/TOT/音调抢占/菜单离开都恢复。
  I²C 恢复失败则关闭 PA、结束 TX，不靠无限重试或延长发射处理。

未带入 MDC、PTT RF 尾音、诊断页、newlib runtime 或驱动修补。官方 SPI Flash、
EEPROM、CPS、Codeplug、校准和音频驱动原样保留；不是用前轮 GNU 固件充当基线。
旧固件会忽略新增设置位；恢复官方默认会重新启用 Talk Permit。读取时不写存储。

## 已安装的构建环境

* 用户目录：`/home/yuhui/.local/opt/nxp/mcuxpressoide-24.12.148`。
* 启动命令：`/home/yuhui/.local/bin/mcuxpressoide-24.12`；已添加用户桌面启动项。
* 从用户授权的 NXP 下载链接取得 24.12.148 安装包，通过内嵌 MD5 校验；另记录
  SHA256 `34b9163869d9d274ddcc2b2482d51d9aa2bf5f1cf0581b885172d4c107c58ed5`。
* 解包官方 Debian 包到用户目录，随附 Arm GNU 13.2.1、Redlib 和 JVM 可运行。
  没有运行系统安装脚本、安装 USB/J-Link/LinkServer/刷写驱动或更改系统 udev。
* Eclipse 部分导入/构建插件仍要求显示服务，已在用户目录补充 Ubuntu Xvfb 依赖。
  构建使用独立虚拟显示器，未访问用户桌面。
* IDE 日志提示缺 K22 SDK part-support 描述，并采用 generic cm4 描述。项目自身已
  包含构建所需 SDK 源码，不需要替换驱动。实际命令验证 CPU_MK22FN512VLL12、
  Cortex-M4 hard-float、Redlib、原 firmware.ld；不是由 generic 描述生成链接脚本。
  MCU-specific 调试器设备支持未安装，本任务也未使用调试/刷写。
* 导入时 IDE 为 active GD-77 preset 的 Heap and Stack placement 填入 Default；
  [project-audit.json](validation/talk-permit/native/talk-permit/project-audit.json)
  记录其余 692 个节点不变，实际堆栈布局由原手工 linker script 验证。构建后恢复
  原始工程文件和两个 helper 的临时执行权限，不把 IDE 自动保存内容带入源码提交。

[安装记录](validation/talk-permit/native/installation.json) 不含临时下载鉴权参数。
旧的 25.6 安装包没有用于本轮构建。

## native 构建与固定 ZIP 流程

`tools/build_native_gd77.py` 直接调用 MCUXpresso 的官方 GD-77 managed build preset，
使用源工程自己的逐文件配置、Redlib、heap_4、原 linker script，检查无 -flto。
原工程 post-build 仅执行 size/objcopy/codec_cleaner，FirmwareLoader 是注释。
工具检查成功日志、鲜新输出、源码版本标记，然后自动执行镜像检查与 CPS ZIP 打包。
没有手工重新拼一套 GCC 参数，也不借用此前 newlib 配方。

```sh
# 功能版：一条命令完成 native 构建、校验、CPS ZIP
python3 tools/build_native_gd77.py --out artifacts/native-talk-permit \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'

# 已保留的无功能改动基线工作树
python3 tools/build_native_gd77.py \
  --source artifacts/native-official-baseline/source \
  --out artifacts/native-official-baseline --name OpenGD77-Official-Base \
  --donor-zip '/home/yuhui/Downloads/2021-01-26 GD-77 CPS & Firmware Changelog - Ham Version.zip'
```

另一环境需要自行安装同样的 IDE，使用 `--ide-root`、`--headless-deps` 指向其路径。
基线工作树来源 `aac8ae0`，新克隆可用 `git worktree add --detach PATH aac8ae0` 重建。
脚本保留原生构建时间宏，不承诺跨时间逐字节一致；每次交付以生成的 manifest 为准。

`check_native_gd77.py` 验证实际 ELF32/LOAD、BIN 逐节内容、硬浮点 ABI、vector/SP/Reset、
FreeRTOS/PIT/SAI 中断、Flash/RAM/heap/ISR 栈、上传上限和固定 codec 占位段。
原 linker script 的 codec 输出节含前置 padding，校验允许这种官方布局，但严格检查
0x4400/0xF00 和 0x54000/0x28090 的真实占位字节及应用代码不重叠。
ZIP 根目录为 OpenGD77.bin + 18 个匹配 GLA；CRC、GLA 签名/大小/版本和 BIN 不变性
检查通过后才交付。donor 只在忽略目录中独立提取，固定 SHA256/长度/ZIP CRC 校验。
不要将带空 codec 段的 BIN/AXF 用于 raw SWD；CPS 必须合并 donor。

## 本轮产物与证据

两套 BIN 都是 502768 字节，SP=0x20000000、Reset=0x4191；长度/入口与官方包一致。
**重建基线与官方发布 BIN 不逐字节相同，不能由此替代实机验收。**

| 构建 | 源码提交 | CPS ZIP（本地） | codec2 前 / 上传末尾余量 |
|---|---|---|---|
| baseline | `aac8ae0` | `artifacts/native-official-baseline/OpenGD77-Official-Base.zip` | 3720 / 1040 字节 |
| talk-permit | `f610134` | `artifacts/native-talk-permit/OpenGD77-TalkPermit.zip` | 2848 / 1040 字节 |

**baseline**：ZIP 227932 字节。

BIN SHA256：`af6f02c3ea5e8f1e9dc5f2565f17816a6ab489640212bd59c8492397f55b2f1e`。

ZIP SHA256：`d55d21084b696ba1f187becc707401f2a0edca0c4445b53f7d2934a6f4509caf`。

**talk-permit**：ZIP 228580 字节。

BIN SHA256：`62c19b6b0de3ddfa31cf71c1ba0634a1f36dc0e064a8e56f76703862cfec48eb`。

ZIP SHA256：`c9d7af70290e58d926074002c60ca785106bbc1c1d540a53afc0acb0520cd6b0`。

* native 两套构建：**Pass**，各 0 errors / 4 warnings；四条 #warning 来自未改动的
  官方 soundBeepTaskFunction。首次无 DISPLAY 的失败记录仅为环境排查，不当作构建成功。
* ELF/BIN/ZIP 检查及破坏测试：**Pass**，拒绝损坏向量/中断/codec/数据、截断/追加和
  丢失 ALLOC。Redlib C heap 与 ISR stack 均为 4096 字节；FreeRTOS heap_4
  数组仍为 20480 字节，原任务栈配置不改。
* 真实 Talk Permit helper 的主机/UBSan/-O2/-flto 对照、四平台语法：前阶段 **Pass**；
  主机优化开关不加进固件。证据 [host.log](validation/talk-permit/host.log)。
* 用本次 IDE 随附编译器重新比较官方源与功能分支的 57 个设置字段，两种 enum ABI
  都 **Pass**：[settings-abi.log](validation/talk-permit/native/settings-abi.log)。
  该探针仅作布局比较，不是把普通 GCC/newlib 产物当作 native 固件。
* 官方源差异检查：**Pass**，除了五个固件增量和 README，其余官方文件内容保持一致，
  两份工程 metadata 在工作树中逐字节保留：[official-diff.json](validation/talk-permit/native/official-diff.json)。
* 新 native 基线及功能版的实际启动、按键音、Talk Permit、MIC 恢复、RF 不串音、
  DMR、CPS/设置持久化：**Not Run**。没有连接或操作电台。

[构建汇总](validation/talk-permit/native/summary.json)、
[baseline 日志](validation/talk-permit/native/baseline-build.log)、
[Talk Permit 日志](validation/talk-permit/native/talk-build.log)、
[安装与编译器记录](validation/talk-permit/native/installation.json)。
源码提交 f610134 是本轮已交付 BIN 内的 Git 版本；后续 native 构建/校验脚本提交为 75847cd，
不改变这五个固件增量。源码、安装包、生成固件和 donor 的边界分别记录，不上传私有产物。

## 人工验收顺序

1. CPS E2026.05.26.01 选择 GD-77，先单独选择本地 GD-77_V4.3.6.sgl 为 donor，再选择
   OpenGD77-Official-Base.zip。人工测试启动、CPS 读写、原按键音、FM 语音。
   **如果基线按键音仍无声，先停止新增功能验收，回报基线结果。**
2. 只有原样 native 基线正常后，再选择 OpenGD77-TalkPermit.zip，仍需同一 donor。
   先确认按键音和 FM 语音，再测试 Sound Options → Talk Permit On/Off、确认/取消/
   保存/断电保持，Prompt=Beep、信道允许 beep。
3. 普通手动 FM、On 时本机应有三声标称 940 Hz ×40 ms、10 ms 间隔，总旋律 140 ms。
   验证提示期间 MIC 静音、结束排空后恢复。用独立接收设备确认三声没有进入 RF。
4. 测 Off、提示期间松键、快速重按、TOT、1750/DTMF 抢占、VOX/APRS/卫星/DMR 排除，
   以及 FM RX、DMR、CPS/Codeplug 和校准兼容。保留备份，不清空或重置电台数据。
5. 官方发布 R20260131 仍是已知稳定回退固件；本地 native baseline 是诊断对照，
   不替代用户已验证的发布包，也不把它的结果搬到本地构建上。
