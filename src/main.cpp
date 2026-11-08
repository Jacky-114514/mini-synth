// ============================================================
//  mini-synth 命令行入口
//
//  两种用法：
//    1) 演示曲：  ./build/mini-synth --demo --play
//    2) 弹一串音：./build/mini-synth --score "C4:0.5 E4:0.5 G4:1.0" --play
//
//  再加 --cutoff / --res / --attack ... 就能听出每个参数的作用。
// ============================================================

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "demo/Demo.h"
#include "io/Playback.h"
#include "io/WavWriter.h"
#include "synth/Synth.h"

namespace {

constexpr int kSampleRate = 48000;
constexpr int kBlock      = 64;   // 一次渲染 64 个采样点（约 1.3 ms）

// ------------------------------------------------------------
// 事件：某一时刻按下或松开某个音
// ------------------------------------------------------------
struct TimedEvent {
    float        time = 0.0f;
    bool         on   = true;
    int          note = 60;
    float        velocity = 1.0f;
    synth::Patch patch;
};

struct Options {
    bool        demoMode = true;
    bool        play     = false;
    std::string score;
    std::string out = "build/out.wav";
    synth::Patch patch;
};

// 试音用的默认音色（一个有点亮的 Lead）
synth::Patch makeLeadPatch() {
    synth::Patch p;
    p.wave1 = synth::Wave::Saw;
    p.wave2 = synth::Wave::Square;
    p.detuneCents = 5.0f;
    p.mix2 = 0.4f;
    p.cutoffHz = 1800.0f;
    p.resonance = 0.35f;
    p.filterEnvAmount = 0.5f;
    p.ampAttack = 0.01f;
    p.ampDecay = 0.25f;
    p.ampSustain = 0.60f;
    p.ampRelease = 0.35f;
    p.fltAttack = 0.005f;
    p.fltDecay = 0.30f;
    p.fltSustain = 0.20f;
    p.fltRelease = 0.30f;
    p.gain = 0.30f;
    return p;
}

void printUsage() {
    std::cout <<
        "mini-synth —— 一个用来学合成器原理的最小合成器\n"
        "\n"
        "用法：\n"
        "  mini-synth [选项]\n"
        "\n"
        "模式：\n"
        "  --demo                 渲染内置演示曲（默认）\n"
        "  --score \"C4:0.5 E4:0.5 G4:1.0\"\n"
        "                         按顺序弹出一串音，冒号后是时长（秒）\n"
        "\n"
        "输出：\n"
        "  --out <文件>           输出 WAV 路径，默认 build/out.wav\n"
        "  --play                 生成后交给 Windows 播放\n"
        "\n"
        "音色参数（只对 --score 生效）：\n"
        "  --wave saw|square|sine 波形\n"
        "  --cutoff <Hz>          滤波器截止频率，越小越闷\n"
        "  --res <0..1>           共振，越大越\"哇\"\n"
        "  --attack <秒>          音量包络的攻击时间\n"
        "  --decay <秒>           音量包络的衰减时间\n"
        "  --sustain <0..1>       音量包络的持续电平\n"
        "  --release <秒>         音量包络的释放时间\n"
        "  --detune <音分>        两个振荡器的失谐量\n"
        "  --drive <0..1>         滤波器包络深度\n"
        "  --gain <0..1>          这个音色的电平\n"
        "\n"
        "例子：\n"
        "  mini-synth --demo --play\n"
        "  mini-synth --score \"A3:0.4 C4:0.4 E4:0.4 A4:1.2\" --cutoff 600 --play\n"
        "  mini-synth --score \"C2:0.3 C2:0.3 G2:0.6\" --wave saw --cutoff 200 --play\n";
}

synth::Wave parseWave(const std::string& s) {
    if (s == "saw")    return synth::Wave::Saw;
    if (s == "square") return synth::Wave::Square;
    if (s == "sine")   return synth::Wave::Sine;
    std::cerr << "未知波形：" << s << "（可选 saw / square / sine）\n";
    std::exit(1);
}

// "C4" -> 60，"A#3" -> 58，"Eb2" -> 39
int noteNameToMidi(const std::string& name) {
    if (name.size() < 2) return -1;

    int semitone = -1;
    switch (std::toupper(static_cast<unsigned char>(name[0]))) {
    case 'C': semitone = 0;  break;
    case 'D': semitone = 2;  break;
    case 'E': semitone = 4;  break;
    case 'F': semitone = 5;  break;
    case 'G': semitone = 7;  break;
    case 'A': semitone = 9;  break;
    case 'B': semitone = 11; break;
    default: return -1;
    }

    std::size_t i = 1;
    if (i < name.size() && name[i] == '#') { ++semitone; ++i; }
    else if (i < name.size() && name[i] == 'b') { --semitone; ++i; }

    if (i >= name.size() || !std::isdigit(static_cast<unsigned char>(name[i]))) return -1;
    const int octave = name[i] - '0';

    return (octave + 1) * 12 + semitone;
}

std::vector<TimedEvent> buildScoreEvents(const std::string& score,
                                         const synth::Patch& patch,
                                         float& length) {
    std::vector<TimedEvent> events;

    float t = 0.0f;
    std::size_t pos = 0;
    while (pos < score.size()) {
        while (pos < score.size() && std::isspace(static_cast<unsigned char>(score[pos]))) ++pos;
        if (pos >= score.size()) break;

        std::size_t end = pos;
        while (end < score.size() && !std::isspace(static_cast<unsigned char>(score[end]))) ++end;
        const std::string token = score.substr(pos, end - pos);
        pos = end;

        const std::size_t colon = token.find(':');
        if (colon == std::string::npos) {
            std::cerr << "格式错误，应为 音名:时长，例如 C4:0.5，实际是：" << token << "\n";
            std::exit(1);
        }

        const int note = noteNameToMidi(token.substr(0, colon));
        if (note < 0) {
            std::cerr << "看不懂这个音名：" << token.substr(0, colon) << "\n";
            std::exit(1);
        }

        const float duration = std::stof(token.substr(colon + 1));
        if (duration <= 0.0f) {
            std::cerr << "时长必须大于 0：" << token << "\n";
            std::exit(1);
        }

        events.push_back({t, true, note, 0.9f, patch});
        events.push_back({t + duration * 0.95f, false, note, 0.0f, patch});
        t += duration;
    }

    length = t + 2.0f;   // 留出释放尾巴
    return events;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    opt.patch = makeLeadPatch();

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];

