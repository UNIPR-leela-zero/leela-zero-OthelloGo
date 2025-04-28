#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/*
 * System-specific input check
 */
#ifdef _WIN32
#undef HAVE_SELECT
#define NOMINMAX
#else
#define HAVE_SELECT
#endif

/*
 * STATIC BOARD DEFINITIONS
 */
#ifdef USE_OTHELLO
static constexpr auto BOARD_SIZE = 8;
static constexpr auto KOMI = 0.5f;
#else
static constexpr auto BOARD_SIZE = 19;
static constexpr auto KOMI = 7.5f;
#endif

static constexpr auto NUM_INTERSECTIONS = BOARD_SIZE * BOARD_SIZE;
static constexpr auto POTENTIAL_MOVES = NUM_INTERSECTIONS + 1;

/*
 * Features
 */
//#define USE_BLAS

#if !defined(__APPLE__) && !defined(__MACOSX)
#if defined(USE_BLAS)
#define USE_OPENBLAS
#endif
#endif

//#define USE_MKL

#ifndef USE_CPU_ONLY
#define USE_OPENCL
#define USE_HALF
#endif

//#define USE_TUNER

static constexpr auto PROGRAM_NAME = "Leela Zero";
static constexpr auto PROGRAM_VERSION = "0.17";

#if defined(USE_BLAS) && defined(USE_OPENBLAS)
static constexpr auto MAX_CPUS = 64;
#else
static constexpr auto MAX_CPUS = 256;
#endif

#ifdef USE_HALF
#include "half/half.hpp"
#endif

#ifdef USE_OPENCL
#define USE_OPENCL_SELFCHECK
static constexpr auto SELFCHECK_PROBABILITY = 2000;
#endif

#if (_MSC_VER >= 1400)
#pragma warning(disable : 4996)
#endif

#endif
