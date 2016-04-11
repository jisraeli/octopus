//
//  individual_genotype_model.cpp
//  Octopus
//
//  Created by Daniel Cooke on 01/04/2016.
//  Copyright © 2016 Oxford University. All rights reserved.
//

#include "individual_genotype_model.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "fixed_ploidy_genotype_likelihood_model.hpp"
#include "maths.hpp"
#include "logging.hpp"

namespace Octopus
{
namespace GenotypeModel
{
    Individual::Individual(unsigned ploidy) : ploidy_ {ploidy} {}
    
    Individual::InferredLatents::InferredLatents(GenotypeProbabilityMap&& genotype_posteriors)
    : genotype_posteriors {std::move(genotype_posteriors)} {}
    
    auto make_latents(const SampleIdType& sample,
                      std::vector<Genotype<Haplotype>>&& genotypes,
                      std::vector<double>&& flat_genotype_posteriors)
    {
        Individual::InferredLatents::GenotypeProbabilityMap genotype_posteriors {
            std::make_move_iterator(std::begin(genotypes)),
            std::make_move_iterator(std::end(genotypes))
        };
        
        insert_sample(sample, flat_genotype_posteriors, genotype_posteriors);
        
        return Individual::InferredLatents {std::move(genotype_posteriors)};
    }
    
    Individual::InferredLatents
    Individual::infer_latents(const SampleIdType& sample,
                              std::vector<Genotype<Haplotype>> candidate_genotypes,
                              const CoalescentModel& genotype_prior_model,
                              const HaplotypeLikelihoodCache& haplotype_likelihoods) const
    {
        assert(!candidate_genotypes.empty());
        
        FixedPloidyGenotypeLikelihoodModel likelihood_model {ploidy_, haplotype_likelihoods};
        
        std::vector<double> genotype_log_posteriors(candidate_genotypes.size());
        
        std::transform(std::cbegin(candidate_genotypes), std::cend(candidate_genotypes),
                       std::begin(genotype_log_posteriors),
                       [&] (const auto& genotype) {
                           return std::log(genotype_prior_model.evaluate(genotype))
                                        + likelihood_model.log_likelihood(sample, genotype);
                       });
        
        const auto norm = Maths::log_sum_exp(genotype_log_posteriors);
        
        for (auto& p : genotype_log_posteriors) p = std::exp(p -= norm);
        
        return make_latents(sample, std::move(candidate_genotypes),
                            std::move(genotype_log_posteriors));
    }
    
    Individual::InferredLatents
    Individual::infer_latents(const SampleIdType& sample,
                              const std::vector<Haplotype>& haplotypes,
                              const CoalescentModel& genotype_prior_model,
                              const HaplotypeLikelihoodCache& haplotype_likelihoods) const
    {
        assert(!haplotypes.empty());
        
        auto genotypes = generate_all_genotypes(haplotypes, ploidy_);
        
        Logging::DebugLogger log {};
        if (DEBUG_MODE) {
            stream(log) << "Generated all possible " << genotypes.size() << " genotypes";
        }
        
        return infer_latents(sample, std::move(genotypes), genotype_prior_model,
                             haplotype_likelihoods);
    }
} // namesapce GenotypeModel
} // namespace Octopus