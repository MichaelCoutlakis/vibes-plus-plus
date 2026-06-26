/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <fmt/base.h>
#include <fmt/printf.h>
#include <set>
#include <string>
#include <vpp/functional.hpp>

constexpr static inline int add(int a, int b, int c) { return a + b + c; }

void example_bind_front()
{
    struct calculator
    {
        int add(int a, int b, int c) { return a + b + c; }
    };

    calculator my_calc;
    auto my_adder = vpp::bind_front(&calculator::add, &my_calc);
    fmt::print("my_adder(1, 2, 3) = {}\n", my_adder(1, 2, 3));

    auto add2 = vpp::bind_front(add);
    fmt::print("add2(1, 2, 3) = {}\n", add2(1, 2, 3));
}

void example_compare_projection()
{

    struct sample
    {
        float time{}, value{};
        float get_time() const { return time; }
    };

    std::multiset<sample, vpp::compare_proj<&sample::get_time, std::less<>>> samples_by_time{
        {5.f, 0.f},
        {0.f, 1.f},
        {1.f, 2.f}
    };

    fmt::println("samples by time");
    for(auto &s : samples_by_time)
        fmt::println("time={}, value={}", s.time, s.value);
}

int main()
{
    example_bind_front();
    example_compare_projection();
    return 0;
}