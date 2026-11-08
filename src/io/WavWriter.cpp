#include "WavWriter.h"

#include <cmath>
#include <vector>

namespace {

void putU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
}

void putU32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 0xFF));
    }
}

} // namespace

namespace synth {

bool WavWriter::open(const std::string& path, int sampleRate, int channels) {
    close();

    file_ = std::fopen(path.c_str(), "wb");
    if (file_ == nullptr) return false;

    sampleRate_ = sampleRate;
    channels_   = channels;
    dataBytes_  = 0;

    // 先写 44 字节的 WAV 头，长度字段等 close() 时再回填
    std::vector<uint8_t> header;
    header.insert(header.end(), {'R', 'I', 'F', 'F'});
    putU32(header, 36);                  // 文件长度 - 8（稍后修正）
    header.insert(header.end(), {'W', 'A', 'V', 'E'});
    header.insert(header.end(), {'f', 'm', 't', ' '});
    putU32(header, 16);                  // fmt 块长度
    putU16(header, 1);                   // 1 = PCM
    putU16(header, static_cast<uint16_t>(channels));
    putU32(header, static_cast<uint32_t>(sampleRate));
    putU32(header, static_cast<uint32_t>(sampleRate * channels * 2));  // 字节率
    putU16(header, static_cast<uint16_t>(channels * 2));               // 块对齐
    putU16(header, 16);                  // 位深
    header.insert(header.end(), {'d', 'a', 't', 'a'});
    putU32(header, 0);                   // 数据长度（稍后修正）

    std::fwrite(header.data(), 1, header.size(), file_);
    return true;
}

void WavWriter::write(const float* data, int frames) {
    if (file_ == nullptr) return;

    std::vector<int16_t> pcm(static_cast<size_t>(frames) * channels_);
    for (size_t i = 0; i < pcm.size(); ++i) {
        float x = data[i];
        if (x > 1.0f)  x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        pcm[i] = static_cast<int16_t>(std::lround(x * 32767.0f));
    }

    std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), file_);
    dataBytes_ += static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
}

bool WavWriter::close() {
    if (file_ == nullptr) return false;

    const uint32_t riffSize = 36 + dataBytes_;
    std::fseek(file_, 4, SEEK_SET);
    std::fwrite(&riffSize, 4, 1, file_);
    std::fseek(file_, 40, SEEK_SET);
    std::fwrite(&dataBytes_, 4, 1, file_);

    std::fclose(file_);
    file_ = nullptr;
    return true;
}

} // namespace synth
