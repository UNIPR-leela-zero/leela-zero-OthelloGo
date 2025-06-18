/*
    This file is part of Leela Zero.
    Copyright (C) 2018-2019 Gian-Carlo Pascutto

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
#include <cassert>
#include <iterator>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "FastBoard.h"
#include "FastState.h"
#include "GTP.h"
#include "Random.h"
#include "UCTNode.h"
#include "Utils.h"

/*
 * These functions belong to UCTNode but should only be called on the root node
 * of UCTSearch and have been seperated to increase code clarity.
 */

UCTNode* UCTNode::get_first_child() const {
    if (m_children.empty()) {
        return nullptr;
    }

    return m_children.front().get();
}

void UCTNode::dirichlet_noise(const float epsilon, const float alpha) {
    auto child_cnt = m_children.size();

    // Conterrà i campioni della distribuzione di
    // Dirichlet per ciascun figlio
    auto dirichlet_vector = std::vector<float>{};
    std::gamma_distribution<float> gamma(alpha, 1.0f);
    for (size_t i = 0; i < child_cnt; i++) {
        // Per ogni figlio viene generato un campione
        // della distribuzione di gamma
        dirichlet_vector.emplace_back(gamma(Random::get_Rng()));
    }

    auto sample_sum =
        std::accumulate(begin(dirichlet_vector), end(dirichlet_vector), 0.0f);

    // If the noise vector sums to 0 or a denormal, then don't try to
    // normalize.
    if (sample_sum < std::numeric_limits<float>::min()) {
        return;
    }

    // Il vettore viene normalizzato
    for (auto& v : dirichlet_vector) {
        v /= sample_sum;
    }

    child_cnt = 0;
    // Per ogni figlio viene calcolata un nuovo valore di 
    // policy usando la formula del rumore di Dirichlet
    for (auto& child : m_children) {
        auto policy = child->get_policy();
        auto eta_a = dirichlet_vector[child_cnt++];
        // Mescola il valore di policy originale con il rumore di
        // dirichlet in base al valore di epsilon
        policy = policy * (1 - epsilon) + epsilon * eta_a;
        child->set_policy(policy);
    }
}

// Seleziona un nodo figlio casuale in base al numero di visite
void UCTNode::randomize_first_proportionally() {
    // Accumulo: variabile che tiene traccia della somma cumulativa 
    // delle probabilità proporzionali dei nodi figli durante l'iterazione del ciclo. 
    auto accum = 0.0;
    auto norm_factor = 0.0;
    auto accum_vector = std::vector<double>{};

    for (const auto& child : m_children) {
        auto visits = child->get_visits();
        if (norm_factor == 0.0) {
            norm_factor = visits;
            // Nonsensical options? End of game?
            if (visits <= cfg_random_min_visits) {
                // Il nodo figlio ha un numero di visite minore della soglia minima
                return;
            }
        }
        if (visits > cfg_random_min_visits) {
            // Calcolo dell'accumulo proporzionale del figlio corrente
            accum += std::pow(visits / norm_factor, 1.0 / cfg_random_temp);
            accum_vector.emplace_back(accum);
        }
    }

    // Distribuzione compresa tra 0 e l'accumulo proporzionale
    auto distribution = std::uniform_real_distribution<double>{0.0, accum};
    // Numeri casuale dalla distribuzione
    auto pick = distribution(Random::get_Rng());
    auto index = size_t{0};
    // Trova l'indice del primo elemento che supera il numero casuale pick
    for (size_t i = 0; i < accum_vector.size(); i++) {
        if (pick < accum_vector[i]) {
            index = i;
            break;
        }
    }

    // Take the early out
    if (index == 0) {
        return;
    }

    assert(m_children.size() > index);

    // Now swap the child at index with the first child
    std::iter_swap(begin(m_children), begin(m_children) + index);
}

// Restituisce un puntatore al primo nodo figlio che non rappresenta la mossa passo
UCTNode* UCTNode::get_nopass_child(FastState& state) const {
    for (const auto& child : m_children) {
        /* If we prevent the engine from passing, we must bail out when
           we only have unreasonable moves to pick, like filling eyes.
           Note that this knowledge isn't required by the engine,
           we require it because we're overruling its moves. */
        if (child->m_move != FastBoard::PASS) {
            return child.get();
        }
    }
    return nullptr;
}

// Used to find new root in UCTSearch.
std::unique_ptr<UCTNode> UCTNode::find_child(const int move) {
    for (auto& child : m_children) {
        if (child.get_move() == move) {
            // no guarantee that this is a non-inflated node
            child.inflate();
            return std::unique_ptr<UCTNode>(child.release());
        }
    }

    // Can happen if we resigned or children are not expanded
    return nullptr;
}

void UCTNode::inflate_all_children() {
    for (const auto& node : get_children()) {
        node.inflate();
    }
}

void UCTNode::prepare_root_node(Network& network, const int color,
                                std::atomic<int>& nodes,
                                GameState& root_state) {
    float root_eval;
    const auto had_children = has_children();
    // Se è espandibile crea i nodi figli
    if (expandable()) {
        create_children(network, nodes, root_state, root_eval);
    }
    // Se ha già dei figli, calcola la sua eval
    if (had_children) {
        root_eval = get_net_eval(color);
    } else {
        // Nodo appena espanso, viene calcolata la sua eval
        root_eval = (color == FastBoard::BLACK ? root_eval : 1.0f - root_eval);
    }
    Utils::myprintf("NN eval=%f\n", root_eval);

    // There are a lot of special cases where code assumes
    // all children of the root are inflated, so do that.
    inflate_all_children();

    //kill_passes(root_state);

    if (cfg_noise) {
        // Adjust the Dirichlet noise's alpha constant to the board size
        auto alpha = 0.5f; // for Othello
        dirichlet_noise(0.25f, alpha);
    }
}
