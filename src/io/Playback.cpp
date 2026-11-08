#include "Playback.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {

// 执行命令并取回标准输出的第一行
std::string captureLine(const std::string& cmd) {
    std::string result;
    std::FILE* pipe = ::popen(cmd.c_str(), "r");
    if (pipe == nullptr) return result;

    std::array<char, 1024> buf{};
    if (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        result = buf.data();
    }
    ::pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

} // namespace

namespace synth {

bool playWavOnWindows(const std::string& linuxPath) {
    if (std::getenv("WSL_DISTRO_NAME") == nullptr) {
        std::cout << "提示：当前不在 WSL 中，请手动播放 " << linuxPath << "\n";
        return false;
    }

    // /home/jacky/xxx.wav  ->  \\wsl.localhost\Ubuntu-22.04\home\jacky\xxx.wav
    const std::string winPath = captureLine("wslpath -w '" + linuxPath + "' 2>/dev/null");
    if (winPath.empty()) {
        std::cout << "提示：wslpath 转换失败，请手动播放 " << linuxPath << "\n";
        return false;
    }

    // 注意：这里必须用 fork + exec 直接传参数，
    // 不能再套一层 shell —— Windows 路径里的反斜杠会被 shell 吃掉。
    const std::string script =
        "(New-Object Media.SoundPlayer '" + winPath + "').PlaySync()";

    std::cout.flush();   // 避免子进程继承还没写出去的缓冲区内容

    const pid_t pid = ::fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // 子进程：把 stdout/stderr 接到 /dev/null，再 exec powershell
        const int devNull = ::open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            ::dup2(devNull, STDOUT_FILENO);
            ::dup2(devNull, STDERR_FILENO);
            ::close(devNull);
        }

        char* argv[] = {
            const_cast<char*>("powershell.exe"),
            const_cast<char*>("-NoProfile"),
            const_cast<char*>("-Command"),
            const_cast<char*>(script.c_str()),
            nullptr,
        };
        ::execvp("powershell.exe", argv);
        ::_exit(127);   // exec 失败
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        // 被信号打断就重试
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace synth
