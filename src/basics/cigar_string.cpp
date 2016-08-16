// Copyright (c) 2016 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#include "cigar_string.hpp"

#include <cctype>
#include <algorithm>
#include <array>
#include <ostream>
#include <stdexcept>

#include <boost/lexical_cast.hpp>

namespace octopus {

CigarOperation::CigarOperation(const Size size, const Flag flag) noexcept
:
size_ {size},
flag_ {flag}
{}

CigarOperation::Flag CigarOperation::flag() const noexcept
{
    return flag_;
}

CigarOperation::Size CigarOperation::size() const noexcept
{
    return size_;
}

bool CigarOperation::advances_reference() const noexcept
{
    return !(flag_ == Flag::Insertion || flag_ == Flag::HardClipped || flag_ == Flag::Padding);
}

bool CigarOperation::advances_sequence() const noexcept
{
    return !(flag_ == Flag::Deletion || flag_ == Flag::HardClipped);
}

// non-member methods

bool is_valid(const CigarOperation::Flag flag) noexcept
{
    using Flag = CigarOperation::Flag;
    switch (flag) {
        case Flag::AlignmentMatch:
        case Flag::SequenceMatch:
        case Flag::Substitution:
        case Flag::Insertion:
        case Flag::Deletion:
        case Flag::SoftClipped:
        case Flag::HardClipped:
        case Flag::Padding:
        case Flag::Skipped: return true;
        default: return false;
    }
}

bool is_valid(const CigarOperation& op) noexcept
{
    return is_valid(op.flag()) && op.size() > 0;
}

bool is_match(const CigarOperation& op) noexcept
{
    using Flag = CigarOperation::Flag;
    switch (op.flag()) {
        case Flag::AlignmentMatch:
        case Flag::SequenceMatch:
        case Flag::Substitution: return true;
        default: return false;
    }
}

bool is_indel(const CigarOperation& op) noexcept
{
    using Flag = CigarOperation::Flag;
    return op.flag() == Flag::Insertion || op.flag() == Flag::Deletion;
}

bool is_clipping(const CigarOperation& op) noexcept
{
    using Flag = CigarOperation::Flag;
    return op.flag() == Flag::SoftClipped || op.flag() == Flag::HardClipped;
}

// CigarString

CigarString parse_cigar(const std::string& cigar)
{
    CigarString result {};
    result.reserve(cigar.size() / 2); // max possible CigarOperation
    
    std::string digits {};
    digits.reserve(3); // 100+ matches are common
    
    for (const char c : cigar) {
        if (std::isdigit(c)) {
            digits += c;
        } else {
            result.emplace_back(boost::lexical_cast<CigarOperation::Size>(digits),
                                static_cast<CigarOperation::Flag>(c));
            digits.clear();
        }
    }
    
    if (!digits.empty()) {
        throw std::invalid_argument {"parse_cigar: unparsed characters in " + cigar};
    }
    
    result.shrink_to_fit();
    
    return result;
}

bool is_valid(const CigarString& cigar) noexcept
{
    return !cigar.empty() && std::all_of(std::cbegin(cigar), std::cend(cigar),
                                         [] (const auto& op) { return is_valid(op); });
}

bool is_minimal(const CigarString& cigar) noexcept
{
    return std::adjacent_find(std::cbegin(cigar), std::cend(cigar),
                              [] (const auto& lhs, const auto& rhs) {
                                  return lhs.flag() == rhs.flag();
                              }) == std::cend(cigar);
}

bool is_front_soft_clipped(const CigarString& cigar) noexcept
{
    return !cigar.empty() && cigar.front().flag() == CigarOperation::Flag::SoftClipped;
}

bool is_back_soft_clipped(const CigarString& cigar) noexcept
{
    return !cigar.empty() && cigar.back().flag() == CigarOperation::Flag::SoftClipped;
}

bool is_soft_clipped(const CigarString& cigar) noexcept
{
    return is_front_soft_clipped(cigar) || is_back_soft_clipped(cigar);
}

std::pair<CigarOperation::Size, CigarOperation::Size>
get_soft_clipped_sizes(const CigarString& cigar) noexcept
{
    return std::make_pair((is_front_soft_clipped(cigar)) ? cigar.front().size() : 0,
                          (is_back_soft_clipped(cigar)) ? cigar.back().size() : 0);
}

// non-member functions

template <typename Predicate>
CigarString splice(const CigarString& cigar, CigarOperation::Size offset,
                   CigarOperation::Size size, Predicate pred)
{
    CigarString result {};
    result.reserve(cigar.size());
    
    auto op_it = std::cbegin(cigar);
    
    const auto last_op = std::cend(cigar);
    
    while (op_it != last_op && (offset >= op_it->size() || !pred(*op_it))) {
        if (pred(*op_it)) offset -= op_it->size();
        ++op_it;
    }
    
    if (op_it != last_op) {
        const auto remainder = op_it->size() - offset;
        
        if (remainder >= size) {
            result.emplace_back(size, op_it->flag());
            result.shrink_to_fit();
            return result;
        }
        
        result.emplace_back(remainder, op_it->flag());
        size -= remainder;
        ++op_it;
    }
    
    while (op_it != last_op && size > 0 && (size >= op_it->size() || !pred(*op_it))) {
        result.emplace_back(*op_it);
        if (pred(*op_it)) size -= op_it->size();
        ++op_it;
    }
    
    if (op_it != last_op && size > 0) {
        result.emplace_back(size, op_it->flag());
    }
    
    result.shrink_to_fit();
    
    return result;
}

CigarString splice(const CigarString& cigar, const CigarOperation::Size offset,
                   const CigarOperation::Size size)
{
    return splice(cigar, offset, size, [] (const auto& op) { return true; });
}

CigarString splice_reference(const CigarString& cigar, const CigarOperation::Size offset,
                             const CigarOperation::Size size)
{
    return splice(cigar, offset, size, [] (const auto& op) { return op.advances_reference(); });
}

CigarString splice_sequence(const CigarString& cigar, const CigarOperation::Size offset,
                            const CigarOperation::Size size)
{
    return splice(cigar, offset, size, [] (const auto& op) { return op.advances_sequence(); });
}

bool operator==(const CigarOperation& lhs, const CigarOperation& rhs) noexcept
{
    return lhs.flag() == rhs.flag() && lhs.size() == rhs.size();
}

bool operator<(const CigarOperation& lhs, const CigarOperation& rhs) noexcept
{
    return (lhs.flag() == rhs.flag()) ? lhs.size() < rhs.size() : lhs.flag() < rhs.flag();
}

std::ostream& operator<<(std::ostream& os, const CigarOperation::Flag& flag)
{
    os << static_cast<char>(flag);
    return os;
}

std::ostream& operator<<(std::ostream& os, const CigarOperation& cigar_operation)
{
    os << cigar_operation.size() << cigar_operation.flag();
    return os;
}

std::ostream& operator<<(std::ostream& os, const CigarString& cigar)
{
    std::copy(std::cbegin(cigar), std::cend(cigar), std::ostream_iterator<CigarOperation>(os));
    return os;
}

std::size_t CigarHash::operator()(const CigarOperation& op) const noexcept
{
    using boost::hash_combine;
    size_t result {};
    hash_combine(result, op.flag());
    hash_combine(result, op.size());
    return result;
}

std::size_t CigarHash::operator()(const CigarString& cigar) const noexcept
{
    return boost::hash_range(std::cbegin(cigar), std::cend(cigar));
}

} // namespace octopus
