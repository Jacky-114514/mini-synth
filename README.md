# mini-synth

一个用来**搞懂合成器原理**的最小项目。

它是一台 16 复音的减法合成器：两个振荡器 → 一个低通滤波器 → 两个 ADSR 包络 → 混音 → 软限幅。
代码量不大，每个文件只干一件事，注释按"给第一次接触合成器的人"来写。

目前它能：

- 渲染一段 11.6 秒的演示曲（Am–F–C–G 和弦进行，Pad / Bass / Pluck 三层音色）
- 按你给的音符串合成音频，比如 `C4:0.5 E4:0.5 G4:1.0`
- 通过命令行改音色参数，立刻听出滤波器、包络、失谐的作用
- 输出标准 16 bit / 48 kHz 的 WAV

---

## 1. 快速开始

```bash
cd /home/jacky/mini-synth
make            # 编译，生成 build/mini-synth
make demo       # 生成 build/demo.wav 并调用 Windows 播放
```

编译只要 g++，没有任何第三方依赖。

想自己弹一串音：

```bash
# 音名:时长（秒），空格分隔
./build/mini-synth --score "C4:0.4 E4:0.4 G4:0.8" --play

# 同一个音，把滤波器关小，声音立刻变闷
./build/mini-synth --score "A3:1.2" --cutoff 300 --play

# 再把滤波器开大，变亮
./build/mini-synth --score "A3:1.2" --cutoff 8000 --play
```

`--play` 会调用 Windows 的播放器；不加就只生成 WAV 文件。

---

## 2. 为什么在 WSL 里要"借 Windows 播放"

WSL 本身**没有声卡设备**（`/dev/snd` 里只有一个 timer），所以 Linux 程序没法直接出声。

这个项目的做法是：

1. 先把音频算好，写成标准的 `.wav` 文件；
2. 再用 `wslpath` 把路径转换成 Windows 路径；
3. 最后 `fork + exec` 调用 `powershell.exe`，用 .NET 的 `SoundPlayer` 播放。

这段代码在 `src/io/Playback.cpp`，只跟"播放"有关，和合成算法完全无关。
**搬到 RK3562 上时，把这一层换成 ALSA 就行**（见第 8 节），DSP 代码一行都不用改。

---

## 3. 目录结构

```
mini-synth/
├── Makefile
├── README.md
├── src/
│   ├── main.cpp              命令行入口、参数解析、渲染循环
│   ├── synth/                ← 纯算法，不依赖任何操作系统
│   │   ├── Oscillator.h      振荡器（PolyBLEP 抗混叠）
│   │   ├── Envelope.h        ADSR 包络
│   │   ├── Filter.h          12 dB 低通滤波器
│   │   ├── Patch.h           一套音色参数（预设）
│   │   ├── Voice.h/.cpp      一个"正在响的音符"
│   │   └── Synth.h/.cpp      16 复音管理 + 混音
│   ├── io/
│   │   ├── WavWriter.h/.cpp  写 WAV 文件
│   │   └── Playback.h/.cpp   调 Windows 播放
│   └── demo/
│       └── Demo.h/.cpp       内置演示曲
└── build/                    编译产物（make 生成）
```

`src/synth/` 是**可以整体搬走**的：它不 include 任何系统头文件，只依赖标准 C++。

---

## 4. 一个音是怎么发出来的

```
按下 C4
   │
   ▼
Synth::noteOn()          找一个空闲 Voice（没有就偷一个）
   │
   ▼
Voice::noteOn()          设置频率、重置相位、启动两个包络
   │
   ▼
每个采样点（1/48000 秒）：
   振荡器1 ┐
           ├─ 相加 → 滤波器 → × 音量包络 → 累加到混音总线
   振荡器2 ┘              ▲
                          └── 滤波器包络在这里推动截止频率
   │
   ▼
Synth::render()          所有 Voice 相加 → tanh 软限幅 → 输出
```

对应的核心代码就这几行（`Voice::render`）：

```cpp
float s = osc1_.process() + p.mix2 * osc2_.process();   // 两个振荡器混合
s *= oscGain;

const float fenv   = fltEnv_.process();                  // 滤波器包络
const float cutoff = p.cutoffHz * std::exp2(p.filterEnvAmount * fenv * 4.0f);
filter_.set(cutoff, p.resonance);
s = filter_.processLowpass(s);                           // 滤波

const float aenv = ampEnv_.process();                    // 音量包络
out[i] += s * aenv * velAmp * p.gain;                    // 累加，不是覆盖
```

注意最后是 `+=`：每个 Voice 都往同一个缓冲区里**加**自己的声音，这就是"混音"。

---

## 5. 五个零件，五种参数

| 零件 | 管什么 | 代码 | 参数 |
|---|---|---|---|
| 振荡器 | 音高、基础音色 | `Oscillator.h` | 波形、失谐 |
| 滤波器 | 亮度、厚薄 | `Filter.h` | `cutoff`、`res` |
| 音量包络 | 音符的生老病死 | `Envelope.h` | A / D / S / R |
| 滤波器包络 | 音色的"哇" | `Envelope.h` | A / D / S / R + 深度 |
| 混音/限幅 | 不爆音 | `Synth.cpp` | 总线增益 |

**记住一句话**：振荡器决定"是什么乐器"，包络和滤波器决定"怎么演奏它"。

---

## 6. 参数实验：改一个数，听一个变化

