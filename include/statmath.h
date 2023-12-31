#pragma once

#include <cstdlib>

template<class float_t=float>
class EWMA {
/**
 * Implement exponential weighted moving average
 */

public:
    const float_t alpha;
    float_t y = std::numeric_limits<float_t>::quiet_NaN();

    explicit EWMA(uint32_t span) : alpha(2.f / (float_t) (span + 1)) {}

    inline void add(float_t x) {
        if (isnan(x)) return;
        if (isnan(y)) y = x;
        y = (1 - alpha) * y + alpha * x;
    }

    inline float_t get() const { return y; }
};

template<class float_t=float>
class EWM {
    float_t last_x = std::numeric_limits<float_t>::quiet_NaN();
public:
    EWMA<float_t> avg, std;

    explicit EWM(uint32_t span) : avg(span), std(span) {}

    inline void add(float_t x) {
        avg.add(x);
        if (!isnan(last_x)) {
            float_t pct = (x - last_x) / last_x;
            if (std::isfinite(pct))
                std.add(pct * pct);
        }
        last_x = x;
    }
};


template<typename T>
inline bool greaterThan(const T &a, const T &b) { return a > b; }

template<typename T, bool (*comp)(const T &, const T &)>
inline T median_of_three(const T &a, const T &b, const T &c) {
    return comp(a, b) ? (comp(b, c) ? b : (comp(a, c) ? c : a))
                      : (comp(c, b) ? b : (comp(c, a) ? c : a));
}

template<typename T>
class RunningMedian3 {
    T s1{}, s2{};
public:
    inline T next(const T &s) {
        T m = median_of_three<T, greaterThan<T>>(s, s1, s2);
        s2 = s1;
        s1 = s;
        return m;
    }
};

/*
template<typename RandomAccessIter, typename comp>
inline size_t median_of_three(const RandomAccessIter &array, size_t l, size_t m, size_t r)  {
    return comp(array[l], array[m]) ? ( comp(array[m], array[r]) ? m : ( comp( array[l], array[r]) ? r : l ) )
                                    : ( comp(array[r], array[m]) ? m : ( comp( array[r], array[l] ) ? r : l ) );
}

inline size_t pseudo_median_of_nine( const RandomAccessIterator &array, const quick_sort_range &range ) const {
    size_t offset = range.size/8u;
    return median_of_three(array,
                           median_of_three(array, 0, offset, offset*2),
                           median_of_three(array, offset*3, offset*4, offset*5),
                           median_of_three(array, offset*6, offset*7, range.size - 1) );

}
*/