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

// Scansiona un flusso di input carattere per carattere ed estrae partite SGF (Smart Game Format) salvate.
// Queste vengono salvate all'interno di un vector di stringhe.
std::vector<std::string> SGFParser::chop_stream(std::istream& ins,
    const size_t stopat) {
    std::vector<std::string> result;
    std::string gamebuff;

    ins >> std::noskipws;

    int nesting = 0;    // Gestione annidamento delle parentesi. 
    bool intag = false; // Gestione situazione dei Tag. I tag vengono aperti con "[" e non possono contenere altre parentesi prima di essere chiusi con "]".
    int line = 0;
    gamebuff.clear();

    char c;
    // Continua finchè non ci sono più caratteri da leggere, 
    // oppure fino a quando il vettore result non raggiunge il limite definito da "stopat".
    while (ins >> c && result.size() <= stopat) {
        if (c == '\n') {
            line++;
        }

        gamebuff.push_back(c);
        // Garantisce che il carattere successivo a '\\' NON venga letto come carattere speciale, bensì come carattere "normale".
        if (c == '\\') {
            // read literal char
            ins >> c;
            gamebuff.push_back(c);
            // Skip special char parsing
            continue;
        }

        // Gestisce i caratteri parentesi tonde. Se non si è all'interno di un'altra parentesi tonda (nesting == 0), 
        // gli spazi bianchi e i ';' vengono ignorati,ovvero vengono letti nel ciclo while e poi il gamebuff viene pulito. 
        // In questo modo verranno registrate solamente le informazioni essenziali.
        if (c == '(' && !intag) {
            if (nesting == 0) {
                // eat ; too
                do {
                    ins >> c;
                } while (std::isspace(c) && c != ';');
                gamebuff.clear();
            }
            nesting++;
        }
        else if (c == ')' && !intag) {
            nesting--;
            // Se nesting è uguale a zero, allora la partita è conclusa, quindi viene salvata all'interno del vettore result. 
            if (nesting == 0) {
                result.push_back(gamebuff);
            }
        }
        else if (c == '[' && !intag) {
            intag = true;
        }
        else if (c == ']') {
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
    // std::ifstream::binary --> file in modalità binaria.
    // std::ifstream::in --> file in sola lettura.
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
// (Estrae una specifica partita da un file SGF in base all'indice specificato e la restituisce come stringa)
std::string SGFParser::chop_from_file(const std::string& filename,
    const size_t index) {
    auto vec = chop_all(filename, index);
    return vec[index];
}

// Estrae il nome di una proprietà e lo restituisce come stringa.
// Le proprietà sono rappresentate da coppie di valori (nome proprietà - contenuto proprietà) 
// e forniscono informazioni sulla partita.
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
        }
        else {
            result.push_back(c);
        }
    }

    return result;
}

// Estrae il valore di una proprietà e lo restituisce come stringa.
bool SGFParser::parse_property_value(std::istringstream& strm,
    std::string& result) {

    // Gli spazi bianchi non vengono ignorati.
    strm >> std::noskipws;

    // Se c non è uno spazio bianco, il carattere viene rimesso nello stream di input.
    char c;
    while (strm >> c) {
        if (!std::isspace(c)) {
            strm.unget();
            break;
        }
    }

    strm >> c;

    // Il primo carattere della proprietà deve essere ']', ovvero il carattere di apertura.
    if (c != '[') {
        strm.unget();
        return false;
    }

    // I caratteri vengono letti finché la proprietà non viene chiusa con ']'.
    while (strm >> c) {
        if (c == ']') {
            break;
        }
        else if (c == '\\') {
            strm >> c;
        }
        result.push_back(c);
    }

    // Gli spazi bianchi vengono ignorati.
    strm >> std::skipws;

    return true;
}

// Crea un albero SGF rappresentante la struttura della partita a partire da un flusso di input SGF.
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
        // Controlla se il carattere è alfabetico ed è maiuscolo.
        if (std::isalpha(c) && std::isupper(c)) {
            // La proprietà viene salvata dal carattere successivo, quindi quello corrente viene scartato.
            strm.unget();

            std::string propname = parse_property_name(strm);
            bool success;

            do {
                std::string propval;
                success = parse_property_value(strm, propval);
                // Se l'analisi del valore ha successo, viene creato un nodo 
                // contenente la proprietà e il ciclo do-while continua.
                if (success) {
                    node->add_property(propname, propval);
                }
            } while (success);

            continue;
        }

        if (c == '(') {
            // eat first ;
            char cc;
            // Ignora spazi bianchi ed eventuale ';' successivo.
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
            // Viene chiamato parse per continuare l'analisi dell'input 
            // della nuova variante di gioco, a partire dal nodo appena creato.
            parse(strm, newptr);
        }
        else if (c == ')') {
            // variation ends, go back
            // if the variation didn't start here, then
            // push the "variation ends" mark back
            // and try again one level up the tree
            if (!splitpoint) {
                strm.unget();
                return;
            }
            else {
                splitpoint = false;
                continue;
            }
            // Inizio nuovo movimento della partita, creazione nuovo figlio.
        }
        else if (c == ';') {
            // new node
            SGFTree* newptr = node->add_child();
            node = newptr;
            continue;
        }
    }
}
