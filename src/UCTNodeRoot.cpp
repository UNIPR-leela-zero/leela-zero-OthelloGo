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
#include "KoState.h"
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

void UCTNode::kill_superkos(const GameState& state) {
    UCTNodePointer* pass_child = nullptr;
    size_t valid_count = 0;

    for (auto& child : m_children) {
        auto move = child->get_move();
        if (move != FastBoard::PASS) {
            KoState mystate = state;
            mystate.play_move(move);

            // Checks if the superko rule was broken.
            if (mystate.superko()) {
                // Don't delete nodes for now, just mark them invalid.
                child->invalidate();
            }
        } else {
            pass_child = &child;
        }
        if (child->valid()) {
            valid_count++;
        }
    }

    if (valid_count > 1 && pass_child
        && !state.is_move_legal(state.get_to_move(), FastBoard::PASS)) {
        // Remove the PASS node according to "avoid" -- but only if there are
        // other valid nodes left.
        (*pass_child)->invalidate();
    }

    // Now do the actual deletion.
    m_children.erase(
        std::remove_if(begin(m_children), end(m_children),
                       [](const auto& child) { return !child->valid(); }),
        end(m_children));
}

// Dirichlet noise technique: stochastic exploration during the node
// selection phase of the search tree.  Guarantees greater
// diversification in exploring game states.
void UCTNode::dirichlet_noise(const float epsilon, const float alpha) {
    auto child_cnt = m_children.size();

    // Contains the samples of each child's Dirichlet distribution.
    auto dirichlet_vector = std::vector<float>{};
    std::gamma_distribution<float> gamma(alpha, 1.0f);
    for (size_t i = 0; i < child_cnt; i++) {
        // For each child a sample of the gamma distribution is generated.
        dirichlet_vector.emplace_back(gamma(Random::get_Rng()));
    }

    auto sample_sum =
        std::accumulate(begin(dirichlet_vector), end(dirichlet_vector), 0.0f);

    // If the noise vector sums to 0 or a denormal, then don't try to
    // normalize.
    if (sample_sum < std::numeric_limits<float>::min()) {
        return;
    }

    // The vector is normalized.
    for (auto& v : dirichlet_vector) {
        v /= sample_sum;
    }

    child_cnt = 0;
    // For each child calculate a new policy value using the Dirichlet
    // noise formula.
    for (auto& child : m_children) {
        auto policy = child->get_policy();
        auto eta_a = dirichlet_vector[child_cnt++];
        // Mix the original policy value with the Dirichlet noise
        // based on epsilon's value.
        policy = policy * (1 - epsilon) + epsilon * eta_a;
        child->set_policy(policy);
    }
}

// Select a random child node based on the number of visits.
void UCTNode::randomize_first_proportionally() {
    // Keeps track of the cumulative sum of the proportional
    // probabilities of each child during each loop iteration.
    auto accum = 0.0;
    auto norm_factor = 0.0;
    auto accum_vector = std::vector<double>{};

    for (const auto& child : m_children) {
        auto visits = child->get_visits();
        if (norm_factor == 0.0) {
            norm_factor = visits;
            // Nonsensical options? End of game?
            if (visits <= cfg_random_min_visits) {
                // The child node has number of visits less than the
                // minimum threshold.
                return;
            }
        }
        if (visits > cfg_random_min_visits) {
            // Calculate the proportional accumulation of the current child.
            accum += std::pow(visits / norm_factor, 1.0 / cfg_random_temp);
            accum_vector.emplace_back(accum);
        }
    }

    // Distribution between 0 and the proportional accumulation.
    auto distribution = std::uniform_real_distribution<double>{0.0, accum};
    // Random numbers of the distribution.
    auto pick = distribution(Random::get_Rng());
    auto index = size_t{0};
    // Look for the index of the first element that goes over the
    // random number "pick".
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

// Returns a pointer to the first child node that doesn't represent "PASS".
UCTNode* UCTNode::get_nopass_child(FastState& state) const {
    for (const auto& child : m_children) {
        /* If we prevent the engine from passing, we must bail out when
           we only have unreasonable moves to pick, like filling eyes.
           Note that this knowledge isn't required by the engine,
           we require it because we're overruling its moves. */
        if (child->m_move != FastBoard::PASS
            && !state.board.is_eye(state.get_to_move(), child->m_move)) {
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
    // If it's expandable create child nodes.
    if (expandable()) {
        create_children(network, nodes, root_state, root_eval);
    }
    // If it already has children, calculate the evaluation.
    if (had_children) {
        root_eval = get_net_eval(color);
    } else {
        // The node was just expanded so calculate the evaluation.
        root_eval = (color == FastBoard::BLACK ? root_eval : 1.0f - root_eval);
    }
    Utils::myprintf("NN eval=%f\n", root_eval);

    // There are a lot of special cases where code assumes
    // all children of the root are inflated, so do that.
    inflate_all_children();

    // Remove illegal moves, so the root move list is correct.
    // This also removes a lot of special cases.
    kill_superkos(root_state);

    if (cfg_noise) {
        // Adjust the Dirichlet noise's alpha constant to the board size
        auto alpha = 0.03f * 361.0f / NUM_INTERSECTIONS;
        dirichlet_noise(0.25f, alpha);
    }
}
