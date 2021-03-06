/*
 * Copyright (c) 2015 Ambroz Bizjak
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

#ifndef AIPSTACK_TYPE_LIST_UTILS_H
#define AIPSTACK_TYPE_LIST_UTILS_H

#include <type_traits>

#include <aipstack/meta/TypeList.h>
#include <aipstack/meta/TypeDict.h>
#include <aipstack/meta/BasicMetaUtils.h>
#include <aipstack/meta/FuncUtils.h>

namespace AIpStack {

/**
 * @addtogroup meta
 * @{
 */

#ifndef IN_DOXYGEN

template <typename... Ts>
struct MakeTypeListHelper;

template <>
struct MakeTypeListHelper<> {
    typedef EmptyTypeList Type;
};

template <typename T, typename... Ts>
struct MakeTypeListHelper<T, Ts...> {
    typedef ConsTypeList<T, typename MakeTypeListHelper<Ts...>::Type> Type;
};

#endif

template <typename... Ts>
using MakeTypeList =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename MakeTypeListHelper<Ts...>::Type;
#endif

#ifndef IN_DOXYGEN

template <typename List, typename Func>
struct MapTypeListHelper;

template <typename Func>
struct MapTypeListHelper<EmptyTypeList, Func> {
    typedef EmptyTypeList Type;
};

template <typename Head, typename Tail, typename Func>
struct MapTypeListHelper<ConsTypeList<Head, Tail>, Func> {
    typedef ConsTypeList<FuncCall<Func, Head>, typename MapTypeListHelper<Tail, Func>::Type> Type;
};

#endif

template <typename List, typename Func>
using MapTypeList =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename MapTypeListHelper<List, Func>::Type;
#endif

#ifndef IN_DOXYGEN

namespace Private {
    template <typename List>
    struct TypeListLengthHelper;

    template <>
    struct TypeListLengthHelper<EmptyTypeList> {
        static const int Value = 0;
    };

    template <typename Head, typename Tail>
    struct TypeListLengthHelper<ConsTypeList<Head, Tail>> {
        static const int Value = 1 + TypeListLengthHelper<Tail>::Value;
    };
}

#endif

template <typename List>
using TypeListLength =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
Private::TypeListLengthHelper<List>;
#endif

#ifndef IN_DOXYGEN

template <typename List, typename Reversed>
struct TypeListReverseHelper;

template <typename Reversed>
struct TypeListReverseHelper<EmptyTypeList, Reversed> {
    using Type = Reversed;
};

template <typename Head, typename Tail, typename Reversed>
struct TypeListReverseHelper<ConsTypeList<Head, Tail>, Reversed> {
    using Type = typename TypeListReverseHelper<Tail, ConsTypeList<Head, Reversed>>::Type;
};

#endif

template <typename List>
using TypeListReverse =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename TypeListReverseHelper<List, EmptyTypeList>::Type;
#endif

#ifndef IN_DOXYGEN

namespace Private {
    template <typename List, int Index>
    struct TypeListGetHelper;
    
    template <typename Head, typename Tail, int Index>
    struct TypeListGetHelper<ConsTypeList<Head, Tail>, Index> {
        using Result = typename TypeListGetHelper<Tail, Index - 1>::Result;
    };
    
    template <typename Head, typename Tail>
    struct TypeListGetHelper<ConsTypeList<Head, Tail>, 0> {
        using Result = Head;
    };
}

#endif

template <typename List, int Index>
using TypeListGet =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename Private::TypeListGetHelper<List, Index>::Result;
#endif

#ifndef IN_DOXYGEN

namespace Private {
    template <typename List, typename Value, int Offset>
    struct TypeListFindHelper;
    
    template <typename Tail, typename Value, int Offset>
    struct TypeListFindHelper<ConsTypeList<Value, Tail>, Value, Offset> {
        using Result = TypeDictFound<WrapInt<Offset>>;
    };
    
    template <typename Head, typename Tail, typename Value, int Offset>
    struct TypeListFindHelper<ConsTypeList<Head, Tail>, Value, Offset> {
       using Result = typename TypeListFindHelper<Tail, Value, Offset + 1>::Result; 
    };
    
    template <typename Value, int Offset>
    struct TypeListFindHelper<EmptyTypeList, Value, Offset> {
        using Result = TypeDictNotFound;
    };
}

#endif

template <typename List, typename Value>
using TypeListFind =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename Private::TypeListFindHelper<List, Value, 0>::Result;
#endif

template <typename List, typename Value>
using TypeListIndex = typename TypeListFind<List, Value>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListFindMapped = TypeListFind<MapTypeList<List, Func>, FuncValue>;

template <typename List, typename Func, typename FuncValue>
using TypeListIndexMapped = typename TypeListFindMapped<List, Func, FuncValue>::Result;

template <typename List, typename Func, typename FuncValue>
using TypeListGetMapped = TypeListGet<List, TypeListIndexMapped<List, Func, FuncValue>::Value>;

#ifndef IN_DOXYGEN

namespace SequenceListPrivate {
    template <int Count, int Start>
    struct SequenceListHelper {
        typedef ConsTypeList<
            WrapInt<Start>,
            typename SequenceListHelper<(Count - 1), (Start + 1)>::Type
        > Type;
    };

    template <int Start>
    struct SequenceListHelper<0, Start> {
        typedef EmptyTypeList Type;
    };
}

#endif

/**
 * Create a list of WrapInt\<index\> for index from Start up to
 * Start+Count exclusive.
 */
template <int Count, int Start=0>
using SequenceList =
#ifdef IN_DOXYGEN
implementation_hidden;
#else
typename SequenceListPrivate::SequenceListHelper<Count, Start>::Type;
#endif

/**
 * Create a list of ElemTemplate\<index\> for index from
 * 0 up to Count exclusive.
 */
template <int Count, template<int> class ElemTemplate>
using IndexElemListCount = MapTypeList<SequenceList<Count>, ValueTemplateFunc<int, ElemTemplate>>;

/**
 * Create a list of ElemTemplate\<index\> for index from
 * 0 up to the length of List exclusive.
 */
template <typename List, template<int> class ElemTemplate>
using IndexElemList = IndexElemListCount<TypeListLength<List>::Value, ElemTemplate>;

/** @} */

}

#endif
