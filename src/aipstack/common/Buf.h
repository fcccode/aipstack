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

#ifndef AIPSTACK_BUF_H
#define AIPSTACK_BUF_H

#include <stddef.h>
#include <string.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/Hints.h>
#include <aipstack/misc/MinMax.h>

namespace AIpStack {

/**
 * @addtogroup common
 * @{
 */

/**
 * Node in a chain of memory buffers.
 * 
 * It contains the pointer to and length of a buffer,
 * and a pointer to the next node, if any.
 */
struct IpBufNode {
    /**
     * Pointer to the buffer data.
     */
    char *ptr;
    
    /**
     * Length of the buffer.
     */
    size_t len;
    
    /**
     * Pointer to the next buffer node (or NULL).
     */
    IpBufNode const *next;
};

/**
 * Reference to a possibly discontiguous range of memory in
 * a chain of memory buffers.
 * 
 * It contains the pointer to the first buffer node,
 * the byte offset within that first buffer, and the total
 * length of the memory range.
 * 
 * Except where noted otherwise, all functions in IpBufRef
 * require the reference to be *valid*. This means that
 * @ref node is not NULL, the @ref offset points to a valid location
 * within this first buffer (pointing to the end is permitted),
 * and there is at least @ref tot_len remaining data in the first
 * buffer and subsequent buffers together.
 * 
 * Operations with these memory ranges never modify the
 * buffer nodes (@ref IpBufNode). Only the @ref IpBufRef objects
 * are changed or created to refer to different ranges of a
 * buffer chain.
 */
struct IpBufRef {
    /**
     * Pointer to the first buffer node.
     */
    IpBufNode const *node;
    
    /**
     * Byte offset in the first buffer.
     */
    size_t offset;
    
    /**
     * The total length of the data range.
     */
    size_t tot_len;
    
    /**
     * Return the length of this memory range (@ref tot_len).
     * 
     * A valid reference is not needed, this simply returns @ref tot_len
     * without any requirements.
     * 
     * @return @ref tot_len
     */
    inline size_t getTotalLength () const
    {
        return tot_len;
    }
    
    /**
     * Return the pointer to the first chunk of the memory range.
     * 
     * @return `node->ptr + offset`
     */
    inline char * getChunkPtr () const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        
        return node->ptr + offset;
    }
    
