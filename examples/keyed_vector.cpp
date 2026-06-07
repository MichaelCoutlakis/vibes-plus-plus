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

static std::string plugin_id(const plugin &p) { return p.id; }

int main()
{
    // Pointer-to-data-member — no free function needed
    vpp::keyed_vector kv{&plugin::id};

    kv.insert({"audio",   "Audio processing plugin"});
    kv.insert({"video",   "Video rendering plugin"});
    kv.insert({"network", "Network transport plugin"});

    // Duplicate insert — existing entry is returned, nothing changes
    auto [it, inserted] = kv.insert({"audio", "duplicate"});
    std::cout << "insert duplicate: inserted=" << inserted << " existing=\"" << it->description
              << "\"\n";

    // Overwrite an existing entry in-place, preserving order
    kv.insert_or_assign({"video", "Video rendering plugin (v2)"});

    // Iterate in insertion order
    std::cout << "\nPlugins (insertion order):\n";
    for(const auto &p : kv)
        std::cout << "  [" << p.id << "] " << p.description << "\n";

    // Lookup
    if(const plugin *p = kv.find_ptr("network"))
        std::cout << "\nFound: " << p->description << "\n";

    // Erase
    kv.erase("audio");
    std::cout << "\nAfter erasing 'audio': " << kv.size() << " plugin(s) remain\n";
    return EXIT_SUCCESS;
}
