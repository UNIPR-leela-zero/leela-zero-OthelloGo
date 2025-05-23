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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <regex>
#include <sstream>

#include "TimeControl.h"

#include "GTP.h"
#include "Timing.h"
#include "Utils.h"

using namespace Utils;

TimeControl::TimeControl(const int maintime, const int byotime,
                         const int byostones, const int byoperiods)
    : m_maintime(maintime),
      m_byotime(byotime),
      m_byostones(byostones),
      m_byoperiods(byoperiods) {

    reset_clocks();
}

std::string TimeControl::stones_left_to_text_sgf(const int color) const {
    auto s = std::string{};
    // We must be in byo-yomi before interpreting stones.
    // Byo-yomi: additional time.
    if (m_inbyo[color]) {
        const auto c = color == FastBoard::BLACK ? "OB[" : "OW[";
        if (m_byostones) {
            // Byo-yomi based on the number of stones.
            s += c + std::to_string(m_stones_left[color]) + "]";
        } else if (m_byoperiods) {
            // KGS extension.
            // Byo-yomi based on periods.
            s += c + std::to_string(m_periods_left[color]) + "]";
        }
    }
    return s;
}

// Play time in text.
std::string TimeControl::to_text_sgf() const {
    // There is no byo-yomi.
    if (m_byotime != 0 && m_byostones == 0 && m_byoperiods == 0) {
        return ""; // Infinite time.
    }
    auto s = "TM[" + std::to_string(m_maintime / 100) + "]";
    if (m_byotime) {
        if (m_byostones) {
            s += "OT[" + std::to_string(m_byostones) + "/";
            s += std::to_string(m_byotime / 100) + " Canadian]";
        } else {
            assert(m_byoperiods);
            s += "OT[" + std::to_string(m_byoperiods) + "x";
            s += std::to_string(m_byotime / 100) + " byo-yomi]";
        }
        s += stones_left_to_text_sgf(FastBoard::BLACK);
        s += stones_left_to_text_sgf(FastBoard::WHITE);
    }
    // Generously round up to avoid a remaining time of 0 triggering byo-yomi
    // to be started when the sgf is loaded. This happens because byo-yomi
    // stones have to be only written to the sgf when actually in byo-yomi
    // and this is interpreted in adjust_time() as a special case
    // that starts byo-yomi.
    const auto black_time_left =
        (m_remaining_time[FastBoard::BLACK] + 99) / 100;
    const auto white_time_left =
        (m_remaining_time[FastBoard::WHITE] + 99) / 100;
    s += "BL[" + std::to_string(black_time_left) + "]";
    s += "WL[" + std::to_string(white_time_left) + "]";
    return s;
}

std::shared_ptr<TimeControl> TimeControl::make_from_text_sgf(
    const std::string& maintime, const std::string& byoyomi,
    const std::string& black_time_left, const std::string& white_time_left,
    const std::string& black_moves_left, const std::string& white_moves_left) {
    // Main game time in centiseconds.
    const auto maintime_centis = std::stoi(maintime) * 100;
    auto byotime = 0;
    auto byostones = 0;
    auto byoperiods = 0;
    if (!byoyomi.empty()) {
        std::smatch m;
        // Defines the regex to recognize possible byo-yomi syntaxes.
        const auto re_canadian = std::regex{"(\\d+)/(\\d+) Canadian"};
        const auto re_byoyomi = std::regex{"(\\d+)x(\\d+) byo-yomi"};
        if (std::regex_match(byoyomi, m, re_canadian)) {
            byostones = std::stoi(m[1]);
            byotime = std::stoi(m[2]) * 100;
        } else if (std::regex_match(byoyomi, m, re_byoyomi)) {
            byoperiods = std::stoi(m[1]);
            byotime = std::stoi(m[2]) * 100;
        } else {
            // Unrecognised byo-yomi syntax.
        }
    }
    const auto timecontrol_ptr = std::make_shared<TimeControl>(
        maintime_centis, byotime, byostones, byoperiods);
    // Black still has some time left.
    if (!black_time_left.empty()) {
        const auto time = std::stoi(black_time_left) * 100;
        const auto stones =
            black_moves_left.empty() ? 0 : std::stoi(black_moves_left);
        timecontrol_ptr->adjust_time(FastBoard::BLACK, time, stones);
    }
    // white still has some time left.
    if (!white_time_left.empty()) {
        const auto time = std::stoi(white_time_left) * 100;
        const auto stones =
            white_moves_left.empty() ? 0 : std::stoi(white_moves_left);
        timecontrol_ptr->adjust_time(FastBoard::WHITE, time, stones);
    }
    return timecontrol_ptr;
}

void TimeControl::reset_clocks() {
    m_remaining_time = {m_maintime, m_maintime};
    m_stones_left = {m_byostones, m_byostones};
    m_periods_left = {m_byoperiods, m_byoperiods};
    m_inbyo = {m_maintime <= 0, m_maintime <= 0};
    // Now that byo-yomi status is set, add time
    // back to our clocks
    if (m_inbyo[0]) {
        m_remaining_time[0] = m_byotime;
    }
    if (m_inbyo[1]) {
        m_remaining_time[1] = m_byotime;
    }
}

void TimeControl::start(const int color) {
    m_times[color] = Time();
}

void TimeControl::stop(const int color) {
    Time stop;
    // Move time duration.
    int elapsed_centis = Time::timediff_centis(m_times[color], stop);

    assert(elapsed_centis >= 0);

    m_remaining_time[color] -= elapsed_centis;

    if (m_inbyo[color]) {
        if (m_byostones) {
            m_stones_left[color]--;
        } else if (m_byoperiods) {
            if (elapsed_centis > m_byotime) {
                m_periods_left[color]--;
            }
        }
    }

    /*
        time up, entering byo yomi
    */
    if (!m_inbyo[color] && m_remaining_time[color] <= 0) {
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
        m_periods_left[color] = m_byoperiods;
        m_inbyo[color] = true;
    } else if (m_inbyo[color] && m_byostones && m_stones_left[color] <= 0) {
        // reset byoyomi time and stones
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
    } else if (m_inbyo[color] && m_byoperiods) {
        m_remaining_time[color] = m_byotime;
    }
}

