#pragma once

#include "Oscillator.h"

namespace synth {

// ------------------------------------------------------------
// 一套音色参数（Patch / 预设）
//
// 合成器里"音色"不是一段代码，而是一组数字。
// 同一个引擎，改这些数字，就从贝斯变成了弦乐 Pad。
// ------------------------------------------------------------
struct Patch {
    // ---- 振荡器 ----
    Wave  wave1       = Wave::Saw;
    Wave  wave2       = Wave::Saw;
    float detuneCents = 8.0f;    // 两个振荡器的失谐量（音分），越大越"厚"
    float mix2        = 0.7f;    // 振荡器 2 的音量比例

    // ---- 滤波器 ----
    float cutoffHz        = 1200.0f;  // 截止频率
    float resonance       = 0.25f;    // 共振
    float filterEnvAmount = 0.0f;     // 滤波器包络的调制深度，1.0 约等于 4 个八度

    // ---- 音量包络 ----
    float ampAttack  = 0.01f;
    float ampDecay   = 0.30f;
    float ampSustain = 0.70f;
    float ampRelease = 0.30f;

    // ---- 滤波器包络 ----
    float fltAttack  = 0.01f;
    float fltDecay   = 0.40f;
    float fltSustain = 0.30f;
    float fltRelease = 0.30f;

    // ---- 其他 ----
    float gain             = 0.25f;  // 这个音色的输出电平
    float velocityToAmp    = 0.70f;  // 力度对音量的影响
    float velocityToCutoff = 0.30f;  // 力度对亮度的影响
};

} // namespace synth
