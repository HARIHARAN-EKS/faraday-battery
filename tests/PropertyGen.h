#pragma once

// Tiny deterministic generator harness for property-based tests.
// Fixed seeds => reproducible failures; no external dependencies.

#include <QList>
#include <QString>
#include <cstdint>
#include <random>

namespace faraday {
namespace propgen {

class Gen
{
public:
    explicit Gen(uint32_t seed) : m_rng(seed) {}

    quint32 u32() { return m_rng(); }
    quint32 u32Below(quint32 bound) // [0, bound)
    {
        return std::uniform_int_distribution<quint32>(0, bound - 1)(m_rng);
    }
    quint32 u32In(quint32 lo, quint32 hi)
    {
        return std::uniform_int_distribution<quint32>(lo, hi)(m_rng);
    }
    qint32 i32In(qint32 lo, qint32 hi)
    {
        return std::uniform_int_distribution<qint32>(lo, hi)(m_rng);
    }
    double dblIn(double lo, double hi)
    {
        return std::uniform_real_distribution<double>(lo, hi)(m_rng);
    }
    bool chance(double p) { return dblIn(0.0, 1.0) < p; }

private:
    std::mt19937 m_rng;
};

// Boundary pools: the values most likely to break integer/double math.
inline const QList<quint32> &u32Boundaries()
{
    static const QList<quint32> pool = {
        0u, 1u, 2u, 99u, 100u, 42581u, 65535u,
        0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu
    };
    return pool;
}

inline const QList<qint32> &i32Boundaries()
{
    static const QList<qint32> pool = {
        INT32_MIN, -1000000, -1, 0, 1, 8000, 65535, INT32_MAX
    };
    return pool;
}

} // namespace propgen
} // namespace faraday
