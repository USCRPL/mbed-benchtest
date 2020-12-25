//
// Created by Luka on 12/16/2020.
//

#ifndef MBED_BENCHTEST_ALIGN_H
#define MBED_BENCHTEST_ALIGN_H

#if RTXOFF_USE_32BIT
template <typename T>
constexpr uint32_t align(T x) {
    return (x + 3U) & ~3UL;
}

static inline bool is_aligned_p(void *x) {
    return (reinterpret_cast<ptrdiff_t>(x) & 3U) == 0;
}

template<typename T>
constexpr bool is_aligned(T x) {
    return (x & 3U) == 0;
}
#else

template<typename T>
constexpr inline uint64_t align(T x) {
return (x + 7U) & ~7UL;
}

static inline bool is_aligned_p(void *x) {
    return (reinterpret_cast<ptrdiff_t>(x) & 7U) == 0;
}

template<typename T>
constexpr inline bool is_aligned(T x) {
return (x & 7U) == 0;
}

#endif

#endif //MBED_BENCHTEST_ALIGN_H
