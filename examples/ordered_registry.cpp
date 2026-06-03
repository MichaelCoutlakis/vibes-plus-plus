/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <cstdlib>
#include <iostream>
#include <string>
#include <vpp/ordered_registry.hpp>

struct plugin
{
    std::string id;
    std::string description;
};

static std::string plugin_id(const plugin &p) { return p.id; }

int main()
{
    vpp::ordered_registry reg{&plugin_id};

    reg.insert({"audio", "Audio processing plugin"});
    reg.insert({"video", "Video rendering plugin"});
    reg.insert({"network", "Network transport plugin"});

    // Duplicate insert — existing entry is returned, nothing changes
    auto [it, inserted] = reg.insert({"audio", "duplicate"});
    std::cout << "insert duplicate: inserted=" << inserted << " existing=\"" << it->description
              << "\"\n";

    // Overwrite an existing entry in-place, preserving order
    reg.insert_or_assign({"video", "Video rendering plugin (v2)"});

    // Iterate in insertion order
    std::cout << "\nPlugins (insertion order):\n";
    for(const auto &p : reg)
        std::cout << "  [" << p.id << "] " << p.description << "\n";

    // Lookup
    if(const plugin *p = reg.find_ptr("network"))
        std::cout << "\nFound: " << p->description << "\n";

    // Erase
    reg.erase("audio");
    std::cout << "\nAfter erasing 'audio': " << reg.size() << " plugin(s) remain\n";
    return EXIT_SUCCESS;
}
