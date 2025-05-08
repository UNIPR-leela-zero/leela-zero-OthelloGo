/*
    This file is part of Leela Zero.
    Copyright (C) 2017-2019 Gian-Carlo Pascutto and contributors

    Leela Zero is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leela Zero is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Leela Zero.  If not, see <http://www.gnu.org/licenses/>.

    Additional permission under GNU GPL version 3 section 7

    If you modify this Program, or any covered work, by linking or
    combining it with NVIDIA Corporation's libraries from the
    NVIDIA CUDA Toolkit and/or the NVIDIA CUDA Deep Neural
    Network library and/or the NVIDIA TensorRT inference library
    (or a modified version of those libraries), containing parts covered
    by the terms of the respective license agreement, the licensors of
    this Program grant you additional permission to convey the resulting
    work.
*/

#include "config.h"

#include <boost/filesystem.hpp>
#include <boost/math/distributions/students_t.hpp>
#include <cstdarg>
#include <cstdio>
#include <mutex>

#include "Utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "GTP.h"

Utils::ThreadPool thread_pool;

// Values memorized in the table (determines the size).
auto constexpr z_entries = 1000;
std::array<float, z_entries> z_lookup;

// Creates a table of the critical values of t Student distribution.
void Utils::create_z_table() {
    // Scans the degrees of freedom.
    for (auto i = 1; i < z_entries + 1; i++) {
        // T Student distribution.
        boost::math::students_t dist(i);
        // Calculates the critical values based on the distribution and the significance level of cfg_ci_alpha.
        auto z =
            boost::math::quantile(boost::math::complement(dist, cfg_ci_alpha));
        z_lookup[i - 1] = z;
    }
}

// Returns the critical value based on the v degrees of freedom.
float Utils::cached_t_quantile(const int v) {
    if (v < 1) {
        return z_lookup[0];
    }
    if (v < z_entries) {
        return z_lookup[v - 1];
    }
    // z approaches constant when v is high enough.
    // With default lookup table size the function is flat enough that we
    // can just return the last entry for all v bigger than it.
    return z_lookup[z_entries - 1];
}

// Checks if there are any available input data.
bool Utils::input_pending() {
// POSIX systems: OS supports select().
#ifdef HAVE_SELECT
    fd_set read_fds;
    // All bits reset to 0.
    FD_ZERO(&read_fds);
    // Sets the bit corresponding to the used input.
    FD_SET(0, &read_fds);
    struct timeval timeout{0, 0};
    select(1, &read_fds, nullptr, nullptr, &timeout);
    return FD_ISSET(0, &read_fds);
#else
    // Windows systems: OS doesn't support select().
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;

    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        // Checks if the standard input is a pipe.
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            // Disable mouse and window input.
            SetConsoleMode(inh,
                           dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }

    // Checks input data based on the presence of pipes.
    if (pipe) {
        if (!PeekNamedPipe(inh, nullptr, 0, nullptr, &dw, nullptr)) {
            myprintf("Nothing at other end - exiting\n");
            exit(EXIT_FAILURE);
        }

        return dw;
    } else {
        if (!GetNumberOfConsoleInputEvents(inh, &dw)) {
            myprintf("Nothing at other end - exiting\n");
            exit(EXIT_FAILURE);
        }

        return dw > 1;
    }
    return false;
#endif
}

static std::mutex IOmutex;

// Output handler
static void myprintf_base(const char* const fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    // Prints on the error console.
    vfprintf(stderr, fmt, ap);

    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        // Prints on file logs.
        vfprintf(cfg_logfile_handle, fmt, ap2);
    }
    va_end(ap2);
}

//Prints messages.
void Utils::myprintf(const char* const fmt, ...) {
    if (cfg_quiet) {
        // Quiet mode, no prints.
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    myprintf_base(fmt, ap);
    va_end(ap);
}

// Prints formatted error messages.
void Utils::myprintf_error(const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    myprintf_base(fmt, ap);
    va_end(ap);
}

static void gtp_fprintf(FILE* const file, const std::string& prefix,
                        const char* const fmt, va_list ap) {
    // Prefix print.
    fprintf(file, "%s ", prefix.c_str());
    // Prints the message from the specified file.
    vfprintf(file, fmt, ap);
    fprintf(file, "\n\n");
}

static void gtp_base_printf(const int id, std::string prefix,
                            const char* const fmt, va_list ap) {
   // Add ID to the prefix if it's != -1.
    if (id != -1) {
        prefix += std::to_string(id);
    }
    gtp_fprintf(stdout, prefix, fmt, ap);
    // Output's route to the log file.
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        gtp_fprintf(cfg_logfile_handle, prefix, fmt, ap);
    }
}

void Utils::gtp_printf(const int id, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    // Print according to the GTP protocol specifications.
    gtp_base_printf(id, "=", fmt, ap);
    va_end(ap);
}

// No prefix applied.
void Utils::gtp_printf_raw(const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        va_start(ap, fmt);
        vfprintf(cfg_logfile_handle, fmt, ap);
        va_end(ap);
    }
}

// Print error messages with GTP format.
void Utils::gtp_fail_printf(const int id, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gtp_base_printf(id, "?", fmt, ap);
    va_end(ap);
}

// Input is registered in the log file.
void Utils::log_input(const std::string& input) {
    // Checks the configuration of the log files.
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        fprintf(cfg_logfile_handle, ">>%s\n", input.c_str());
    }
}

// Calculates the smallest multiple of "a".
size_t Utils::ceilMultiple(const size_t a, const size_t b) {
    if (a % b == 0) {
        // a is a multiple of b.
        return a;
    }

    auto ret = a + (b - a % b);
    return ret;
}

// Prints the complete path of a file inside a specified directory.
std::string Utils::leelaz_file(const std::string& file) {
#if defined(_WIN32) || defined(__ANDROID__)
    // Use the current directory if the OS is Windows or Android.
    boost::filesystem::path dir(boost::filesystem::current_path());
#else
    // https://stackoverflow.com/a/26696759
    const char* homedir;
    if ((homedir = getenv("HOME")) == nullptr) {
        // Use the user's home directory.
        struct passwd* pwd;
        // NOLINTNEXTLINE(runtime/threadsafe_fn)
        if ((pwd = getpwuid(getuid())) == nullptr) {
            return std::string();
        }
        homedir = pwd->pw_dir;
    }
    boost::filesystem::path dir(homedir);
    // Create the specified directory.
    dir /= ".local/share/leela-zero";
#endif
    // Add the file name to the directory to create the complete path.
    boost::filesystem::create_directories(dir);
    dir /= file;
    return dir.string();
}
