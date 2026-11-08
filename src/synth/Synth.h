#pragma once

#include "Patch.h"
#include "Voice.h"

#include <cstdint>

namespace synth {

// ------------------------------------------------------------
// Synth = 复音管理器
//
// 负责三件事：
//   1. 分配 Voice（按下键时找一个空闲的）
//   2. 回收 Voice（复音不够时"偷"一个，并做淡出）
//   3. 把所有 Voice 混成一路输出
// ------------------------------------------------------------
class Synth {
public:
    static constexpr int kMaxVoices = 16;

    void setSampleRate(float sr);

    void noteOn(int midiNote, float velocity, const Patch& patch);
    void noteOff(int midiNote);
    void allNotesOff();

    // 渲染 frames 个采样点，写入 out（单声道）
    void render(float* out, int frames);

    int activeVoices() const;

private:
    Voice* allocate();

    Voice    voices_[kMaxVoices];
    uint32_t ageCounter_ = 0;
    float    masterGain_ = 0.8f;
};

} // namespace synth
