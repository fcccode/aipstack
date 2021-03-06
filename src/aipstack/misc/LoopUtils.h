/*
 * Copyright (c) 2016 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AIPSTACK_LOOP_UTILS_H
#define AIPSTACK_LOOP_UTILS_H

namespace AIpStack {

/**
 * @ingroup misc
 * @defgroup loop-utils Looping Utilities
 * @brief Utilities for looping using range-based for loops.
 * 
 * The @ref LoopRange(IntType) and @ref LoopRange(IntType, IntType) function allows
 * concisely looping through a range of integers.
 * 
 * @{
 */

#ifndef IN_DOXYGEN

template <typename IntType>
class LoopRangeIter;

template <typename IntType>
class LoopRangeImpl {
public:
    inline LoopRangeImpl (IntType start, IntType end)
    : m_start(start), m_end(end)
    {}
    
    inline LoopRangeImpl (IntType end)
    : m_start(0), m_end(end)
    {}
    
    inline LoopRangeIter<IntType> begin () const
    {
        return LoopRangeIter<IntType>(m_start);
    }
    
    inline LoopRangeIter<IntType> end () const
    {
        return LoopRangeIter<IntType>(m_end);
    }
    
private:
    IntType m_start;
    IntType m_end;
};

template <typename IntType>
class LoopRangeIter {
public:
    inline LoopRangeIter (IntType value)
    : m_value(value)
    {}
    
    inline IntType operator* () const
    {
        return m_value;
    }
    
    inline bool operator!= (LoopRangeIter const &other) const
    {
        return m_value != other.m_value;
    }
    
    inline LoopRangeIter & operator++ ()
    {
        m_value++;
        return *this;
    }
    
private:
    IntType m_value;
};

#endif

/**
 * Loop through integers in the range [0, `end`).
 * 
 * This should be used with the range-based for loop, for example:
 * 
 * ```
 * for (auto x : LoopRange(5)) {
 *    // x is an int with values 0, 1, ..., 4.
 * }
 * ```
 * 
 * @tparam IntType Integer type or generally a copy-constructible type which supports
 *         `x++`, `x != y` and initialization from 0.
 * @param end End of the range, not included.
 * @return Object to be used in a range-based for loop.
 */
template <typename IntType>
LoopRangeImpl<IntType> LoopRange (IntType end)
{
    return LoopRangeImpl<IntType>(end);
}

/**
 * Loop through integers in the range [`start`, `end`).
 * 
 * This should be used with the range-based for loop, for example:
 * 
 * ```
 * for (auto x : LoopRange(2, 8)) {
 *    // x is an int with values 2, 3, ..., 7.
 * }
 * ```
 * 
 * @tparam IntType Integer type or generally a copy-constructible type which supports
 *         `x++` and `x != y`.
 * @param start Start of the range.
 * @param end End of the range, not included.
 * @return Object to be used in a range-based for loop.
 */
template <typename IntType>
LoopRangeImpl<IntType> LoopRange (IntType start, IntType end)
{
    return LoopRangeImpl<IntType>(start, end);
}

/** @} */

}

#endif
