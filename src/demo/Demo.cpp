#include "Demo.h"

namespace demo {

namespace {

synth::Patch makePad() {
    synth::Patch p;
    p.wave1 = synth::Wave::Saw;
    p.wave2 = synth::Wave::Saw;
    p.detuneCents = 12.0f;   // 失谐大一点，声音更宽
    p.mix2 = 0.8f;
    p.cutoffHz = 1300.0f;
    p.resonance = 0.15f;
    p.filterEnvAmount = 0.15f;
    p.ampAttack = 0.45f;     // 慢攻击：像弦乐一样"涌"出来
    p.ampDecay = 1.0f;
    p.ampSustain = 0.85f;
    p.ampRelease = 0.9f;
    p.fltAttack = 0.6f;
    p.fltDecay = 1.0f;
    p.fltSustain = 0.5f;
    p.fltRelease = 0.9f;
    p.gain = 0.13f;
    return p;
}

synth::Patch makeBass() {
    synth::Patch p;
    p.wave1 = synth::Wave::Saw;
    p.mix2 = 0.0f;
    p.cutoffHz = 260.0f;     // 截止很低，声音闷而厚
    p.resonance = 0.35f;
    p.filterEnvAmount = 0.55f;
    p.ampAttack = 0.004f;
    p.ampDecay = 0.22f;
    p.ampSustain = 0.0f;     // 不持续：每个音都是一个"点"
    p.ampRelease = 0.12f;
    p.fltAttack = 0.002f;
    p.fltDecay = 0.18f;
    p.fltSustain = 0.0f;
    p.fltRelease = 0.12f;
    p.gain = 0.55f;
    return p;
}

synth::Patch makePluck() {
    synth::Patch p;
    p.wave1 = synth::Wave::Saw;
    p.wave2 = synth::Wave::Square;
    p.detuneCents = 3.0f;
    p.mix2 = 0.35f;
    p.cutoffHz = 2200.0f;
    p.resonance = 0.50f;     // 共振高，扫频时"哇"的一声更明显
    p.filterEnvAmount = 0.70f;
    p.ampAttack = 0.002f;
    p.ampDecay = 0.16f;
    p.ampSustain = 0.0f;
    p.ampRelease = 0.20f;
    p.fltAttack = 0.002f;
    p.fltDecay = 0.12f;
    p.fltSustain = 0.0f;
    p.fltRelease = 0.15f;
    p.gain = 0.18f;
    return p;
}

} // namespace

Data make() {
    Data d;
    d.bpm = 100.0f;

    const float beat = 60.0f / d.bpm;    // 0.6 秒
    const float bar  = beat * 4.0f;      // 2.4 秒

    const synth::Patch pad   = makePad();
    const synth::Patch bass  = makeBass();
    const synth::Patch pluck = makePluck();

    // 4 个和弦：Am - F - C - G
    // 音符号：60 = C4，69 = A4
    const int chords[4][3] = {
        {57, 60, 64},   // A3  C4  E4
        {53, 57, 60},   // F3  A3  C4
        {60, 64, 67},   // C4  E4  G4
        {55, 59, 62},   // G3  B3  D4
    };
    const int roots[4] = {45, 41, 48, 43};   // A2  F2  C3  G2

    for (int b = 0; b < 4; ++b) {
        const float t0 = b * bar;

        // ---- Pad：整小节铺底 ----
        for (int n = 0; n < 3; ++n) {
            d.events.push_back({t0, true, chords[b][n], 0.75f, pad});
            d.events.push_back({t0 + bar * 0.85f, false, chords[b][n], 0.0f, pad});
        }

        // ---- Bass：每小节 8 个八分音符 ----
        for (int e = 0; e < 8; ++e) {
            const float t = t0 + e * (beat * 0.5f);
            d.events.push_back({t, true, roots[b], 0.9f, bass});
            d.events.push_back({t + 0.26f, false, roots[b], 0.0f, bass});

            // ---- Pluck：和弦音上行琶音，高八度 ----
            const int arpNote = chords[b][e % 3] + 12;
            d.events.push_back({t, true, arpNote, 0.8f, pluck});
            d.events.push_back({t + 0.24f, false, arpNote, 0.0f, pluck});
        }
    }

    d.length = 4 * bar + 2.0f;   // 留 2 秒让 Pad 的释放尾巴自然消失
    return d;
}

} // namespace demo
