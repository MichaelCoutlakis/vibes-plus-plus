/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <thread>

#include <vpp/thread.hpp>

void example_ordered_pipeline()
{
    std::string output;
    auto sink = [&](std::string s) { output.append(s); };
    vpp::thread_pool pool(2);

    vpp::ordered_pipeline<std::string> pipeline(pool, sink);

    using namespace std::chrono_literals;
    pipeline.push(
        [s = "the answer"]()
        {
            std::this_thread::sleep_for(100ms);
            return s;
        });
    pipeline.push(
        [s = " is 42"]()
        {
            std::this_thread::sleep_for(1ms);
            return s;
        });

    pipeline.flush();
    std::cout << output << "\n"; // Should print "the answer is 42"
}

int main()
{
    example_ordered_pipeline();
    return 0;
}