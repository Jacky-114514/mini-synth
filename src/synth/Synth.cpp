#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace synth {

void Synth::setSampleRate(float sr) {
    for (auto& v : voices_) v.setSampleRate(sr);
}

void Synth::noteOn(int midiNote, float velocity, const Patch& patch) {
    Voice* v = allocate();
    v->noteOn(midiNote, velocity, patch, ++ageCounter_);
}

void Synth::noteOff(int midiNote) {
    for (auto& v : voices_) {
        if (v.isActive() && !v.isReleasing() && v.note() == midiNote) {
            v.noteOff();
        }
    }
}

void Synth::allNotesOff() {
    for (auto& v : voices_) {
        if (v.isActive()) v.noteOff();
    }
}

int Synth::activeVoices() const {
    int n = 0;
    for (const auto& v : voices_) {
        if (v.isActive()) ++n;
    }
    return n;
}

// 找一个可以用来发声的 Voice
Voice* Synth::allocate() {
    // 1) 优先用完全空闲的
    for (auto& v : voices_) {
        if (!v.isActive()) return &v;
    }

    // 2) 没有空闲的，就偷一个"正在释放且最安静"的
    Voice* best = nullptr;
    for (auto& v : voices_) {
        if (v.isReleasing() && (best == nullptr || v.level() < best->level())) {
            best = &v;
        }
    }
    if (best != nullptr) return best;

    // 3) 实在没有，偷最老的那个
    best = &voices_[0];
    for (auto& v : voices_) {
        if (v.age() < best->age()) best = &v;
    }
    return best;
}

void Synth::render(float* out, int frames) {
    std::fill(out, out + frames, 0.0f);

    for (auto& v : voices_) {
        v.render(out, frames);
    }

    // 总线软限幅：tanh 在信号小时近似线性，信号大时平滑压住，避免削顶爆音
    for (int i = 0; i < frames; ++i) {
        out[i] = std::tanh(out[i] * masterGain_);
    }
}

} // namespace synth
