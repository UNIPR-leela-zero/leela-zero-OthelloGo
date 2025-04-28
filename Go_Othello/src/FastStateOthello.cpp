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

#include <algorithm>
#include <iterator>
#include <vector>

#include "FastStateOthello.h"

#include "FastBoardOthello.h"
#include "GTPOthello.h"
#include "Utils.h"
#include "ZobristOthello.h"

using namespace Utils;

void FastState::init_game(const int size, const float komi) {
    board.reset_board(size);

    m_movenum = 0;
    m_lastmove = FastBoard::NO_VERTEX;
    m_handicap = 0;
    m_passes = 0;
    m_komi = komi;

    return;
}

void FastState::set_komi(const float komi) {
    m_komi = komi;
}

void FastState::reset_game() {
    reset_board();

    m_movenum = 0;
    m_passes = 0;
    m_handicap = 0;
    m_lastmove = FastBoard::NO_VERTEX;
}

void FastState::reset_board() {
    board.reset_board(board.get_boardsize());
}
bool FastState::has_legal_moves(const int color) const {
    return board.legal_moves_present(color);
}
//checks if the move is legal
bool FastState::is_move_legal(const int color, const int vertex) const { //tochange
    
    if (vertex == FastBoard::PASS && board.legal_moves_present(color)) {
        return false;
    }
    
    return !cfg_analyze_tags.is_to_avoid(color, vertex, m_movenum) 
        && (vertex == FastBoard::RESIGN || 
            vertex == FastBoard::PASS ||
            (board.get_state(vertex) == FastBoard::EMPTY && board.is_play_legal(color, vertex)));
}

void FastState::play_move(const int vertex) {
    play_move(board.m_tomove, vertex);
}

//this plays the move
void FastState::play_move(const int color, const int vertex) {
    
    if (vertex != FastBoard::PASS) {
        board.update_board(color, vertex);
    }
    
    m_lastmove = vertex;
    m_movenum++;
    board.m_tomove = !color;
    if (vertex == FastBoard::PASS) {
        increment_passes();
    } else {
        set_passes(0);
    }
}

size_t FastState::get_movenum() const {
    return m_movenum;
}

int FastState::get_last_move() const {
    return m_lastmove;
}

int FastState::get_passes() const {
    return m_passes;
}
float FastState::get_komi() const {
    return m_komi;
}

void FastState::set_passes(const int val) {
    m_passes = val;
}

void FastState::increment_passes() {
    m_passes++;
    if (m_passes > 4) m_passes = 4;
}

int FastState::get_to_move() const {
    return board.m_tomove;
}

void FastState::set_to_move(const int tom) {
    board.set_to_move(tom);
}



void FastState::display_state() {
    auto points = board.area_score(get_komi());
    myprintf("\nPasses: %d            Black (X) Pawns: %.1f\n",
             m_passes, points.first);
    if (board.black_to_move()) {
        myprintf("Black (X) to move");
    } else {
        myprintf("White (O) to move");
    }
    myprintf("    White (O) Pawns: %.1f\n",
             points.second);

    board.display_board(get_last_move());
}

std::string FastState::move_to_text(const int move) const {
    return board.move_to_text(move);
}

std::pair<float, float> FastState::final_score() const {
    return board.area_score(get_komi());
}


void FastState::set_handicap(const int hcap) {
    m_handicap = hcap;
}

int FastState::get_handicap() const {
    return m_handicap;
}

std::uint64_t FastState::get_symmetry_hash(const int symmetry) const {
    return board.calc_symmetry_hash(symmetry);
}

