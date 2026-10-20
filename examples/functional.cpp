/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <fmt/base.h>
#include <fmt/printf.h>
#include <set>
#include <string>
#include <vpp/functional.hpp>

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
    example_compare_projection();
    return 0;
}