    /**
     * Return the length of the first chunk of the memory range.
     * 
     * @return `min(tot_len, node->len - offset)`
     */
    inline size_t getChunkLength () const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        
        return MinValue(tot_len, (size_t)(node->len - offset));
    }
    
    /**
     * Move to the next buffer in the memory range.
     * 
     * This decrements @ref tot_len by @ref getChunkLength(), sets @ref node to
     * `node->next` and sets @ref offset to 0. After that it returns
     * whether there is any more data in the (now modified) memory
     * range, that is @ref tot_len > 0.
     * 
     * @return Whether there is any more data after the adjustment.
     */
    bool nextChunk ()
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        
        tot_len -= MinValue(tot_len, (size_t)(node->len - offset));
        node = node->next;
        offset = 0;
        
        bool more = (tot_len > 0);
        AIPSTACK_ASSERT(!more || node != nullptr)
        
        return more;
    }
    
    /**
     * Try to extend the memory range backward in the first buffer.
     * 
     * If amount is greater than offset, returns false since
     * insufficient memory is available in the first buffer.
     * Otherwise, sets *new_ref to the memory region extended
     * to the left by amount and returns true. The *new_ref
     * will have the same node, offset decremented by amount
     * and tot_len incremented by amount.
     * 
     * @param amount Number of bytes to reveal.
     * @param new_ref Pointer to where the result will be stored (must not
     *        be null).
     * @return True on success (enough space was available, `*new_ref` was
     *         set), false on failure (`*new_ref` was not changed).
     */
    inline bool revealHeader (size_t amount, IpBufRef *new_ref) const
    {
        if (amount > offset) {
            return false;
        }
        
        *new_ref = IpBufRef {
            node,
            (size_t)(offset  - amount),
            (size_t)(tot_len + amount)
        };
        return true;
    }
    
    /**
     * Extend the memory range backward in the first buffer assuming there
     * is space.
     * 
     * @param amount Number od bytes to reveal. Must be less then or equal
     *        to @ref offset.
     * @return The adjusted memory range.
     */
    inline IpBufRef revealHeaderMust (size_t amount) const
    {
        AIPSTACK_ASSERT(amount <= offset)
        
        return IpBufRef {
            node,
            (size_t)(offset  - amount),
            (size_t)(tot_len + amount)
        };
    }
    
    /**
     * Check if there is at least `amount` bytes available
     * in the first chunk of this memory range.
     * 
     * @param amount Number of bytes to check for.
     * @return `getChunkLength() >= amount`.
     */
    inline bool hasHeader (size_t amount) const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        
        return tot_len >= amount && node->len - offset >= amount;
    }
    
    /**
     * Return a memory range without an initial portion of this memory range
     * that must be within the first chunk.
     * 
     * @param amount Number of bytes to hide. Must be less than or equal
     *        to @ref getChunkLength().
     * @return A reference with the same @ref node, @ref offset incremented
     *         by `amount` and @ref tot_len decremented by `amount`.
     */
    inline IpBufRef hideHeader (size_t amount) const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        AIPSTACK_ASSERT(amount <= node->len - offset)
        AIPSTACK_ASSERT(amount <= tot_len)
        
        return IpBufRef {
            node,
            (size_t)(offset  + amount),
            (size_t)(tot_len - amount)
        };
    }
    
    /**
     * Return an @ref IpBufNode corresponding to the first buffer
     * of the memory range with the offset applied.
     * 
     * @return An @ref IpBufNode with `ptr` equal to `node->ptr + offset`,
     *        `len` equal to `node->len - offset` and `next` equal to
     *        `node->next`.
     */
    inline IpBufNode toNode () const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        
        return IpBufNode {
            node->ptr + offset,
            (size_t)(node->len - offset),
            node->next
        };
    }
    
    /**
     * Construct a memory range consisting of an initial portion
     * of the first chunk of this memory range continued by
     * data in a specified buffer chain.
     * 
     * It sets `*out_node` to a new @ref IpBufNode referencing the
     * initial portion and continuing into the given buffer
     * chain (`ptr = node->ptr`, `len = offset + header_len`,
     * `next = cont`), and returns an @ref IpBufRef using `out_node`
     * as its first buffer (`node = out_node`, `offset = offset`,
     * `tot_len = total_len`).
     * 
     * Note that this does not "apply" the offset to the node
     * as @ref toNode does. This is to allow @ref revealHeader.
     *
     * It is important to understand that this works by creating
     * a new @ref IpBufNode, because the buffer chain model cannot
     * support this operation otherwise. The returned @ref IpBufRef
     * will be valid only so long as `out_node` remains valid.
     * 
     * @param header_len Length of the initial portion. Must be less than or
     *        equal to `node->len - offset`.
     * @param cont Pointer to the buffer node with data after the initial
     *        portion (may be null if there is no such data).
     * @param total_len Total length of the constructed memory range.
     *        Must be greater than or equal to `header_len`.
     * @param out_node Pointer to where the new @ref IpBufNode for the
     *        initial portion will be written (must not be null).
     * @return An @ref IpBufRef referencing the constructed memory range.
     */
    IpBufRef subHeaderToContinuedBy (size_t header_len, IpBufNode const *cont,
                                     size_t total_len, IpBufNode *out_node) const
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(offset <= node->len)
        AIPSTACK_ASSERT(header_len <= node->len - offset)
        AIPSTACK_ASSERT(total_len >= header_len)
        
        *out_node = IpBufNode{node->ptr, (size_t)(offset + header_len), cont};
        return IpBufRef{out_node, offset, total_len};
    }
    
    /**
     * Consume a number of bytes from the front of the memory range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param amount Number of bytes to consume. Must be less than or equal
     *        to @ref tot_len.
     */
    void skipBytes (size_t amount)
    {
        processBytes(amount, [](char *, size_t) {});
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * memory range while copying them to the given memory
     * location.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param amount Number of bytes to copy out and consume. Must be less than
     *        or equal to @ref tot_len.
     * @param dst Location to copy to. May be null only if `amount` is null.
     */
    void takeBytes (size_t amount, char *dst)
    {
        processBytes(amount, [&](char *data, size_t len) {
            ::memcpy(dst, data, len);
            dst += len;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while copying bytes from the given memory location
     * into the consumed part of the range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param amount Number of bytes to copy in and consume. Must be less than
     *        or equal to @ref tot_len.
     * @param src Location to copy from. May be null only if `amount` is null.
     */
    void giveBytes (size_t amount, char const *src)
    {
        processBytes(amount, [&](char *data, size_t len) {
            ::memcpy(data, src, len);
            src += len;
        });
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while copying bytes from another memory range
     * into the consumed part of the range.
     * 
     * The number of bytes consumed and copied is equal to
     * the length of the other memory range (`src`), and must
     * not exceed the length of this memory range.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @param src Memory range to copy in. `src.tot_len` must be less than
     *        or equal to @ref tot_len of this memory range.
     */
    void giveBuf (IpBufRef src)
    {
        processBytes(src.tot_len, [&](char *data, size_t len) {
            src.takeBytes(len, data);
        });
    }
    
    /**
     * Consume and return a single byte from the front of the
     * memory range.
     * 
     * @ref tot_len must be positive.
     * 
     * This moves to subsequent buffers eagerly (see @ref processBytes).
     * 
     * @return The consumed byte.
     */
    char takeByte ()
    {
        AIPSTACK_ASSERT(tot_len > 0)
        
        char ch;
        processBytes(1, [&](char *data, size_t len) {
            ch = *data;
        });
        return ch;
    }
    
    /**
     * Consume a number of bytes from the front of the memory
     * range while processing them with a user-specified function.
     * 
     * The function will be called on the subsequent contiguous
     * chunks of the consumed part of this memory range. It will
     * be passed two arguments: a char pointer to the start of
     * the chunk, and a size_t length of the chunk. The function
     * will not be called on zero-sized chunks.
     * 
     * This function moves forward to subsequent buffers eagerly.
     * This means that when there are no more bytes to be
     * processed, it will move to the next buffer as long as
     * it is currently at the end of the current buffer and there
     * is a next buffer.
     * 
     * This eager moving across buffer is useful when the buffer
     * chain is a ring buffer, so that the offset into the buffer
     * will remain always less than the buffer size, never becoming
     * equal.
     * 
     * @tparam Func Function object type.
     * @param amount Number of bytes to process and consume. Must be less than
     *        or equal to @ref tot_len.
     * @param func Function to call in order to process chunks (see above). The
     *        function must not modify this @ref IpBufRef object, and the state of
     *        this object at the time of invocation is unspecified.
     */
    template <typename Func>
    void processBytes (size_t amount, Func func)
    {
        AIPSTACK_ASSERT(node != nullptr)
        AIPSTACK_ASSERT(amount <= tot_len)
        
        while (true) {
            AIPSTACK_ASSERT(offset <= node->len)
            size_t rem_in_buf = node->len - offset;
            
            if (rem_in_buf > 0) {
                if (amount == 0) {
                    return;
                }
                
                size_t take = MinValue(rem_in_buf, amount);
                func(getChunkPtr(), size_t(take));
                
                tot_len -= take;
                
                if (take < rem_in_buf || node->next == nullptr) {
                    offset += take;
                    AIPSTACK_ASSERT(amount == take)
                    return;
                }
                
                amount -= take;
            } else {
                if (node->next == nullptr) {
                    AIPSTACK_ASSERT(amount == 0)
                    return;
                }
            }
            
            node = node->next;
            offset = 0;
        }
    }
    
    /**
     * Return a memory range that is a prefix of this memory range.
     * 
     * It returns an @ref IpBufRef with the same @ref node and @ref offset
     * but with @ref tot_len equal to `new_tot_len`.
     * 
     * @ref node is allowed to be null.
     * 
     * @param new_tot_len The length of the prefix memory range. Must be less than
     *        or equal to @ref tot_len.
     * @return The prefix range whose length is `new_tot_len`.
     */
    inline IpBufRef subTo (size_t new_tot_len) const
    {
        AIPSTACK_ASSERT(new_tot_len <= tot_len)
        
        return IpBufRef {
            node,
            offset,
            new_tot_len
        };
    }
    
    /**
     * Return a sub-range of the buffer reference from the given
     * offset of the given length.
     * 
     * This is implemented by calling @ref skipBytes(`offset`) on a copy
     * of this object then returning @ref subTo(`len`) of this copy.
     * 
     * @param offset Offset from the start of this memory range. Must be less than
     *        or equal to @ref tot_len.
     * @param len Length of sub-range. Must be less than or equal to @ref tot_len
     *        - `offset`.
     * @return The sub-range starting at `offset` whose length is `len`.
     */
    inline IpBufRef subFromTo (size_t offset, size_t len) const
    {
        IpBufRef buf = *this;
        buf.skipBytes(offset);
        buf = buf.subTo(len);
        return buf;
    }
};

/** @} */

}

#endif
