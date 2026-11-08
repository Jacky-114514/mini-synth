#pragma once

#include <algorithm>
#include <cmath>

namespace synth {

// ------------------------------------------------------------
// 12 dB/oct 低通滤波器（TPT 状态变量滤波器）
//
// 对应电路里的 RC 低通 + 反馈，只不过这里的截止频率
// 每采样点都能改，所以能被包络和 LFO "扫"起来。
//
//   cutoffHz  ：截止频率，超过它的高频会被压下去
//   resonance ：共振 0~1，越大截止点附近的"包"越高，声音越"哇"
// ------------------------------------------------------------
class SvfFilter {
public:
    void setSampleRate(float sr) {
        sampleRate_ = (sr > 0.0f) ? sr : 48000.0f;
    }

    void set(float cutoffHz, float resonance) {
        cutoffHz = std::clamp(cutoffHz, 20.0f, sampleRate_ * 0.45f);

        // resonance 0..1 映射到 Q 0.7..12
        const float q = 0.7f + 11.3f * std::clamp(resonance, 0.0f, 0.99f);

        g_ = std::tan(3.14159265358979f * cutoffHz / sampleRate_);
        k_ = 1.0f / q;
    }

    void reset() { ic1_ = 0.0f; ic2_ = 0.0f; }

    float processLowpass(float in) {
        const float a1 = 1.0f / (1.0f + g_ * (g_ + k_));
        const float a2 = g_ * a1;
        const float a3 = g_ * a2;

        const float v3 = in - ic2_;
        const float v1 = a1 * ic1_ + a2 * v3;
        const float v2 = ic2_ + a2 * ic1_ + a3 * v3;

        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;

        return v2;   // 低通输出
    }

private:
    float sampleRate_ = 48000.0f;
    float g_ = 0.1f;
    float k_ = 1.0f;
    float ic1_ = 0.0f;
    float ic2_ = 0.0f;
};

} // namespace synth
