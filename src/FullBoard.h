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

#ifndef FULLBOARD_H_INCLUDED
#define FULLBOARD_H_INCLUDED

#include "config.h"

#include <cstdint>

#include "FastBoard.h"

class FullBoard : public FastBoard {
public:
    int remove_string(int i);
    int update_board(int color, int i);

    std::uint64_t get_hash() const;
    std::uint64_t get_ko_hash() const;
    void set_to_move(int tomove);

    void reset_board(int size);
    void display_board(int lastmove = -1);

    bool legal_moves_present(int color) const;
    bool is_play_legal(int color, int i) const;
    int flippable_direction(int color, int start, int dir) const;
    std::uint64_t calc_hash(int komove = NO_VERTEX) const;
    std::uint64_t calc_symmetry_hash(int komove, int symmetry) const;
    void flip(int starting, int end, int dir);
    std::uint64_t calc_ko_hash() const;

    std::uint64_t m_hash;
    std::uint64_t m_ko_hash;

private:
    template <class Function>
    std::uint64_t calc_hash(int komove, Function transform) const;
};

#endif
