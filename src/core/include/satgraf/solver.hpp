#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace satgraf::solver {

enum class SolverResult {
    SAT,
    UNSAT,
    UNKNOWN,
    CRASH,
    TIMEOUT,
    NOT_STARTED
};

class NamedFifo {
public:
    explicit NamedFifo(std::string path)
        : path_(std::move(path)), owns_(false) {
        if (::mkfifo(path_.c_str(), S_IRUSR | S_IWUSR) != 0 && errno != EEXIST) {
            throw std::system_error(errno, std::system_category(),
                                    "mkfifo(" + path_ + ")");
        }
        owns_ = true;
    }

    NamedFifo(const NamedFifo&) = delete;
    NamedFifo& operator=(const NamedFifo&) = delete;

    NamedFifo(NamedFifo&& o) noexcept
        : path_(std::move(o.path_)), owns_(o.owns_) { o.owns_ = false; }

    NamedFifo& operator=(NamedFifo&& o) noexcept {
        if (this != &o) {
            cleanup();
            path_ = std::move(o.path_);
            owns_ = o.owns_;
            o.owns_ = false;
        }
        return *this;
    }

    ~NamedFifo() { cleanup(); }

    const std::string& path() const noexcept { return path_; }

    void release() noexcept { owns_ = false; }

private:
    void cleanup() noexcept {
        if (owns_) {
            ::unlink(path_.c_str());
            owns_ = false;
        }
    }

    std::string path_;
    bool owns_;
};

class ExternalSolver {
public:
    ExternalSolver() = default;

    ExternalSolver(const ExternalSolver&) = delete;
    ExternalSolver& operator=(const ExternalSolver&) = delete;

    ExternalSolver(ExternalSolver&& o) noexcept
        : pid_(o.pid_)
        , fifo_path_(std::move(o.fifo_path_))
        , fifo_fd_(o.fifo_fd_)
        , running_(o.running_)
        , reaped_(o.reaped_)
        , exit_code_(o.exit_code_) {
        o.pid_ = -1;
        o.fifo_fd_ = -1;
        o.running_ = false;
        o.reaped_ = false;
    }

    ExternalSolver& operator=(ExternalSolver&& o) noexcept {
        if (this != &o) {
            terminate_gracefully();
            close_fifo();
            pid_ = o.pid_;
            fifo_path_ = std::move(o.fifo_path_);
            fifo_fd_ = o.fifo_fd_;
            running_ = o.running_;
            reaped_ = o.reaped_;
            exit_code_ = o.exit_code_;
            o.pid_ = -1;
            o.fifo_fd_ = -1;
            o.running_ = false;
            o.reaped_ = false;
        }
        return *this;
    }

    ~ExternalSolver() {
        terminate_gracefully();
        close_fifo();
    }

    void start(const std::string& solver_path,
               const std::string& cnf_path,
               const std::string& fifo_path) {
        if (running_) {
            throw std::runtime_error("Solver already running");
        }

        if (!std::filesystem::exists(solver_path)) {
            throw std::runtime_error("Solver binary not found: " + solver_path);
        }

        fifo_path_ = fifo_path;
        reaped_ = false;
        exit_code_ = -1;

        pid_ = ::fork();
        if (pid_ < 0) {
            throw std::system_error(errno, std::system_category(), "fork()");
        }

        if (pid_ == 0) {
            // Child process
            ::close(STDIN_FILENO);

            sigset_t mask;
            ::sigemptyset(&mask);
            ::sigprocmask(SIG_SETMASK, &mask, nullptr);

            int maxfd = ::sysconf(_SC_OPEN_MAX);
            for (int fd = 3; fd < maxfd; fd++) {
                ::close(fd);
            }

            ::execl(solver_path.c_str(), solver_path.c_str(),
                    cnf_path.c_str(),
                    ("--pipe=" + fifo_path).c_str(),
                    static_cast<char*>(nullptr));

            ::_exit(127);
        }

        // Parent
        running_ = true;
    }

