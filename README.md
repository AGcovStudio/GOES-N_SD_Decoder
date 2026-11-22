# GOES-N SD Decoder

Baseband playback decoder for the **Sensor Data (SD)** downlink of the GOES-N/O/P satellites (GOES-13/14/15) at
**1676.0 MHz**: the raw output of the imager and the sounder, before ground processing. It reads a baseband IQ
recording (`.cs8`, `.cu8`, `.cs16`, `.cf32`, `.wav`), demodulates it and writes the imager pictures (visible and
infrared), a false-colour picture and the sounder frames.

It only reads recordings. It has no transmit function and makes no network connections.

Developer: AGcov (BI2QYN)

## The signal

One carrier with two BPSK components in quadrature:

| | Imager | Sounder |
|---|---|---|
| Rate | 2.6208 Mbit/s | 40 kbit/s |
| Coding | NRZ-M | NRZ-M |
| Frame | 480-bit blocks of 48 10-bit words, 14-bit sync `0x2B50`, XOR with a fixed 60-byte sequence | 4224-bit frames after the marker `0x3683A2E6`, 13-stage whitening, odd parity per word |
| Forward error correction | none | none |

A scan (one east-west sweep, 8 visible detector rows and 2 rows per infrared group) is about 5200 image blocks
between long all-zero fills at the turnarounds; one scan cycle takes about 1.15 s.

## What the decoder does

- BPSK demodulator (root-raised-cosine, carrier and symbol tracking) for the imager, and a separate decimated
  path for the sounder on the other axis. Sample rate 3.41 MS/s or more; **6 MS/s recommended**.
- Imager block sync with a soft-weighted sync histogram, de-whitening, and soft bits kept for every block.
- **Soft-decision imaging**: SD is usually received near threshold (sync-bit error rates of 20–30 % still give
  pictures). Each pixel bit plane is decided by a reliability-weighted vote over a small neighbourhood
  (3 rows × 5 samples for the visible channel), after which only isolated dots far from their 3 × 3 median are
  replaced by that median. Untick "soft decision" in the window (or `--hard` on the command line) for plain bit
  decisions.
- Scan detection, **scan-width tolerance** (a different width must repeat for 3 scans before it is accepted;
  single odd scans are dropped and counted), and **scan direction** from the picture itself: each scan is matched
  against the previous one as it is and mirrored, and the absolute east-west orientation of the whole picture comes
  from a soft vote of the header's direction flag.
- Automatic splitting into pictures (time gap, change of scan mode, long blank stretch, scene change, at most
  360 scans per picture).
- Live view while decoding: constellation, spectrum, link statistics and a growing preview.

## Output

`<output folder>\GOES SD\<UTC time>\` per picture. The time is taken from the recording's file name when it has
one (`2026-11-22_02-52-07_6000000SPS_1676000000Hz.cs16`, read as UTC), otherwise it is the time of decoding.

| File | Contents |
|---|---|
| `GOES_SD_VIS.png` | visible channel, 16-bit, raw 10-bit counts × 64 (0 = no data), square pixels |
| `GOES_SD_IR1.png` … `GOES_SD_IR3.png` | infrared detector groups 1–3, 16-bit raw counts, colder = brighter |
| `GOES_SD_VIS_Enhanced.png` | visible, percentile stretch (1 %–99.7 %) with gamma 0.8 |
| `GOES_SD_IR1/3_Enhanced.png` | infrared groups 1 and 3, grey stretch |
| `GOES_SD_IR2_Enhanced.png` | infrared group 2, cloud-top colour ramp |
| `GOES_SD_FalseColor.png` | visible × IR group 2 through the GOES false-colour table |
| `GOES_SD_Sounder.bin` / `.csv` | sounder frames (264 16-bit words each) and their counters / parity |
| `GOES_SD_Observation.json` | scans, blank scans, direction decisions, width changes, link statistics |

**The data are not calibrated** (the SD stream carries no calibration tables), so the enhanced pictures use
percentile stretches. IR group 2 has the highest contrast and is most likely the 10.7 µm window channel; groups 1
and 3 are 13.3 µm and 6.5 µm in some order that has not been confirmed. IR group 4 is constant in the recordings
used here and is not written.

## Stitching a full disc: `tools/stitch_segments.py`

A full disc takes about 25 minutes and comes out as several pictures. The script puts consecutive pictures back
together, shifts every scan so the centre of the Earth disc (midpoint of the two limbs in IR group 2) lies on one
column — this removes the slow east-west drift of the scan start and makes the disc round —, fills empty scans by
interpolation from the scans above and below, optionally applies a 3 × 3 median filter, and writes the same set of
products for the whole disc.

```
pip install numpy pillow
python tools/stitch_segments.py "<output folder>/GOES SD" -o full_disc --median
```

`--time-gaps` leaves empty scans for time gaps between pictures, worked out from their start times.

## Build (Windows, MinGW)

You need Qt 6 (MinGW 64-bit), CMake and the MinGW toolchain from the Qt installer. No other libraries.

1. Edit the three paths at the top of `scripts/build_release.bat` if Qt lives somewhere else.
2. Run `scripts\build_release.bat`. The program ends up in `dist\`; if Inno Setup 6 is installed, the installer is
   made in `installer\output\`.

Or by hand (also on Linux):

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64
cmake --build build --parallel 4
```