在项目目录下逐条试，每次只改一个参数：

| 想听到 | 命令 | 应该听到 |
|---|---|---|
| 变闷 | `--cutoff 250` | 像隔着门听，低频还在，高频没了 |
| 变亮 | `--cutoff 8000` | 刺、有"齿音" |
| 加共振 | `--res 0.9 --cutoff 800` | 截止点附近"呜"地拎起来，像哇音 |
| 慢起音 | `--attack 1.5` | 声音慢慢"涌"出来，像弦乐 |
| 断奏 | `--decay 0.12 --sustain 0` | 每个音"哒"一下就没了 |
| 长释放 | `--release 2.0` | 松手后还拖很久 |
| 变厚 | `--detune 25` | 两个振荡器打架，声音变宽、轻微晃动 |
| 方波 | `--wave square` | 比锯齿更"空"、更像游戏机 |
| 正弦 | `--wave sine` | 很纯，像口哨，几乎没有泛音 |

一个完整的对比（先听，再改）：

```bash
./build/mini-synth --score "A3:0.5 C4:0.5 E4:0.5 A4:1.5" --cutoff 400  --res 0.2 --play
./build/mini-synth --score "A3:0.5 C4:0.5 E4:0.5 A4:1.5" --cutoff 3000 --res 0.7 --play
```

---

## 7. 演示曲里发生了什么

`src/demo/Demo.cpp` 里只有一张"事件表"：什么时间、按下哪个音、多长、用哪套参数。

- **100 BPM**，每小节 2.4 秒，共 4 小节：Am → F → C → G
- **Pad**：整小节铺底，慢攻击、长释放，`cutoff 1300`
- **Bass**：每小节 8 个八分音符，`cutoff 260`、`sustain 0`，所以每个音都是"点"
- **Pluck**：高八度琶音，`cutoff 2200`、`res 0.5`，滤波器包络很深，所以有"哇"的音头

三层用的是**同一个引擎**，只是 `Patch` 里的数字不同——这就是"音色是参数，不是代码"。

---

## 8. 搬到 RK3562 要改什么

`src/synth/` 和 `src/demo/` 原样保留，只换 I/O：

```cpp
#include <alsa/asoundlib.h>

snd_pcm_t* pcm;
snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE,
                   SND_PCM_ACCESS_RW_INTERLEAVED,
                   1, 48000, 1, 20000);   // 单声道、48k、20ms 缓冲

float   mix[128];
int16_t out[128];
while (running) {
    synth.render(mix, 128);                  // ← 同一个函数
    for (int i = 0; i < 128; ++i) out[i] = (int16_t)(mix[i] * 32767);
    snd_pcm_writei(pcm, out, 128);
}
```

要点：

1. **实时线程**：`SCHED_FIFO` 优先级 80 左右，绑到一个核上。
2. **块大小 128**：48 kHz 下约 2.7 ms，加上缓冲总延迟约 5~10 ms。
3. **不要**在音频线程里 `malloc`、加锁、读文件、`printf`。
4. 参数从控制线程传进来时用双缓冲或环形队列，并在块边界切换。
5. 编译要加 `-lasound`，目标机上装 `libasound2-dev`。

在 RK3562 上，这套 16 复音的引擎占用很小；真正的瓶颈会是你后面加的混响、粒度和采样。

---

## 9. 目前故意没做的部分

这是"第一课"的版本，以下都留给你自己加，顺序建议从易到难：

- [ ] **LFO**：一个 0.1~10 Hz 的振荡器，去摆动 cutoff 或音高（颤音）
- [ ] **滑音 / 单音模式**：让两个音之间平滑过渡
- [ ] **第二个滤波器**：串联或并联，声音更复杂
- [ ] **预设文件**：把 `Patch` 存成 JSON，做音色浏览器
- [ ] **实时键盘**：用 termios 读终端按键，或接 USB MIDI
- [ ] **波表**：把单周期波形做成表，按音高选带限版本
- [ ] **FM**：再加一个"调制器"振荡器去推相位
- [ ] **效果**：合唱、延迟、混响

已知的简化（不影响现在玩，但要知道）：

- **偷 voice 时会有一声轻微的"咔哒"**。专业做法是给被偷的 Voice 做 5 ms 淡出；现在因为 16 个复音够用，演示曲最多只用到 8 个，所以听不到。
- 参数在音符发声期间不会实时变化（每次 noteOn 时复制一份 Patch）。
- 单声道输出。

---

## 10. 想更快建立听感

免费的三个软件，玩一两个晚上，比看十篇文章管用：

- **Vital** —— 波表合成器，波形、包络、滤波器全都可视化
- **Surge XT** —— 开源，减法 + 波表 + FM 都有
- **VCV Rack** —— 模块化，像搭电路一样连线，对做嵌入式的人特别直观

书：Simon Cann 的 *How to Make a Noise*（免费 PDF），讲的就是减法合成这套东西。

---

## 11. 常见问题

**`make` 报错找不到 g++？**
`sudo apt install build-essential`。

**运行了但没声音？**
先确认 `build/demo.wav` 生成了，双击它能不能放。如果不能，问题在播放器；如果能，问题在 `--play` 那段，可以手动打开 WAV。

**播放很慢？**
`--play` 是同步播放，程序会等音频放完才退出，这是正常的。

**想改默认音色？**
`src/main.cpp` 里的 `makeLeadPatch()`，改完 `make` 重新生成。
