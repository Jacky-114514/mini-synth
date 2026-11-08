#pragma once

#include <string>

namespace synth {

// 在 WSL 里把 WAV 交给 Windows 播放。
// 之所以要这样：WSL 本身没有声卡设备（/dev/snd 只有 timer），
// 但可以通过 interop 调用 powershell.exe，用 Windows 的音频播放。
bool playWavOnWindows(const std::string& linuxPath);

} // namespace synth