    SolverResult wait_for_result(std::chrono::milliseconds timeout =
                                      std::chrono::milliseconds::max()) {
        if (!running_) { return SolverResult::NOT_STARTED; }

        int status = 0;
        auto deadline = (timeout == std::chrono::milliseconds::max())
            ? std::chrono::steady_clock::time_point::max()
            : std::chrono::steady_clock::now() + timeout;

        while (true) {
            pid_t r = ::waitpid(pid_, &status, WNOHANG);
            if (r == pid_) {
                reaped_ = true;
                running_ = false;
                return interpret_status(status);
            }
            if (r < 0) {
                running_ = false;
                return SolverResult::CRASH;
            }

            if (std::chrono::steady_clock::now() >= deadline) {
                return SolverResult::TIMEOUT;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void cancel() {
        if (!running_ || pid_ <= 0) return;

        ::kill(pid_, SIGTERM);

        int status = 0;
        for (int i = 0; i < 50; ++i) {
            if (::waitpid(pid_, &status, WNOHANG) != 0) {
                reaped_ = true;
                running_ = false;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ::kill(pid_, SIGKILL);
        ::waitpid(pid_, &status, 0);
        reaped_ = true;
        running_ = false;
    }

    bool is_running() const {
        if (!running_ || pid_ <= 0) return false;
        int status = 0;
        pid_t r = ::waitpid(pid_, &status, WNOHANG);
        if (r != 0) {
            // Process exited — but we can't modify in const method.
            // Caller should call wait_for_result() or cancel().
            return false;
        }
        return true;
    }

    int open_fifo_for_read(std::chrono::milliseconds timeout =
                               std::chrono::milliseconds(5000)) {
        if (fifo_fd_ >= 0) return fifo_fd_;

        if (fifo_path_.empty()) {
            throw std::runtime_error("No FIFO path set");
        }

        // Use poll() to wait for the writer (solver) to open the FIFO
        // FIFOs block on open() until both sides are open, so we do it
        // in a polling loop with a timeout
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            // Try non-blocking open first
            int fd = ::open(fifo_path_.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                // Successfully opened — now switch to blocking for reads
                int flags = ::fcntl(fd, F_GETFL);
                ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
                fifo_fd_ = fd;
                return fifo_fd_;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        throw std::runtime_error("Timeout waiting for FIFO: " + fifo_path_);
    }

    std::string read_fifo_line(std::chrono::milliseconds timeout =
                                    std::chrono::milliseconds(30000)) {
        if (fifo_fd_ < 0) {
            throw std::runtime_error("FIFO not open");
        }

        std::string line;
        char buf;

        struct pollfd pfd;
        pfd.fd = fifo_fd_;
        pfd.events = POLLIN;

        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true) {
            int remaining_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count());
            if (remaining_ms <= 0) {
                return {};
            }

            int pr = ::poll(&pfd, 1, remaining_ms);
            if (pr < 0) {
                if (errno == EINTR) continue;
                return {};
            }
            if (pr == 0) {
                return {};
            }

            ssize_t n = ::read(fifo_fd_, &buf, 1);
            if (n <= 0) {
                return {};
            }
            if (buf == '\n') {
                return line;
            }
            line += buf;
        }
    }

    pid_t pid() const noexcept { return pid_; }
    bool running() const noexcept { return running_; }
    const std::string& fifo_path() const noexcept { return fifo_path_; }
    int fifo_fd() const noexcept { return fifo_fd_; }

    void set_fifo_path(const std::string& path) { fifo_path_ = path; }

private:
    static SolverResult interpret_status(int status) {
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            switch (code) {
                case 10: return SolverResult::SAT;
                case 20: return SolverResult::UNSAT;
                case 127: return SolverResult::CRASH;
                default: return SolverResult::UNKNOWN;
            }
        }
        if (WIFSIGNALED(status)) {
            return SolverResult::CRASH;
        }
        return SolverResult::UNKNOWN;
    }

    void terminate_gracefully() noexcept {
        if (running_ && pid_ > 0 && !reaped_) {
            cancel();
        }
    }

    void close_fifo() noexcept {
        if (fifo_fd_ >= 0) {
            ::close(fifo_fd_);
            fifo_fd_ = -1;
        }
    }

    pid_t pid_ = -1;
    std::string fifo_path_;
    int fifo_fd_ = -1;
    bool running_ = false;
    bool reaped_ = false;
    int exit_code_ = -1;
};

inline std::string generate_fifo_path() {
    return "/tmp/satgraf-pipe-" + std::to_string(::getpid());
}

inline std::string result_to_string(SolverResult r) {
    switch (r) {
        case SolverResult::SAT:        return "SAT";
        case SolverResult::UNSAT:      return "UNSAT";
        case SolverResult::UNKNOWN:    return "UNKNOWN";
        case SolverResult::CRASH:      return "CRASH";
        case SolverResult::TIMEOUT:    return "TIMEOUT";
        case SolverResult::NOT_STARTED: return "NOT_STARTED";
    }
    return "INVALID";
}

}