void TimeControl::display_color_time(const int color) {
    auto rem = m_remaining_time[color] / 100; /* centiseconds to seconds */
    auto minuteDiv = std::div(rem, 60);
    auto hourDiv = std::div(minuteDiv.quot, 60);
    auto seconds = minuteDiv.rem;
    auto minutes = hourDiv.rem;
    auto hours = hourDiv.quot;
    auto name = color == 0 ? "Black" : "White";
    myprintf("%s time: %02d:%02d:%02d", name, hours, minutes, seconds);
    if (m_inbyo[color]) {
        if (m_byostones) {
            myprintf(", %d stones left", m_stones_left[color]);
        } else if (m_byoperiods) {
            myprintf(", %d period(s) of %d seconds left",
                     m_periods_left[color], m_byotime / 100);
        }
    }
    myprintf("\n");
}

void TimeControl::display_times() {
    display_color_time(FastBoard::BLACK);
    display_color_time(FastBoard::WHITE);
    myprintf("\n");
}

int TimeControl::max_time_for_move(const int boardsize, const int color,
                                   const size_t movenum) const {
    // default: no byo yomi (absolute)
    auto time_remaining = m_remaining_time[color];
    auto moves_remaining = get_moves_expected(boardsize, movenum);
    auto extra_time_per_move = 0;

    if (m_byotime != 0) {
        /*
          no periods or stones set means
          infinite time = 1 month
        */
        if (m_byostones == 0 && m_byoperiods == 0) {
            return 31 * 24 * 60 * 60 * 100;
        }

        // byo yomi and in byo yomi
        if (m_inbyo[color]) {
            if (m_byostones) {
                moves_remaining = m_stones_left[color];
            } else {
                assert(m_byoperiods);
                // Just use the byo yomi period
                time_remaining = 0;
                extra_time_per_move = m_byotime;
            }
        } else {
            /*
              byo yomi time but not in byo yomi yet
            */
            if (m_byostones) {
                int byo_extra = m_byotime / m_byostones;
                time_remaining = m_remaining_time[color] + byo_extra;
                // Add back the guaranteed extra seconds
                extra_time_per_move = byo_extra;
            } else {
                assert(m_byoperiods);
                int byo_extra = m_byotime * (m_periods_left[color] - 1);
                time_remaining = m_remaining_time[color] + byo_extra;
                // Add back the guaranteed extra seconds
                extra_time_per_move = m_byotime;
            }
        }
    }

    // always keep a cfg_lagbugger_cs centisecond margin
    // for network hiccups or GUI lag
    auto base_time = std::max(time_remaining - cfg_lagbuffer_cs, 0)
                     / std::max(moves_remaining, 1);
    auto inc_time = std::max(extra_time_per_move - cfg_lagbuffer_cs, 0);

    return base_time + inc_time;
}

void TimeControl::adjust_time(const int color, const int time,
                              const int stones) {
    m_remaining_time[color] = time;
    // From pachi: some GTP things send 0 0 at the end of main time
    if (!time && !stones) {
        m_inbyo[color] = true;
        m_remaining_time[color] = m_byotime;
        m_stones_left[color] = m_byostones;
        m_periods_left[color] = m_byoperiods;
    }
    if (stones) {
        // stones are only given in byo-yomi
        m_inbyo[color] = true;
    }
    // we must be in byo-yomi before interpreting stones
    // the previous condition guarantees we do this if != 0
    if (m_inbyo[color]) {
        if (m_byostones) {
            m_stones_left[color] = stones;
        } else if (m_byoperiods) {
            // KGS extension
            m_periods_left[color] = stones;
        }
    }
}

size_t TimeControl::opening_moves(const int boardsize) const {
    auto num_intersections = boardsize * boardsize;
    // Number of expected opening moves.
    auto fast_moves = num_intersections / 6;
    return fast_moves;
}

// Number of expected moves left, based on the dimension of the board
// and the number of played moves.
int TimeControl::get_moves_expected(const int boardsize,
                                    const size_t movenum) const {
    auto board_div = 5;
    if (cfg_timemanage != TimeManagement::OFF) {
        // We will take early exits with time management on, so
        // it's OK to make our base time bigger.
        board_div = 9;
    }

    // Note this is constant as we play, so it's fair
    // to underestimate quite a bit.
    auto base_remaining = (boardsize * boardsize) / board_div;

    // Don't think too long in the opening.
    auto fast_moves = opening_moves(boardsize);
    if (movenum < fast_moves) {
        // Played moves is less than the number of exepcted opening moves.
        return (base_remaining + fast_moves) - movenum;
    } else {
        return base_remaining;
    }
}

// Returns true if we are in a time control where we
// can save up time. If not, we should not move quickly
// even if certain of our move, but plough ahead.
bool TimeControl::can_accumulate_time(const int color) const {
    if (m_inbyo[color]) {
        // Cannot accumulate in Japanese byo yomi
        if (m_byoperiods) {
            return false;
        }

        // Cannot accumulate in Canadese style with
        // one move remaining in the period
        if (m_byostones && m_stones_left[color] == 1) {
            return false;
        }
    } else {
        // If there is a base time, we should expect
        // to be able to accumulate. This may be somewhat
        // of an illusion if the base time is tiny and byo
        // yomi time is big.
    }

    return true;
}
