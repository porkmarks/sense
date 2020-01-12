#pragma once

#include <cmath>
#include <numeric>
#include <limits>

namespace util
{

namespace dsp
{

template<class T> bool equals(T const& a, T const& b)
{
    return a == b;
}

template<> inline bool equals(float const& a, float const& b)
{
    return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

template<class T> void apply_coefficients(T& x, T& w0, T& w1, T& w2, double d1, double d2, double A)
{
    w0 = static_cast<T>(d1*w1 + d2*w2 + x);
    x = static_cast<T>(A*(w0 + 2.0*w1 + w2));
    w2 = w1;
    w1 = w0;
}

}



template<class T>
class Butterworth
{
    Butterworth(Butterworth<T> const&) = delete;
    Butterworth<T>& operator=(Butterworth<T> const&) = delete;
public:
    Butterworth() = default;

    bool setup(size_t order, float rate, float cutoff_frequency)
    {
        if (rate <= 0 ||
                cutoff_frequency <= 0 ||
                cutoff_frequency >= rate / 2.f)
        {
            return false;
        }
        //        m_dsp.setup(poles, rate, cutoff_frequency);
        m_rate = rate;
        m_order = order;

        //        printf("  n = filter order 2,4,6,...\n");
        //        printf("  s = sampling frequency\n");
        //        printf("  f = half power frequency\n");

        //        double s = rate;
        //        double f = cutoff_frequency;
        double a = std::tan(M_PI*cutoff_frequency/rate);

        double a2 = a*a;
        A.resize(m_order);
        d1.resize(m_order);
        d2.resize(m_order);
        w0.resize(m_order);
        w1.resize(m_order);
        w2.resize(m_order);

        for(size_t i = 0; i < m_order; ++i)
        {
            double r = sin(M_PI*(2.0*i+1.0)/(4.0*m_order));
            double s = a2 + 2.0*a*r + 1.0;
            A[i] = a2/s;
            d1[i] = 2.0*(1.0-a2)/s;
            d2[i] = -(a2 - 2.0*a*r + 1.0)/s;
        }

        return true;
    }

    void reset(T const& t)
    {
        for(size_t i = 0; i < m_order; ++i)
        {
            w0[i] = t;
            w1[i] = t;
            w2[i] = t;
        }
        size_t count = std::max<size_t>(static_cast<size_t>(m_rate * 1000.f), 10000);
        for (size_t i = 0; i < count; i++)
        {
            T copy = t;
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            process(copy);
            if (dsp::equals(copy, t))
            {
                break;
            }
        }
    }
    void reset()
    {
        auto t = m_last; //need to make a copy as reset will change it
        reset(t);
    }

    void process(T& t)
    {
        if (m_needs_reset)
        {
            m_needs_reset = false;
            reset(t);
        }
        for (size_t i = 0; i < m_order; ++i)
        {
            dsp::apply_coefficients(t, w0[i], w1[i], w2[i], d1[i], d2[i], A[i]);
        }
        m_last = t;
    }

private:
    size_t m_order = 0;
    float m_rate = 0;
    bool m_needs_reset = true;
    std::vector<double> A;
    std::vector<double> d1;
    std::vector<double> d2;
    std::vector<T> w0;
    std::vector<T> w1;
    std::vector<T> w2;
    T m_last;
};



}
