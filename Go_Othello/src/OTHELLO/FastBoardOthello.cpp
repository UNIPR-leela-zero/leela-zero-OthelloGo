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
#include <array>
#include <cassert>
#include <cctype>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>

#include "FastBoardOthello.h"

#include "Utils.h"

using namespace Utils;

const int FastBoard::NBR_SHIFT;
const int FastBoard::NUM_VERTICES;
const int FastBoard::NO_VERTEX;
const int FastBoard::PASS;
const int FastBoard::RESIGN;


const std::array<FastBoard::vertex_t, 4> FastBoard::s_cinvert = {
    WHITE, BLACK, EMPTY, INVAL
};

//returns the board size
int FastBoard::get_boardsize() const {
    return m_boardsize;
}

//this returns the vertex number given x and y position
int FastBoard::get_vertex(const int x, const int y) const {
    assert(x >= 0 && x < BOARD_SIZE);
    assert(y >= 0 && y < BOARD_SIZE);
    assert(x >= 0 && x < m_boardsize);
    assert(y >= 0 && y < m_boardsize);

    int vertex = ((y + 1) * m_sidevertices) + (x + 1); 

    assert(vertex >= 0 && vertex < m_numvertices);

    return vertex;
}

//this returns the x and y coordinates given the vertex
std::pair<int, int> FastBoard::get_xy(const int vertex) const {
    // int vertex = ((y + 1) * (get_boardsize() + 2)) + (x + 1);
    int x = (vertex % m_sidevertices) - 1;
    int y = (vertex / m_sidevertices) - 1;

    assert(x >= 0 && x < m_boardsize);
    assert(y >= 0 && y < m_boardsize);
    assert(get_vertex(x, y) == vertex);

    return std::make_pair(x, y);
}

//returns the state of a vertex, which means whether it's black, white, empty, or invalid
FastBoard::vertex_t FastBoard::get_state(const int vertex) const {
    assert(vertex >= 0 && vertex < NUM_VERTICES);
    assert(vertex >= 0 && vertex < m_numvertices);

    return m_state[vertex];
}

//this sets the state of a vertex
void FastBoard::set_state(const int vertex, const FastBoard::vertex_t content) {
    assert(vertex >= 0 && vertex < NUM_VERTICES);
    assert(vertex >= 0 && vertex < m_numvertices);
    assert(content >= BLACK && content <= INVAL);

    m_state[vertex] = content;
}

//this calls the get_state function but takes the x and y coordinates, which means it calls the get_vertex function to get the vertex to send
FastBoard::vertex_t FastBoard::get_state(const int x, const int y) const {
    return get_state(get_vertex(x, y));
}

//same as stated above but for set_state
void FastBoard::set_state(const int x, const int y,
                          const FastBoard::vertex_t content) {
    set_state(get_vertex(x, y), content);
}

//this sets up the board from zero given the size
void FastBoard::reset_board(const int size) {
    m_boardsize = size;
    m_sidevertices = size + 2; //adds two to account for borders
    m_numvertices = m_sidevertices * m_sidevertices;
    m_tomove = BLACK;
    m_empty_cnt = 0; //counts the empty vertexes
    std::array<int, 4>  center_piece_position;
    int count = 0;
    //these are the directions
    m_dirs[0] = -m_sidevertices;
    m_dirs[1] = -m_sidevertices + 1;
    m_dirs[2] = +1;
    m_dirs[3] = +m_sidevertices +1;
    m_dirs[4] = +m_sidevertices;
    m_dirs[5] = +m_sidevertices-1;
    m_dirs[6] = -1;
    m_dirs[7] = -m_sidevertices - 1;
    
    //sets up all the vertices as invalid
    for (int i = 0; i < m_numvertices; i++) {
        m_state[i] = INVAL;
        m_neighbours[i] = 0;
    }
    
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int vertex = get_vertex(i, j);
            
            if (i == size / 2-1 && j == size / 2-1 || i == size / 2  && j == size / 2 ) { //pedine bianche iniziali
                m_state[vertex] = BLACK;
                center_piece_position[count++] = vertex;
            }
            else if (i == size / 2 && j == size / 2 -1 || i == size / 2 -1 && j == size / 2) { //pedine nere iniziali
                m_state[vertex] = WHITE;
                center_piece_position[count++] = vertex;
            }
            else {
                m_state[vertex] = EMPTY; //sets up the vertex state as empty
                m_empty_idx[vertex] = m_empty_cnt; //since m_empty_cnt is used as the position in the m_empty vector, where all the empty vertexes are stored, you can use this to find the index of the vertex in m_empty
                m_empty[m_empty_cnt++] = vertex; //this adds the vertex to the list of empty ones then increases the m_empty_cnt value
            }
            

            if (i == 0 || i == size - 1) { //this checks if it's on the top or bottom edge
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY);
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY); //if it's not a border vertex, it puts 2 in the bitmax in the zone dedicated to "empty" 
            }

            if (j == 0 || j == size - 1) {//this checks if it's on the right or left edge
                m_neighbours[vertex] += (1 << (NBR_SHIFT * BLACK))
                                      | (1 << (NBR_SHIFT * WHITE));
                m_neighbours[vertex] +=  1 << (NBR_SHIFT * EMPTY);
            } else {
                m_neighbours[vertex] +=  2 << (NBR_SHIFT * EMPTY); //if it's not a border vertex, it puts 2 in the bitmax in the zone dedicated to "empty" 
            }

            
        }
        
        
        
        for (int i = 0; i < count; i++) {
            auto piece = center_piece_position[i];
            add_neighbour(piece, m_state[piece]);
        }
    }
    
    assert(m_state[NO_VERTEX] == INVAL);
}

