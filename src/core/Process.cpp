#include "Process.hpp"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

extern char** environ;

static pid_t spawn(const std::vector<std::string>& argv, int stdoutFd) {
    std::vector<char*> args;
    for (auto& a : argv)
        args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (stdoutFd >= 0)
        posix_spawn_file_actions_adddup2(&fa, stdoutFd, STDOUT_FILENO);
    else
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    // children must not inherit Hyprland's blocked signal mask
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t none;
    sigemptyset(&none);
    posix_spawnattr_setsigmask(&attr, &none);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSID);

    pid_t pid = -1;
    if (posix_spawnp(&pid, args[0], &fa, &attr, args.data(), environ) != 0)
        pid = -1;
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    return pid;
}

CProcess::CProcess(const std::vector<std::string>& argv, std::function<void(const std::string&)> onLine, std::function<void(const std::string&)> onExit) :
    m_onLine(std::move(onLine)), m_onExit(std::move(onExit)) {
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0)
        return;
    m_pid = spawn(argv, fds[1]);
    close(fds[1]);
    if (m_pid < 0) {
        close(fds[0]);
        return;
    }
    m_fd = fds[0];
    fcntl(m_fd, F_SETFL, fcntl(m_fd, F_GETFL) | O_NONBLOCK);
    m_watch = makeUnique<CFdWatch>(m_fd, [this] { onReadable(); });
}

CProcess::~CProcess() {
    m_watch.reset();
    if (m_fd >= 0) {
        close(m_fd);
        if (m_pid > 0)
            kill(m_pid, SIGTERM);
    }
}

void CProcess::onReadable() {
    char buf[4096];
    while (true) {
        const ssize_t N = read(m_fd, buf, sizeof(buf));
        if (N > 0) {
            m_all.append(buf, N);
            m_buf.append(buf, N);
            size_t nl;
            while ((nl = m_buf.find('\n')) != std::string::npos) {
                const std::string LINE = m_buf.substr(0, nl);
                m_buf.erase(0, nl + 1);
                if (m_onLine)
                    m_onLine(LINE);
            }
            continue;
        }
        if (N < 0 && (errno == EAGAIN || errno == EINTR))
            return;
        break; // EOF or error
    }

    // stdout closed: stop watching (the fd would stay readable forever)
    m_watch.reset();
    close(m_fd);
    m_fd = -1;
    // last thing: the owner may destroy us from inside onExit
    if (m_onExit) {
        auto cb  = m_onExit;
        auto out = std::move(m_all);
        cb(out);
    }
}

void spawnDetached(const std::vector<std::string>& argv) {
    spawn(argv, -1);
}
