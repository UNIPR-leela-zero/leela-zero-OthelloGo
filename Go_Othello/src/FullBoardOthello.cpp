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

#include <array>
#include <cassert>

#include "FullBoardOthello.h"

#include "NetworkOthello.h"
#include "Utils.h"
#include "ZobristOthello.h"

using namespace Utils;

//The Fullboard class extends FastBoard

//this is a more general hash of the boardstate which takes into account prisoners, the last move made, and the current player to move. You can apply a function to transform the values before hashing
template <class Function>
std::uint64_t FullBoard::calc_hash(Function transform) const {
    auto res = Zobrist::zobrist_empty;

    for (auto i = 0; i < m_numvertices; i++) {
        if (m_state[i] != INVAL) {
            res ^= Zobrist::zobrist[m_state[i]][transform(i)];
        }
    }

    if (m_tomove == BLACK) {
        res ^= Zobrist::zobrist_blacktomove;
    }

    return res;
}

std::uint64_t FullBoard::calc_hash() const {
    return calc_hash([](const auto vertex) { return vertex; });
}

//This calls calc_hash after applying a symmetry transformation
std::uint64_t FullBoard::calc_symmetry_hash(const int symmetry) const {
    return calc_hash([this, symmetry](const auto vertex) {
        if (vertex == NO_VERTEX) {
            return NO_VERTEX;
        } else {
            const auto newvtx =
                Network::get_symmetry(get_xy(vertex), symmetry, m_boardsize);
            return get_vertex(newvtx.first, newvtx.second);
        }
    });
}

std::uint64_t FullBoard::get_hash() const {
    return m_hash;
}

void FullBoard::set_to_move(const int tomove) {
    if (m_tomove != tomove) {
        m_hash ^= Zobrist::zobrist_blacktomove;
    }
    FastBoard::set_to_move(tomove);
}

//this function does the updating of the board when a new piece is added

void FullBoard::update_board(const int color, const int i) {
    assert(i != FastBoard::PASS);
    assert(m_state[i] == EMPTY);

    m_hash ^= Zobrist::zobrist[m_state[i]][i];
    m_state[i] = vertex_t(color);
    m_hash ^= Zobrist::zobrist[m_state[i]][i];

    /* update neighbor liberties (they all lose 1) */
    add_neighbour(i, color);
    
    for (int k = 0; k < 8; k++) {
        int tmp_vtx = i;
        tmp_vtx += m_dirs[k];

        if ((m_state[i] == BLACK && m_state[tmp_vtx] == WHITE) || (m_state[i] == WHITE && m_state[tmp_vtx] == BLACK)) {
            
            while (!(m_state[tmp_vtx] == INVAL || m_state[tmp_vtx] == EMPTY)) {
                assert(tmp_vtx > 0 && tmp_vtx < NUM_VERTICES);
                tmp_vtx += m_dirs[k];

                if (m_state[tmp_vtx] == m_state[i]) { //we found a vertex of the same color as the original after a streak of the opposing color
                    flip(i, tmp_vtx, k);
                    break;
                }

            }
        }
    }
    

    /* move last vertex in list to our position */
    auto lastvertex = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex] = m_empty_idx[i];
    m_empty[m_empty_idx[i]] = lastvertex;



}

//flips all vertexes from starting to end in direction d; if you can't get from start to end through direction dir, segmentation fault
void FullBoard::flip(const int starting, const int end, const int dir) {
    
    auto color = m_state[starting];

    int tmp = starting+m_dirs[dir];
    assert(tmp > 0 && tmp < NUM_VERTICES);
    while (tmp != end) {
        assert(tmp > 0 && tmp < NUM_VERTICES);
        m_state[tmp] = color;
        flip_neighbour(tmp, color);
        tmp += m_dirs[dir];
    }
    
}

void FullBoard::display_board(const int lastmove) {
    FastBoard::display_board(lastmove);

    myprintf("Hash: %llX\n\n", get_hash());
}

void FullBoard::reset_board(const int size) {
    FastBoard::reset_board(size);

    m_hash = calc_hash();
}

//checks if there is a legal move present
bool FullBoard::legal_moves_present(const int color) const{
    
    
    for (int i = 0; i < m_empty_cnt; i++) {
        
        if (color == BLACK) { //if the player color is black, check if there are white neighbours
            if (count_neighbours(WHITE, m_empty[i]) > 0) {
                if (is_play_legal(color, m_empty[i])) {
                    return true;
                }
            }
        }
        else if (color == WHITE) { //if the player color is white, check if there are black neighbours
            if (count_neighbours(BLACK, m_empty[i]) > 0) {
                if (is_play_legal(color, m_empty[i])) {
                    return true;
                }
            }
        }

    }
    return false;
}

//checks if a play is legal
bool FullBoard::is_play_legal(const int color, const int i) const {
    //myprintf("I am in is_play_legal\n\n");
    for (int k = 0; k < 8; k++) {
        int tmp_vtx = i;
        tmp_vtx += m_dirs[k];
        //myprintf("I am in direction : %llX\n\n", k);
        if ((color == BLACK && m_state[tmp_vtx] == WHITE) || (color == WHITE && m_state[tmp_vtx] == BLACK)) {

            while (!(m_state[tmp_vtx] == INVAL || m_state[tmp_vtx] == EMPTY)) {
                assert(tmp_vtx > 0 && tmp_vtx < NUM_VERTICES);
                tmp_vtx += m_dirs[k];

                if (m_state[tmp_vtx] == color) { //we found a vertex of the same color as the original after a streak of the opposing color
                    //myprintf("I have found : %llX\n\n", get_xy(tmp_vtx));
                    return true;
                }

            }
        }
    }

    return false;
}
