#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace synth {

// ------------------------------------------------------------
// 把 float 采样写成 16 bit PCM 的 .wav 文件
//
// 这相当于平台的"输出层"。将来搬到 RK3562 上时，
// 把它换成 ALSA 的 snd_pcm_writei() 就行，DSP 代码一行都不用改。
// ------------------------------------------------------------
class WavWriter {
public:
    ~WavWriter() { close(); }

    bool open(const std::string& path, int sampleRate, int channels);

    // data 是交错的 float 采样，范围 [-1, 1]
    void write(const float* data, int frames);

    bool close();

private:
    std::FILE* file_ = nullptr;
    int      sampleRate_ = 48000;
    int      channels_ = 1;
    uint32_t dataBytes_ = 0;
};

} // namespace synth