// count neighbours of color c at vertex v
// the border of the board has fake neighours of both colors
int FastBoard::count_neighbours(const int c, const int v) const {
    assert(c == WHITE || c == BLACK || c == EMPTY);
    return (m_neighbours[v] >> (NBR_SHIFT * c)) & NBR_MASK; //this returns the number of neighbours of color c by checking the neighbours bitmask, shifting to the color we want to check, then using the NBR_MASK to isolate the ones we need
}

//this adds a vertex, which means it needs to update all its neighbours' data
void FastBoard::add_neighbour(const int vtx, const int color) {
    assert(color == WHITE || color == BLACK || color == EMPTY);

    for (int k = 0; k < 8; k++) {
        int ai = vtx + m_dirs[k]; //iterates on its neighbours

        m_neighbours[ai] += (1 << (NBR_SHIFT * color))
                          - (1 << (NBR_SHIFT * EMPTY)); //removes an empty, adds one of the color of the pawn we added
    }
}

void FastBoard::flip_neighbour(const int vtx, const int color) { //instead of removing empty spaces when adding a new pawn, we want to flip to the new color
    assert(color == WHITE || color == BLACK || color == EMPTY);
    
    for (int k = 0; k < 8; k++) {
        int ai = vtx + m_dirs[k]; //iterates on its neighbours
        if (color == BLACK) {
            m_neighbours[ai] += (1 << (NBR_SHIFT * color))
                - (1 << (NBR_SHIFT * WHITE)); //removes a white, adds a black
        }
        if (color == WHITE) {
            m_neighbours[ai] += (1 << (NBR_SHIFT * color))
                - (1 << (NBR_SHIFT * BLACK)); //removes a black, adds a white
        }
    }
    
}

// Gives the scores for each player
std::pair<float, float> FastBoard::area_score(const float komi) const {
    int black_count=0, white_count=0;
    
    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {
            int vertex = get_vertex(i, j);
            if (m_state[vertex] == BLACK) {
                ++black_count;
            }
            if (m_state[vertex] == WHITE) {
                ++white_count;
            }
        }
    }
    
    return std::make_pair(black_count, white_count+komi);
}

//This function displays the board, marking the last move played
void FastBoard::display_board(const int lastmove) {
    int boardsize = get_boardsize();

    myprintf("\n   ");
    print_columns();
    for (int j = boardsize - 1; j >= 0; j--) {
        myprintf("%2d", j + 1);
        if (lastmove == get_vertex(0, j))
            myprintf("(");
        else
            myprintf(" ");
        for (int i = 0; i < boardsize; i++) {
            if (get_state(i, j) == WHITE) {
                myprintf("O");
            } else if (get_state(i, j) == BLACK) {
                myprintf("X");
            } else if (starpoint(boardsize, i, j)) {
                myprintf("+");
            } else {
                myprintf(".");
            }
            if (lastmove == get_vertex(i, j)) {
                myprintf(")");
            } else if (i != boardsize - 1 && lastmove == get_vertex(i, j) + 1) {
                myprintf("(");
            } else {
                myprintf(" ");
            }
        }
        myprintf("%2d\n", j + 1);
    }
    myprintf("   ");
    print_columns();
    myprintf("\n");
}

