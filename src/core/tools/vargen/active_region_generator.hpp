// Copyright (c) 2017 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#ifndef active_region_generator_hpp
#define active_region_generator_hpp

#include <vector>
#include <string>
#include <functional>

#include <boost/optional.hpp>

#include "config/common.hpp"
#include "basics/genomic_region.hpp"
#include "basics/aligned_read.hpp"
#include "io/reference/reference_genome.hpp"
#include "utils/assembler_active_region_generator.hpp"

namespace octopus { namespace coretools {

class ActiveRegionGenerator
{
public:
    struct Options
    {
    
    };
    
    ActiveRegionGenerator() = delete;
    
    ActiveRegionGenerator(const ReferenceGenome& reference, Options options);
    
    ActiveRegionGenerator(const ActiveRegionGenerator&)            = default;
    ActiveRegionGenerator& operator=(const ActiveRegionGenerator&) = default;
    ActiveRegionGenerator(ActiveRegionGenerator&&)                 = default;
    ActiveRegionGenerator& operator=(ActiveRegionGenerator&&)      = default;
    
    ~ActiveRegionGenerator() = default;
    
    void add_generator(const std::string& name);
    
    void add_read(const SampleName& sample, const AlignedRead& read);
    template <typename ForwardIterator>
    void add_reads(const SampleName& sample, ForwardIterator first, ForwardIterator last);
    
    std::vector<GenomicRegion> generate(const GenomicRegion& region, const std::string& generator) const;
    
    void clear() noexcept;
    
private:
    std::reference_wrapper<const ReferenceGenome> reference_;
    //Options options_;
    boost::optional<AssemblerActiveRegionGenerator> assembler_active_region_generator_;
    
    std::string assembler_name_, cigar_scanner_name_;
    
    bool is_cigar_scanner(const std::string& generator) const noexcept;
    bool is_assembler(const std::string& generator) const noexcept;
    bool using_assembler() const noexcept;
};

template <typename ForwardIterator>
void ActiveRegionGenerator::add_reads(const SampleName& sample, ForwardIterator first, ForwardIterator last)
{
    if (assembler_active_region_generator_) assembler_active_region_generator_->add(sample, first, last);
}

} // namespace coretools
} // namespace octopus

#endif
