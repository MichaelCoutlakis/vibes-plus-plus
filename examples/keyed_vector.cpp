/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/keyed_vector.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

struct plugin
{
    std::string id;
    std::string description;
};

// Type alias — the idiomatic usage pattern.
// Key (std::string) is deduced from &plugin::id automatically.
using plugin_registry = vpp::keyed_vector<plugin, &plugin::id>;

int main()
{
    // Initializer list construction
    plugin_registry reg{
        {"audio",   "Audio processing plugin"},
        {"video",   "Video rendering plugin"},
        {"network", "Network transport plugin"},
    };

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

    // Positional access via as_vec()
    std::cout << "\nFirst plugin: " << reg.as_vec()[0].id << "\n";

    // Erase
    reg.erase("audio");
    std::cout << "\nAfter erasing 'audio': " << reg.size() << " plugin(s) remain\n";
    return EXIT_SUCCESS;
}