A command-line decoder without the window is built as well:

```
goessd_cli <recording> [sample rate in Hz] [output folder] [--hard]
```

## Publishing

`upload_to_github.bat` (Windows, needs Git) commits the folder and pushes it to GitHub; it asks you to sign in
the first time and stores no password itself. Edit `GH_OWNER` / `GH_REPO` at its top if needed.

## Using it

Drop the recording into the window or browse for it, choose the output folder and press *Start*.
`.cs8` / `.cu8` / `.cs16` / `.cf32` files have no sample-rate field: the program looks for `rate <Hz>` in a `.txt`
next to the file, then for `6000000SPS` / `6Msps` in the file name, then uses the value you type. WAV files carry it
in the header. The recording should be centred on 1676.0 MHz; small frequency offsets are tracked by the carrier loop.

A small log is kept in `Documents\GOES-N_SD_Decoder\runtime.log`.

## Licence

GNU General Public License v3.0 or later, see `LICENSE`. Copyright (c) 2026 AGcov (BI2QYN).

The false-colour table `src/gvar_fc_lut.inc` is SatDump's `lut/goes/gvar/fc_lut.png` (GPL-3.0). The SD frame format
was worked out from the GOES-N data book, SatDump's `goesn_sd` module and recordings; the decoder is an independent
implementation. Qt is used under the LGPL v3 (dynamically linked). See `installer/THIRD_PARTY_NOTICES.txt`.

This is an amateur project. It is not affiliated with NOAA, NASA or the SatDump project, and it is not a weather
service. Do not use it where a wrong picture could cause harm. Read `installer/EULA.txt`.

---

## 简介（中文）

GOES-N/O/P（GOES-13/14/15）**1676.0 MHz 传感器数据（Sensor Data，SD）** 的基带回放解码器。SD 是成像仪和探测器
（Sounder）在地面处理之前的原始数据。输入基带录音（`.cs8` `.cu8` `.cs16` `.cf32` `.wav`，建议 6 MS/s），输出成像仪
可见光、红外原始图（16 位计数）、增强图、假彩色图，以及探测器帧。只读录音，不发射，不联网。

- 成像仪 2.6208 Mbit/s、探测器 40 kbit/s，同一载波上正交的两路 BPSK，NRZ-M，无纠错。
- 弱信号用软判决成像：按比特可靠度在邻域内投票，再只把孤立坏点换成 3×3 中值；同步位误码率 20–30% 仍能出图。
- 容忍扫描宽度变化（新宽度需连续 3 条才接受，孤立异常条丢弃）；扫描方向由图像本身匹配判定，整幅的东西朝向由
  头部方向位的软投票决定。
- 自动分图（中断、模式变化、长时间无图像、场景突变，单幅最多 360 条扫描）。
- 数据**未定标**。红外组 2 对比度最高，应为 10.7 µm 窗区；组 1、组 3 是 13.3 µm 和 6.5 µm，对应关系未确认；组 4 恒定，不输出。
- 输出文件夹按录音文件名中的时间（按 UTC）命名，没有时则用解码时间。
- `tools/stitch_segments.py`（只需 numpy 和 Pillow）把一次过境的多幅图拼成整盘：按红外组 2 中左右地球边缘的中点逐条对齐
  扫描（消除扫描起点漂移，圆盘变圆），补空白扫描，可选 3×3 中值滤波（`--median`）。

构建：Qt 6 + CMake（Windows 用 Qt 安装器里的 MinGW），运行 `scripts\build_release.bat`，或按上面的 cmake 命令。
命令行版：`goessd_cli <录音> [采样率 Hz] [输出文件夹] [--hard]`。

许可：GPL-3.0-or-later。假彩色查找表来自 SatDump（GPL-3.0）。业余项目，与 NOAA、NASA、SatDump 无关，不是气象业务系统。
