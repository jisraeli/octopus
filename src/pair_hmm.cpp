//
//  pair_hmm.cpp
//  pair_hmm
//
//  Created by Daniel Cooke on 14/12/2015.
//  Copyright © 2015 Oxford University. All rights reserved.
//

#include "pair_hmm.hpp"

#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <cmath>
#include <functional>
#include <type_traits>
#include <limits>
#include <array>
#include <cassert>

#include "align.h"
#include "banded_simd_viterbi.hpp"

#include <iostream> // DEBUG

namespace
{
    static constexpr double ln_10_div_10 {0.23025850929940458};
    
    template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
    constexpr std::size_t num_values() noexcept
    {
        return std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1;
    }
    
    template <std::size_t N>
    auto make_phred_to_ln_prob_lookup() noexcept
    {
        std::array<double, N> result {};
        
        for (std::size_t i {0}; i < N; ++i) {
            result[i] = -ln_10_div_10 * i;
        }
        
        return result;
    }
    
    template <typename InputIt1, typename InputIt2>
    auto count_mismatches(InputIt1 first1, InputIt1 last1, InputIt2 first2)
    {
        return std::inner_product(first1, last1, first2, std::size_t {0},
                                  std::plus<void>(), std::not_equal_to<void>());
    }
    
    namespace
    {
        std::vector<char> truncate(const std::vector<std::uint8_t>& qualities)
        {
            std::vector<char> result(qualities.size());
            std::transform(std::cbegin(qualities), std::cend(qualities), std::begin(result),
                           [] (const std::uint8_t quality) {
                               constexpr std::uint8_t max_quality {std::numeric_limits<char>::max()};
                               return static_cast<char>(std::min(quality, max_quality));
                           });
            return result;
        }
    } // namespace
    
    auto align(const std::string& truth, const std::string& target,
               const std::vector<std::uint8_t>& target_qualities,
               const std::vector<std::uint8_t>& truth_gap_open_penalties,
               const std::size_t offset_hint,
               const Model& model)
    {
        const auto truth_alignment_size = static_cast<int>(target.size() + 15);
        
        if (offset_hint + truth_alignment_size > truth.size()) {
            return std::numeric_limits<double>::lowest();
        }
        
        const auto truncated_target_qualities          = truncate(target_qualities);
        const auto truncated_truth_gap_open_penalities = truncate(truth_gap_open_penalties);
        
        if (model.flank_clear) {
            const auto score = fastAlignmentRoutine(truth.data() + offset_hint,
                                                    target.data(),
                                                    truncated_target_qualities.data(),
                                                    truth_alignment_size,
                                                    static_cast<int>(target.size()),
                                                    static_cast<int>(model.gapextend),
                                                    static_cast<int>(model.nucprior),
                                                    truncated_truth_gap_open_penalities.data() + offset_hint);
            
            return -ln_10_div_10 * static_cast<double>(score);
        }
        
        int first_pos;
        std::vector<char> align1(2 * target.size() + 16), align2(2 * target.size() + 16);
        
        const auto score = fastAlignmentRoutine(truth.data() + offset_hint,
                                                target.data(),
                                                truncated_target_qualities.data(),
                                                truth_alignment_size,
                                                static_cast<int>(target.size()),
                                                static_cast<int>(model.gapextend),
                                                static_cast<int>(model.nucprior),
                                                truncated_truth_gap_open_penalities.data() + offset_hint,
                                                align1.data(), align2.data(), &first_pos);
        
        const auto flank_score = calculateFlankScore(truth_alignment_size,
                                                     0,
                                                     truncated_target_qualities.data(),
                                                     truncated_truth_gap_open_penalities.data(),
                                                     static_cast<int>(model.gapextend),
                                                     static_cast<int>(model.nucprior),
                                                     static_cast<int>(first_pos + offset_hint),
                                                     align1.data(), align2.data());
        
        return -ln_10_div_10 * static_cast<double>(score - flank_score);
    }
} // namespace

double compute_log_conditional_probability(const std::string& truth, const std::string& target,
                                           const std::vector<std::uint8_t>& target_qualities,
                                           const std::vector<std::uint8_t>& truth_gap_open_penalties,
                                           const std::size_t target_offset_into_truth_hint,
                                           const Model& model)
{
    using std::cbegin; using std::cend; using std::next;
    
    static constexpr std::size_t NUM_QUALITIES {num_values<std::uint8_t>()};
    
    static const auto phred_to_ln_probability = make_phred_to_ln_prob_lookup<NUM_QUALITIES>();
    
    assert(target.size() == target_qualities.size());
    assert(truth.size() == truth_gap_open_penalties.size());
    assert(std::max(truth.size(), target.size()) > target_offset_into_truth_hint);
    
    if (target_offset_into_truth_hint + target.size() > truth.size()) {
        return std::numeric_limits<double>::lowest();
    }
    
    const auto hinted_truth_begin_itr = next(cbegin(truth), target_offset_into_truth_hint);
    
    const auto p = std::mismatch(cbegin(target), cend(target), hinted_truth_begin_itr);
    
    if (p.first == cend(target)) {
        return 0;
    }
    
    const auto num_mismatches = count_mismatches(next(p.first), cend(target), next(p.second)) + 1;
    
    if (num_mismatches == 1) {
        const auto mismatch_index = std::distance(hinted_truth_begin_itr, p.second);
        
        if (target_qualities[mismatch_index] <= truth_gap_open_penalties[mismatch_index]) {
            return phred_to_ln_probability[target_qualities[mismatch_index]];
        }
        
        if (std::equal(next(p.first), cend(target), p.second)) {
            return phred_to_ln_probability[truth_gap_open_penalties[mismatch_index]];
        }
        
        return phred_to_ln_probability[target_qualities[mismatch_index]];
    }
    
    return align(truth, target, target_qualities, truth_gap_open_penalties,
                 target_offset_into_truth_hint, model);
    
    return 0;
}