        auto needValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << name << " 后面需要一个参数\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (a == "--help" || a == "-h") {
            printUsage();
            return 0;
        } else if (a == "--demo") {
            opt.demoMode = true;
        } else if (a == "--score") {
            opt.score = needValue("--score");
            opt.demoMode = false;
        } else if (a == "--out") {
            opt.out = needValue("--out");
        } else if (a == "--play") {
            opt.play = true;
        } else if (a == "--wave") {
            opt.patch.wave1 = parseWave(needValue("--wave"));
            opt.patch.wave2 = opt.patch.wave1;
        } else if (a == "--cutoff") {
            opt.patch.cutoffHz = std::stof(needValue("--cutoff"));
        } else if (a == "--res") {
            opt.patch.resonance = std::stof(needValue("--res"));
        } else if (a == "--attack") {
            opt.patch.ampAttack = std::stof(needValue("--attack"));
        } else if (a == "--decay") {
            opt.patch.ampDecay = std::stof(needValue("--decay"));
        } else if (a == "--sustain") {
            opt.patch.ampSustain = std::stof(needValue("--sustain"));
        } else if (a == "--release") {
            opt.patch.ampRelease = std::stof(needValue("--release"));
        } else if (a == "--detune") {
            opt.patch.detuneCents = std::stof(needValue("--detune"));
        } else if (a == "--drive") {
            opt.patch.filterEnvAmount = std::stof(needValue("--drive"));
        } else if (a == "--gain") {
            opt.patch.gain = std::stof(needValue("--gain"));
        } else {
            std::cerr << "未知参数：" << a << "\n\n";
            printUsage();
            return 1;
        }
    }

    if (!opt.demoMode && opt.score.empty()) {
        std::cerr << "--score 不能为空\n";
        return 1;
    }

    // ---------- 1. 准备事件列表 ----------
    std::vector<TimedEvent> events;
    float length = 0.0f;

    if (opt.demoMode) {
        const demo::Data d = demo::make();
        length = d.length;
        events.reserve(d.events.size());
        for (const auto& e : d.events) {
            events.push_back({e.time, e.on, e.note, e.velocity, e.patch});
        }
    } else {
        events = buildScoreEvents(opt.score, opt.patch, length);
    }

    // 按时间排序；同一时刻先处理"松开"，再处理"按下"
    std::stable_sort(events.begin(), events.end(),
                     [](const TimedEvent& x, const TimedEvent& y) {
                         if (x.time != y.time) return x.time < y.time;
                         return (!x.on && y.on);
                     });

    // ---------- 2. 打开输出 ----------
    const std::filesystem::path outPath(opt.out);
    if (outPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    synth::WavWriter wav;
    if (!wav.open(opt.out, kSampleRate, 1)) {
        std::cerr << "无法写入文件：" << opt.out << "\n";
        return 1;
    }

    // ---------- 3. 渲染 ----------
    synth::Synth synth;
    synth.setSampleRate(kSampleRate);

    const long totalFrames = static_cast<long>(length * kSampleRate);
    std::vector<float> block(kBlock);

    std::size_t nextEvent = 0;
    int    maxVoices  = 0;
    float  peak       = 0.0f;
    double sumSquares = 0.0;

    for (long i = 0; i < totalFrames; i += kBlock) {
        const int frames = static_cast<int>(std::min<long>(kBlock, totalFrames - i));
        const float t = static_cast<float>(i) / kSampleRate;

        while (nextEvent < events.size() && events[nextEvent].time <= t) {
            const TimedEvent& e = events[nextEvent];
            if (e.on) synth.noteOn(e.note, e.velocity, e.patch);
            else      synth.noteOff(e.note);
            ++nextEvent;
        }

        synth.render(block.data(), frames);
        wav.write(block.data(), frames);

        maxVoices = std::max(maxVoices, synth.activeVoices());
        for (int k = 0; k < frames; ++k) {
            peak = std::max(peak, std::fabs(block[k]));
            sumSquares += static_cast<double>(block[k]) * block[k];
        }
    }
    wav.close();

    // ---------- 4. 报告 ----------
    const double rms = std::sqrt(sumSquares / static_cast<double>(std::max<long>(1, totalFrames)));

    std::cout << "模式      : " << (opt.demoMode ? "内置演示曲" : "自定义音符")
              << "\n采样率    : " << kSampleRate << " Hz\n"
              << "时长      : " << length << " 秒\n"
              << "复音峰值  : " << maxVoices << " / " << synth::Synth::kMaxVoices << "\n"
              << "峰值      : " << peak << "（应小于 1.0，否则会削顶）\n"
              << "RMS       : " << rms << "\n"
              << "输出      : " << opt.out << "\n";

    if (!opt.demoMode) {
        std::cout << "音色参数  : wave=" << static_cast<int>(opt.patch.wave1)
                  << " cutoff=" << opt.patch.cutoffHz
                  << " res=" << opt.patch.resonance
                  << " attack=" << opt.patch.ampAttack
                  << " detune=" << opt.patch.detuneCents << "\n";
    }

    // ---------- 5. 播放 ----------
    if (opt.play) {
        std::cout << "正在通过 Windows 播放……\n";
        if (!synth::playWavOnWindows(opt.out)) {
            std::cout << "自动播放没成功，请手动打开：" << opt.out << "\n";
        }
    }

    return 0;
}
