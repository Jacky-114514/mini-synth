#include "Voice.h"

#include <cmath>

namespace synth {

void Voice::setSampleRate(float sr) {
    sampleRate_ = sr;
    osc1_.setSampleRate(sr);
    osc2_.setSampleRate(sr);
    ampEnv_.setSampleRate(sr);
    fltEnv_.setSampleRate(sr);
    filter_.setSampleRate(sr);
}

void Voice::noteOn(int midiNote, float velocity, const Patch& patch, uint32_t age) {
    note_     = midiNote;
    velocity_ = velocity;
    age_      = age;
    patch_    = patch;

    const float baseHz = noteToHz(midiNote);
    const float ratio  = std::pow(2.0f, patch.detuneCents / 1200.0f);

    osc1_.setWave(patch.wave1);
    osc2_.setWave(patch.wave2);
    osc1_.setFrequency(baseHz / ratio);   // 一个偏低一点
    osc2_.setFrequency(baseHz * ratio);   // 一个偏高一点

    // 两个振荡器相位错开，避免完全同相导致声音单薄
    osc1_.reset(0.0f);
    osc2_.reset(0.25f);

    ampEnv_.setParams(patch.ampAttack, patch.ampDecay, patch.ampSustain, patch.ampRelease);
    fltEnv_.setParams(patch.fltAttack, patch.fltDecay, patch.fltSustain, patch.fltRelease);
    ampEnv_.noteOn();
    fltEnv_.noteOn();

    filter_.reset();
}

void Voice::noteOff() {
    ampEnv_.noteOff();
    fltEnv_.noteOff();
}

void Voice::render(float* out, int frames) {
    if (ampEnv_.isIdle()) return;

    const Patch& p = patch_;

    // 力度对音量、亮度的影响
    const float velAmp  = 1.0f - p.velocityToAmp + p.velocityToAmp * velocity_;
    const float velCut  = 1.0f + p.velocityToCutoff * (velocity_ - 1.0f);
    const float oscGain = 1.0f / (1.0f + p.mix2);

    for (int i = 0; i < frames; ++i) {
        // 1) 两个振荡器混合
        float s = osc1_.process() + p.mix2 * osc2_.process();
        s *= oscGain;

        // 2) 滤波器包络把截止频率往上推（最多约 4 个八度）
        const float fenv   = fltEnv_.process();
        const float cutoff = p.cutoffHz * std::exp2(p.filterEnvAmount * fenv * 4.0f) * velCut;
        filter_.set(cutoff, p.resonance);

        // 3) 滤波
        s = filter_.processLowpass(s);

        // 4) 音量包络
        const float aenv = ampEnv_.process();

        // 5) 累加到混音总线
        out[i] += s * aenv * velAmp * p.gain;
    }
}

} // namespace synth
