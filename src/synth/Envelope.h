#pragma once

#include <algorithm>
#include <cmath>

namespace synth {

// ------------------------------------------------------------
// ADSR 包络
//
// 按下一个键：Attack（爬升）→ Decay（回落到持续电平）→ Sustain（保持）
// 松开键：    Release（衰减到 0）
//
// 对应嵌入式里的"状态机 + 斜坡发生器"，只是斜坡是每采样点更新一次。
// ------------------------------------------------------------
class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(float sr) {
        sampleRate_ = (sr > 0.0f) ? sr : 48000.0f;
        recalc();
    }

    // 时间单位：秒
    void setParams(float attack, float decay, float sustain, float release) {
        attack_  = std::max(attack, 0.001f);
        decay_   = std::max(decay, 0.001f);
        sustain_ = std::clamp(sustain, 0.0f, 1.0f);
        release_ = std::max(release, 0.001f);
        recalc();
    }

    void noteOn() {
        stage_ = Stage::Attack;
    }

    void noteOff() {
        if (stage_ != Stage::Idle) stage_ = Stage::Release;
    }

    bool isIdle() const { return stage_ == Stage::Idle; }
    bool isReleasing() const { return stage_ == Stage::Release; }
    float level() const { return level_; }

    float process() {
        switch (stage_) {
        case Stage::Idle:
            level_ = 0.0f;
            break;

        case Stage::Attack:
            level_ += attackStep_;
            if (level_ >= 1.0f) {
                level_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;

        case Stage::Decay:
            level_ += (sustain_ - level_) * decayCoef_;
            if (std::fabs(level_ - sustain_) < 1e-4f) {
                level_ = sustain_;
                stage_ = Stage::Sustain;
            }
            // sustain 为 0 的打击音色：衰减完就直接结束，把 Voice 让出来
            if (sustain_ < 1e-4f && level_ < 1e-3f) {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;

        case Stage::Sustain:
            level_ = sustain_;
            break;

        case Stage::Release:
            level_ -= level_ * releaseCoef_;
            if (level_ < 1e-4f) {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
        }
        return level_;
    }

private:
    void recalc() {
        attackStep_ = 1.0f / std::max(1.0f, attack_ * sampleRate_);

        // 指数逼近：6.0 个时间常数后基本到位，听感比线性自然
        const float decaySamples   = std::max(1.0f, decay_ * sampleRate_);
        const float releaseSamples = std::max(1.0f, release_ * sampleRate_);
        decayCoef_   = 1.0f - std::exp(-6.0f / decaySamples);
        releaseCoef_ = 1.0f - std::exp(-6.0f / releaseSamples);
    }

    float sampleRate_ = 48000.0f;
    float attack_  = 0.01f;
    float decay_   = 0.3f;
    float sustain_ = 0.7f;
    float release_ = 0.3f;

    float attackStep_  = 1.0f / 480.0f;
    float decayCoef_   = 0.01f;
    float releaseCoef_ = 0.01f;

    float level_ = 0.0f;
    Stage stage_ = Stage::Idle;
};

} // namespace synth
