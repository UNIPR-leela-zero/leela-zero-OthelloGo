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

// Valori memorizzati in tabella (ne determina la dimensione)
auto constexpr z_entries = 1000;
std::array<float, z_entries> z_lookup;

// Creazione tabella di valori critici della distribuzione t di Student
void Utils::create_z_table() {
    // Scorre i vari gradi di libert�
    for (auto i = 1; i < z_entries + 1; i++) {
        // Distribuzione t di Student
        boost::math::students_t dist(i);
        // Calcolo dei valori critici in base alla distribuzione e
        // il livello di significativit� cfg_ci_alpha.
        auto z =
            boost::math::quantile(boost::math::complement(dist, cfg_ci_alpha));
        z_lookup[i - 1] = z;
    }
}

// Restituisce un valore critico in base al numero di 
// gradi di libert� v.
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

// Verifica se ci sono dati in input disponibili
bool Utils::input_pending() {
// Sistemi POSIX: il SO supporta select()
#ifdef HAVE_SELECT
    fd_set read_fds;
    FD_ZERO(&read_fds);                 // Tutti i bit vengono azzerati
    FD_SET(0, &read_fds);               // Viene settato il bit corrispondente all'input utilizzato
    struct timeval timeout{0, 0};
    select(1, &read_fds, nullptr, nullptr, &timeout);       
    return FD_ISSET(0, &read_fds);
// Sistemi Windows: il SO non supporta select()
#else
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;

    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        // Verifica se lo standard input � una pipe
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            // Disabilitati input da mouse e gestione eventi in finestra
            SetConsoleMode(inh,
                           dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }

    // Controllo dati in lettura in base alla presenza di pipe
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

// Gestione output
static void myprintf_base(const char* const fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    // Stampa sulla console di errore
    vfprintf(stderr, fmt, ap);

    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        // Stampa su file di log
        vfprintf(cfg_logfile_handle, fmt, ap2);
    }
    va_end(ap2);
}

// Stampa messaggi 
void Utils::myprintf(const char* const fmt, ...) {
    if (cfg_quiet) {
        // Modalit� silenziosa, nessuna stampa
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    myprintf_base(fmt, ap);
    va_end(ap);
}

// Stampa messaggi di errore formattati
void Utils::myprintf_error(const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    myprintf_base(fmt, ap);
    va_end(ap);
}

static void gtp_fprintf(FILE* const file, const std::string& prefix,
    const char* const fmt, va_list ap) {
    // Stampa del prefisso
    fprintf(file, "%s ", prefix.c_str());  
    // Stampa del messaggio da file specificato
    vfprintf(file, fmt, ap);                    
    fprintf(file, "\n\n");
}

static void gtp_base_printf(const int id, std::string prefix,
                            const char* const fmt, va_list ap) {
    // Aggiunto ID al prefisso se diverso di -1
    if (id != -1) {
        prefix += std::to_string(id);
    }
    gtp_fprintf(stdout, prefix, fmt, ap);
    // Instradamento dell'output al file di log
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        gtp_fprintf(cfg_logfile_handle, prefix, fmt, ap);
    }
}

void Utils::gtp_printf(const int id, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    // Stampa seguendo le specifiche del protocollo GTP
    gtp_base_printf(id, "=", fmt, ap);
    va_end(ap);
}

// Nessun prefisso applicato
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

// Stampa messaggi di errore con la formattazione GTP
void Utils::gtp_fail_printf(const int id, const char* const fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gtp_base_printf(id, "?", fmt, ap);
    va_end(ap);
}

// L'input viene registrato nel file di log
void Utils::log_input(const std::string& input) {
    // Controllo configurazione file di log
    if (cfg_logfile_handle) {
        std::lock_guard<std::mutex> lock(IOmutex);
        fprintf(cfg_logfile_handle, ">>%s\n", input.c_str());
    }
}

// Calcola il multiplo pi� piccolo di un numero a
size_t Utils::ceilMultiple(const size_t a, const size_t b) {
    if (a % b == 0) {
        // a � un multiplo di b
        return a;
    }

    auto ret = a + (b - a % b);
    return ret;
}

// Stampa il percorso completo di un file all'interno della directory specifica
std::string Utils::leelaz_file(const std::string& file) {
#if defined(_WIN32) || defined(__ANDROID__)
    // Usa la directory corrente se il SO � Windows o Android
    boost::filesystem::path dir(boost::filesystem::current_path());
#else
    // https://stackoverflow.com/a/26696759
    const char* homedir;
    // Utilizzo home directory utente
    if ((homedir = getenv("HOME")) == nullptr) {
        struct passwd* pwd;
        // NOLINTNEXTLINE(runtime/threadsafe_fn)
        if ((pwd = getpwuid(getuid())) == nullptr) {
            return std::string();
        }
        homedir = pwd->pw_dir;
    }
    boost::filesystem::path dir(homedir);
    // Creazione directory specificata
    dir /= ".local/share/leela-zero";
#endif
    // Viene aggiunto il nome del file alla directory 
    // per creare il percorso completo.
    boost::filesystem::create_directories(dir);
    dir /= file;
    return dir.string();
}
