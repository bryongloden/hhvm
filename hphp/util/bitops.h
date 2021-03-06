/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2016 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_BITOPS_H_
#define incl_HPHP_BITOPS_H_

#include <bitset>

#if !defined(__x86_64__) && !defined(__aarch64__)
#include <folly/Bits.h>
#endif

namespace HPHP {

// GLIBC doesn't provide an fls primitive. Since we're rolling our own
// anyway, fix ffs's wacky offset-by-one historical implementation. These
// guys return success/failure (failure for input of all zeros) and the
// unoffset bit position in their reference param.
template<typename I64, typename J64>
inline bool ffs64(I64 input, J64 &out) {
  bool retval = false;
#if defined(__x86_64__)
  asm volatile (
    "bsfq  %2, %1\n\t"   // bit scan forward
    "setnz %0\n\t":      // zero retval if input == 0
    "=r"(retval), "=r"(out):
    "r"(input):
    "cc"
  );
#elif defined(__aarch64__)
  asm volatile (
    "rbit  %2, %2\n\t"  // reverse bits
    "clz   %1, %2\n\t"  // count leading zeros
    "cmp   %1, #64\n\t"
    "cset  %0, NE":     // return (result != 64)
    "=r"(retval), "=r"(out), "+r"(input):
    :
    "cc"
  );
#elif defined(__powerpc64__)
  // In PowerPC 64, bit 0 is the most significant
  asm volatile (
    "neg    %0, %2\n\t"     // 2-complement of input, using retval as temp
    "and    %0, %2, %0\n\t"
    "cntlzd %1, %0\n\t"     // count leading zeros (starting from index 0)
    "cmpdi  %1, 64\n\t"
    "li     %0, 1\n\t"      // using retval as temp
    "iseleq %0, 0, %0\n\t"  // (input == 0) ? 0 : 1
    "neg    %1, %1\n\t"
    "addi   %1, %1, 63\n\t":// 63 - amount of leading zeros -> position in LSB
    "+r"(retval), "=r"(out):// +r else %0 and %2 will be the same register
    "r"(input):
    "cr0"
  );
#else
  out = folly::findFirstSet(input);
  retval = input != 0;
#endif
  return retval;
}

template<typename I64, typename J64>
inline bool fls64(I64 input, J64 &out) {
  bool retval;
#if defined(__x86_64__)
  asm volatile (
    "bsrq  %2, %1\n\t"   // bit scan reverse
    "setnz %0\n\t":      // zero retval if input == 0
    "=r"(retval), "=r"(out):
    "r"(input):
    "cc"
  );
#elif defined(__aarch64__)
  asm volatile (
    "clz   %1, %2\n\t"      // count leading zeros
    "neg   %1, %1\n\t"
    "adds  %1, %1, #63\n\t" // result = 63 - (# of leading zeros)
                            // "s" suffix sets condition flags
    "cset  %0, PL":         // return (result >= 0)
                            //   because result < 0 iff input == 0
    "=r"(retval), "=r"(out):
    "r"(input):
    "cc"
  );
#elif defined(__powerpc64__)
  // In PowerPC 64, bit 0 is the most significant
  asm volatile (
    "cntlzd %1, %2\n\t"     // count leading zeros (starting from index 0)
    "cmpdi  %1, 64\n\t"
    "li     %0, 1\n\t"      // using retval as temp
    "iseleq %0, 0, %0\n\t"  // (input == 0) ? 0 : 1
    "neg    %1, %1\n\t"
    "addi   %1, %1, 63\n\t":// 63 - amount of leading zeros -> position in LSB
    "=r"(retval), "=r"(out):
    "r"(input):
    "cr0"
  );
#else
  out = folly::findLastSet(input) - 1;
  retval = input != 0;
#endif
  return retval;
}

// Return the index of the first set bit in a bitset, or bitset.size() if none.
template <size_t N>
inline size_t bitset_find_first(const std::bitset<N>& bitset) {
#if defined(__GNUC__) && !defined(__APPLE__)
  // GNU provides non-standard (its a hold over from the original SGI
  // implementation) _Find_first(), which efficiently returns the index of the
  // first set bit.
  return bitset._Find_first();
#else
  for (size_t i = 0; i < bitset.size(); ++i) {
    if (bitset[i]) return i;
  }
  return bitset.size();
#endif
}

// Return the index of the first set bit in a bitset after the given index, or
// bitset.size() if none.
template <size_t N>
inline size_t bitset_find_next(const std::bitset<N>& bitset, size_t prev) {
  assert(prev < bitset.size());
#if defined(__GNUC__) && !defined(__APPLE__)
  // GNU provides non-standard (its a hold over from the original SGI
  // implementation) _Find_next(), which given an index, efficiently returns
  // the index of the first set bit after the index.
  return bitset._Find_next(prev);
#else
  for (size_t i = prev+1; i < bitset.size(); ++i) {
    if (bitset[i]) return i;
  }
  return bitset.size();
#endif
}

// Invoke the given callable on the indices of all the set bits in a bitset.
template <typename F, size_t N>
inline void bitset_for_each_set(const std::bitset<N>& bitset, F f) {
  for (auto i = bitset_find_first(bitset);
       i < bitset.size();
       i = bitset_find_next(bitset, i)) {
    f(i);
  }
}

} // HPHP

#endif
