/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <fmt/base.h>
#include <fmt/printf.h>
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

int main()
{
    example_bind_front();
    return 0;
}