//prints the columns
void FastBoard::print_columns() {
    for (int i = 0; i < get_boardsize(); i++) {
        if (i < 25) {
            myprintf("%c ", (('a' + i < 'i') ? 'a' + i : 'a' + i + 1));
        } else {
            myprintf("%c ", (('A' + (i - 25) < 'I') ? 'A' + (i - 25)
                                                    : 'A' + (i - 25) + 1));
        }
    }
    myprintf("\n");
}


//converts the move to text
std::string FastBoard::move_to_text(const int move) const {
    std::ostringstream result;

    int column = move % m_sidevertices;
    int row = move / m_sidevertices;

    column--;
    row--;

    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (column >= 0 && column < m_boardsize));

    if (move >= 0 && move <= m_numvertices) {
        result << static_cast<char>(column < 8 ? 'A' + column
                                               : 'A' + column + 1);
        result << (row + 1);
    } 
    else if (move == FastBoard::PASS) {
        result << "pass";
    } 
    else if (move == FastBoard::RESIGN) {
        result << "resign";
    }
    else {
        result << "error";
    }

    return result.str();
}

//converts the text to a move
int FastBoard::text_to_move(std::string move) const {
    transform(cbegin(move), cend(move), begin(move), tolower);

    if (move == "pass") {
        return PASS;
    } else if (move == "resign") {
        return RESIGN;
    } else if (move.size() < 2 || !std::isalpha(move[0])
               || !std::isdigit(move[1]) || move[0] == 'i') {
        return NO_VERTEX;
    }

    auto column = move[0] - 'a';
    if (move[0] > 'i') {
        --column;
    }

    int row;
    std::istringstream parsestream(move.substr(1));
    parsestream >> row;
    --row;

    if (row >= m_boardsize || column >= m_boardsize) {
        return NO_VERTEX;
    }

    return get_vertex(column, row);
}

//it converts a move into sgf text, the format used for recording GO games
std::string FastBoard::move_to_text_sgf(const int move) const {
    std::ostringstream result;

    int column = move % m_sidevertices;
    int row = move / m_sidevertices;

    column--;
    row--;

    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (row >= 0 && row < m_boardsize));
    assert(move == FastBoard::PASS || move == FastBoard::RESIGN
           || (column >= 0 && column < m_boardsize));

    // SGF inverts rows
    row = m_boardsize - row - 1;

    if (move >= 0 && move <= m_numvertices) {
        if (column <= 25) {
            result << static_cast<char>('a' + column);
        } else {
            result << static_cast<char>('A' + column - 26);
        }
        if (row <= 25) {
            result << static_cast<char>('a' + row);
        } else {
            result << static_cast<char>('A' + row - 26);
        }
    } else if (move == FastBoard::PASS) {
        result << "tt";
    } else if (move == FastBoard::RESIGN) {
        result << "tt";
    } else {
        result << "error";
    }

    return result.str();
}

bool FastBoard::starpoint(const int size, const int point) {
    int stars[3];
    int points[2];
    int hits = 0;

    if (size % 2 == 0 || size < 9) {
        return false;
    }

    stars[0] = size >= 13 ? 3 : 2;
    stars[1] = size / 2;
    stars[2] = size - 1 - stars[0];

    points[0] = point / size;
    points[1] = point % size;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            if (points[i] == stars[j]) {
                hits++;
            }
        }
    }

    return hits >= 2;
}

bool FastBoard::starpoint(const int size, const int x, const int y) {
    return starpoint(size, y * size + x);
}


int FastBoard::get_to_move() const {
    return m_tomove;
}

bool FastBoard::black_to_move() const {
    return m_tomove == BLACK;
}

bool FastBoard::white_to_move() const {
    return m_tomove == WHITE;
}

void FastBoard::set_to_move(const int tomove) {
    m_tomove = tomove;
}


std::string FastBoard::get_stone_list() const {
    std::string result;

    for (int i = 0; i < m_boardsize; i++) {
        for (int j = 0; j < m_boardsize; j++) {
            int vertex = get_vertex(i, j);

            if (get_state(vertex) != EMPTY) {
                result += move_to_text(vertex) + " ";
            }
        }
    }

    // eat final space, if any.
    if (result.size() > 0) {
        result.resize(result.size() - 1);
    }

    return result;
}
