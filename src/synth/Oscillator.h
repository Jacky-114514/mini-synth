#pragma once

#include <cmath>

namespace synth {

// 波形类型：锯齿、方波、正弦。
// 锯齿波包含全部谐波，是减法合成的"原材料"。
enum class Wave { Saw, Square, Sine };

// ------------------------------------------------------------
// 振荡器（Oscillator）
//
// 本质就是你熟悉的 DDS：
//   phase_  —— 相位累加器，范围 0.0 ~ 1.0，转一圈就是一个周期
//   increment_ —— 频率控制字 = 频率 / 采样率
//
// 直接用 2*phase-1 生成锯齿会在高音区产生"混叠"（刺耳的杂音），
// 所以这里用 PolyBLEP 在波形跳变处做一小段平滑，成本很低。
// ------------------------------------------------------------
class Oscillator {
public:
    void setSampleRate(float sr) {
        sampleRate_ = (sr > 0.0f) ? sr : 48000.0f;
        recalc();
    }

    void setFrequency(float hz) {
        frequency_ = hz;
        recalc();
    }

    void setWave(Wave w) { wave_ = w; }

    // 重置相位。略微错开两个振荡器的相位，声音会更"厚"。
    void reset(float phase = 0.0f) { phase_ = phase; }

    float process() {
        const float dt = increment_;
        float value = 0.0f;

        switch (wave_) {
        case Wave::Sine:
            value = std::sin(2.0f * kPi * phase_);
            break;

        case Wave::Saw:
            value = 2.0f * phase_ - 1.0f;
            value -= polyBlep(phase_, dt);
            break;

        case Wave::Square:
            value = (phase_ < 0.5f) ? 1.0f : -1.0f;
            value += polyBlep(phase_, dt);
            value -= polyBlep(wrap(phase_ + 0.5f), dt);
            break;
        }

        // 相位累加器前进一格，溢出就绕回 0
        phase_ += dt;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        return value;
    }

private:
    static constexpr float kPi = 3.14159265358979f;

    void recalc() {
        increment_ = frequency_ / sampleRate_;
        // 超过 Nyquist 的频率没有意义，钳一下防止数值失控
        if (increment_ > 0.5f) increment_ = 0.5f;
    }

    static float wrap(float p) { return (p >= 1.0f) ? p - 1.0f : p; }

    // PolyBLEP：在跳变点附近加一个二次修正，抵消掉高频混叠
    static float polyBlep(float t, float dt) {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt) {              // 刚跳过 0 点
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt) {       // 即将跳到 1 点
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float sampleRate_ = 48000.0f;
    float frequency_  = 440.0f;
    float increment_  = 440.0f / 48000.0f;
    float phase_      = 0.0f;
    Wave  wave_       = Wave::Saw;
};

} // namespace synth
