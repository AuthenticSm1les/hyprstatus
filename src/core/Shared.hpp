#pragma once

#include <hyprland/src/helpers/memory/Memory.hpp>

// One live instance of T shared by everyone holding it (bar modules, the
// panel), created on first use and destroyed when the last holder lets go.
template <class T>
SP<T> sharedInstance() {
    static WP<T> s_instance;
    if (auto p = s_instance.lock())
        return p;
    auto p     = makeShared<T>();
    s_instance = p;
    return p;
}
