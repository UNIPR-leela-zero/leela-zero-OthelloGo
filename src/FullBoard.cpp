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

#include "FullBoard.h"

#include "Network.h"
#include "Utils.h"
#include "Zobrist.h"

using namespace Utils;

// The FullBoard class extends FastBoard.


// Removes an entire group of stones.
int FullBoard::remove_string(const int i) {
    int pos = i;
    int removed = 0;
    int color = m_state[i];

    do {
        m_hash    ^= Zobrist::zobrist[m_state[pos]][pos];
        m_ko_hash ^= Zobrist::zobrist[m_state[pos]][pos];

        m_state[pos] = EMPTY;
        m_parent[pos] = NUM_VERTICES;

        remove_neighbour(pos, color);

        m_empty_idx[pos] = m_empty_cnt;
        m_empty[m_empty_cnt] = pos;
        m_empty_cnt++;

        m_hash    ^= Zobrist::zobrist[m_state[pos]][pos];
        m_ko_hash ^= Zobrist::zobrist[m_state[pos]][pos];

        removed++;
        pos = m_next[pos];
    } while (pos != i);

    return removed;
}

// Creates a unique hash for the current board state in order to check
// for the ko rule.  (You can't immediately repeat a previous
// gamestate)
std::uint64_t FullBoard::calc_ko_hash() const {
    auto res = Zobrist::zobrist_empty;

    for (auto i = 0; i < m_numvertices; i++) {
        if (m_state[i] != INVAL) {
            res ^= Zobrist::zobrist[m_state[i]][i];
        }
    }

    /* Tromp-Taylor has positional superko */
    return res;
}

// More general hash of the board state which takes into account
// prisoners, the last move made, and the current player to move.  You
// can apply a function to transform the values before hashing.
template <class Function>
std::uint64_t FullBoard::calc_hash(const int komove, Function transform) const {
    auto res = Zobrist::zobrist_empty;

    for (auto i = 0; i < m_numvertices; i++) {
        if (m_state[i] != INVAL) {
            res ^= Zobrist::zobrist[m_state[i]][transform(i)];
        }
    }

    /* prisoner hashing is rule set dependent */
    res ^= Zobrist::zobrist_pris[0][m_prisoners[0]];
    res ^= Zobrist::zobrist_pris[1][m_prisoners[1]];

    if (m_tomove == BLACK) {
        res ^= Zobrist::zobrist_blacktomove;
    }

    res ^= Zobrist::zobrist_ko[transform(komove)];

    return res;
}

std::uint64_t FullBoard::calc_hash(const int komove) const {
    return calc_hash(komove, [](const auto vertex) { return vertex; });
}

// Calls calc_hash after applying a symmetry transformation.
std::uint64_t FullBoard::calc_symmetry_hash(const int komove,
                                            const int symmetry) const {
    return calc_hash(komove, [this, symmetry](const auto vertex) {
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

std::uint64_t FullBoard::get_ko_hash() const {
    return m_ko_hash;
}

void FullBoard::set_to_move(const int tomove) {
    if (m_tomove != tomove) {
        m_hash ^= Zobrist::zobrist_blacktomove;
    }
    FastBoard::set_to_move(tomove);
}

// Updates the board when a new piece is added.
int FullBoard::update_board(const int color, const int i) {
    assert(i != FastBoard::PASS);
    assert(m_state[i] == EMPTY);

    m_hash ^= Zobrist::zobrist[m_state[i]][i];
    m_ko_hash ^= Zobrist::zobrist[m_state[i]][i];

    m_state[i] = vertex_t(color);
    m_next[i] = i;
    m_parent[i] = i;
    m_libs[i] = count_pliberties(i);
    m_stones[i] = 1;

    m_hash ^= Zobrist::zobrist[m_state[i]][i];
    m_ko_hash ^= Zobrist::zobrist[m_state[i]][i];

    /* update neighbor liberties (they all lose 1) */
    add_neighbour(i, color);

    auto eyeplay = (m_neighbours[i] & s_eyemask[!color]);
    auto captured_stones = 0;
    int captured_vtx;

    /* did we play into an opponent eye? */
    if constexpr (!IS_OTHELLO) {
        for (int k = 0; k < 4; k++) {
        int ai = i + m_dirs[k];

            if (m_state[ai] == !color) {
                if (m_libs[m_parent[ai]] <= 0) {
                    int this_captured = remove_string(ai);
                    captured_vtx = ai;
                    captured_stones += this_captured;
                }
            } else if (m_state[ai] == color) {
                int ip = m_parent[i];
                int aip = m_parent[ai];

                if (ip != aip) {
                    if (m_stones[ip] >= m_stones[aip]) {
                        merge_strings(ip, aip);
                    } else {
                        merge_strings(aip, ip);
                    }
                }
            }
        }

    m_hash ^= Zobrist::zobrist_pris[color][m_prisoners[color]];
    m_prisoners[color] += captured_stones;
    m_hash ^= Zobrist::zobrist_pris[color][m_prisoners[color]];
    } else {
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
    }
    /* move last vertex in list to our position */
    auto lastvertex = m_empty[--m_empty_cnt];
    m_empty_idx[lastvertex] = m_empty_idx[i];
    m_empty[m_empty_idx[i]] = lastvertex;

    if constexpr (!IS_OTHELLO) {
      /* check whether we still live (i.e. detect suicide) */
        if (m_libs[m_parent[i]] == 0) {
            assert(captured_stones == 0);
            remove_string(i);
        }

        /* check for possible simple ko */
        if (captured_stones == 1 && eyeplay) {
            assert(get_state(captured_vtx) == FastBoard::EMPTY
                && !is_suicide(captured_vtx, !color));
            return captured_vtx;
        }
    }
    // No ko
    return NO_VERTEX;
}

// flips all vertexes from starting to end in direction d; if you
// can't get from start to end through direction dir, segmentation
// fault
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
//checks if a play is legal
bool FullBoard::is_play_legal(const int color, const int i) const {
    int opp = FastBoard::opposite_color(color);
    for (int k = 0; k < 8; k++) {
        int tmp_vtx = i;
        tmp_vtx += m_dirs[k];
        if ( m_state[tmp_vtx] == opp) {
            while (!(m_state[tmp_vtx] == INVAL || m_state[tmp_vtx] == EMPTY)) {
                assert(tmp_vtx > 0 && tmp_vtx < NUM_VERTICES);
                tmp_vtx += m_dirs[k];
                if (m_state[tmp_vtx] == color) { //we found a vertex of the same color as the original after a streak of the opposing color
                    return true;
                }

            }
        }
    }

    return false;
}

//checks if there is a legal move present
bool FullBoard::legal_moves_present(const int color) const{
    int opp = FastBoard::opposite_color(color);
    for (int i = 0; i < m_empty_cnt; i++) {
        if (count_neighbours(opp, m_empty[i]) > 0) {
                if (is_play_legal(color, m_empty[i])) {
                    return true;
                }
            }
    }
    return false;
}
void FullBoard::display_board(const int lastmove) {
    FastBoard::display_board(lastmove);

    myprintf("Hash: %llX Ko-Hash: %llX\n\n", get_hash(), get_ko_hash());
}

void FullBoard::reset_board(const int size) {
    FastBoard::reset_board(size);

    m_hash = calc_hash();
    m_ko_hash = calc_ko_hash();
}
