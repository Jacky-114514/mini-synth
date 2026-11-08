#pragma once

#include <vector>

#include "synth/Patch.h"

namespace demo {

// 一个演示事件：在某一时刻按下/松开某个音，用哪套音色
struct Event {
    float        time;      // 秒
    bool         on;        // true = 按下，false = 松开
    int          note;      // MIDI 音符号（60 = 中央 C）
    float        velocity;  // 力度 0~1
    synth::Patch patch;     // 这套音色的参数
};

struct Data {
    std::vector<Event> events;
    float length = 0.0f;   // 总时长（含尾音），秒
    float bpm    = 100.0f;
};

// 生成一段 4 小节的和弦进行：Am - F - C - G
// 三层音色（Pad / Bass / Pluck）用的是同一个引擎，只是参数不同。
Data make();

} // namespace demo
