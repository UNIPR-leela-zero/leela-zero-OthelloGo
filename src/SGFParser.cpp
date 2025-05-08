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

#include <cassert>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

#include "SGFParser.h"

#include "SGFTree.h"
#include "Utils.h"

// Scans an input stream character by character and extracts the saved SGF games.
// The games are saved in a string vector.
std::vector<std::string> SGFParser::chop_stream(std::istream& ins,
                                                const size_t stopat) {
    std::vector<std::string> result;
    std::string gamebuff;

    ins >> std::noskipws;

    int nesting = 0;    // Handles parentheses nesting.
    bool intag = false; // Handles tags. Tags are opened with "[" and
                        // cannot contain any other parentheses before
                        // being closed with "]".
    int line = 0;
    gamebuff.clear();

    char c;

    // Loops until there are no more characters to read, or when vector result
    // reaches the limit defined by "stopat".
    while (ins >> c && result.size() <= stopat) {
        if (c == '\n') {
            line++;
        }

        gamebuff.push_back(c);

        // Guarantees that the characters after "\\" won't be read as
        // special characters.
        if (c == '\\') {
            // read literal char
            ins >> c;
            gamebuff.push_back(c);
            // Skip special char parsing
            continue;
        }


        // Handles "(" and ")". If we are not inside another set of round
        // parentheses, then white spaces and ";" will be ignored. They will be
        // read in the while cycle and then the gamebuff will be cleared.  This
        // helps register only essential information.
        if (c == '(' && !intag) {
            if (nesting == 0) {
                // eat ; too
                do {
                    ins >> c;
                } while (std::isspace(c) && c != ';');
                gamebuff.clear();
            }
            nesting++;
        } else if (c == ')' && !intag) {
            nesting--;

            // If nesting == 0, then the game has ended and will be saved in the
            // result vector.
            if (nesting == 0) {
                result.push_back(gamebuff);
            }
        } else if (c == '[' && !intag) {
            intag = true;
        } else if (c == ']') {
            if (intag == false) {
                Utils::myprintf("Tag error on line %d", line);
            }
            intag = false;
        }
    }

    // No game found? Assume closing tag was missing (OGS)
    if (result.size() == 0) {
        result.push_back(gamebuff);
    }

    return result;
}

std::vector<std::string> SGFParser::chop_all(const std::string& filename,
                                             const size_t stopat) {
    // std::ifstream::binary --> binary file
    // std::ifstream::in --> read-only file

    std::ifstream ins(filename.c_str(),
                      std::ifstream::binary | std::ifstream::in);

    if (ins.fail()) {
        throw std::runtime_error("Error opening file");
    }

    auto result = chop_stream(ins, stopat);
    ins.close();

    return result;
}

// scan the file and extract the game with number index
// Extracts a specific game from an SGF file based on the specified index.
// Returns it as a string.
std::string SGFParser::chop_from_file(const std::string& filename,
                                      const size_t index) {
    auto vec = chop_all(filename, index);
    return vec[index];
}

// Extracts the name of a property and returns it as a string.
// Properties are represented by a pair of values (name - content).
// They provide information about the game.
std::string SGFParser::parse_property_name(std::istringstream& strm) {
    std::string result;

    char c;
    while (strm >> c) {
        // SGF property names are guaranteed to be uppercase,
        // except that some implementations like IGS are retarded
        // and don't folow the spec. So allow both upper/lowercase.
        if (!std::isupper(c) && !std::islower(c)) {
            strm.unget();
            break;
        } else {
            result.push_back(c);
        }
    }

    return result;
}

// Extracts the value of a property and returns it as a string.
bool SGFParser::parse_property_value(std::istringstream& strm,
                                     std::string& result) {
    // White spaces will be ignored.
    strm >> std::noskipws;

    // If c is not a white space, the character will be returned in
    // the input stream.
    char c;
    while (strm >> c) {
        if (!std::isspace(c)) {
            strm.unget();
            break;
        }
    }

    strm >> c;

    // The first character of a property has to be the opening character "[".
    if (c != '[') {
        strm.unget();
        return false;
    }

    // The characters are read until the property is closed with "]".
    while (strm >> c) {
        if (c == ']') {
            break;
        } else if (c == '\\') {
            strm >> c;
        }
        result.push_back(c);
    }

    // White spaces will be ignored.
    strm >> std::skipws;

    return true;
}

// Creates an SGF tree from an SGF input stream that represents the
// game's structure.
void SGFParser::parse(std::istringstream& strm, SGFTree* node) {
    bool splitpoint = false;

    char c;
    while (strm >> c) {
        if (strm.fail()) {
            return;
        }

        if (std::isspace(c)) {
            continue;
        }

        // parse a property
        // Checks if the character is alphabetical and is capitalized.
        if (std::isalpha(c) && std::isupper(c)) {
            // The property is saved by the next character. The
            // current one is discarded.
            strm.unget();

            std::string propname = parse_property_name(strm);
            bool success;

            do {
                std::string propval;
                success = parse_property_value(strm, propval);
                // If the analysis of the value succeeds, then create
                // a node that contains the property.
                if (success) {
                    node->add_property(propname, propval);
                }
            } while (success);

            continue;
        }

        if (c == '(') {
            // eat first ;
            char cc;
            // Ignores white spaces and eventual ";".
            do {
                strm >> cc;
            } while (std::isspace(cc));
            if (cc != ';') {
                strm.unget();
            }
            // start a variation here
            splitpoint = true;
            // new node
            SGFTree* newptr = node->add_child();
            // Continue parsing the input of the new game variation,
            // starting from the new node.
            parse(strm, newptr);
        } else if (c == ')') {
            // variation ends, go back
            // if the variation didn't start here, then
            // push the "variation ends" mark back
            // and try again one level up the tree
            if (!splitpoint) {
                strm.unget();
                return;
            } else {
                splitpoint = false;
                continue;
            }
        // Start a new game movement, so create a new child.
        } else if (c == ';') {
            // new node
            SGFTree* newptr = node->add_child();
            node = newptr;
            continue;
        }
    }
}
