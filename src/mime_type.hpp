/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MIME_TYPE_HPP
#define MIME_TYPE_HPP

#include <string>
#include <vector>


namespace mime_type {

    /*
     * input: must have no wildcards, no parameters
     * candidate: can have wildcards
     */

    bool
    match(const std::string& input,
          const std::string& candidate);

    /*
     * input: must have no wildcards, no parameters
     * candidate: can have wildcards
     */
    bool
    match(const std::string& input,
          const std::vector<std::string>& candidates);


} // namespace mime_type

#endif
