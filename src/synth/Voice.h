#pragma once

#include "Envelope.h"
#include "Filter.h"
#include "Oscillator.h"
#include "Patch.h"

#include <cstdint>

namespace synth {

// ------------------------------------------------------------
// Voice = 一个"正在响的音符"
//
// 一个 Voice 内部包含：两个振荡器 + 一个滤波器 + 两个包络。
// 16 个 Voice 同时工作，就是 16 复音。
//
// 按下键时 noteOn()，松开时 noteOff()，
// 每个采样点调用一次 render() 把声音累加到输出缓冲区。
// ------------------------------------------------------------
class Voice {
public:
    void setSampleRate(float sr);

    void noteOn(int midiNote, float velocity, const Patch& patch, uint32_t age);
    void noteOff();

    bool isActive() const { return !ampEnv_.isIdle(); }
    bool isReleasing() const { return ampEnv_.isReleasing(); }
    float level() const { return ampEnv_.level(); }
    uint32_t age() const { return age_; }
    int note() const { return note_; }

    // 把声音"加"到 out[0..frames) 上（不清零，方便多个 Voice 混音）
    void render(float* out, int frames);

private:
    static float noteToHz(int midiNote) {
        return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }

    Oscillator osc1_;
    Oscillator osc2_;
    Envelope   ampEnv_;   // 控制音量
    Envelope   fltEnv_;   // 控制滤波器截止频率
    SvfFilter  filter_;

    Patch    patch_;
    int      note_     = 60;
    float    velocity_ = 1.0f;
    uint32_t age_      = 0;
    float    sampleRate_ = 48000.0f;
};

} // namespace synth
