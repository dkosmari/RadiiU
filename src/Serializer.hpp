/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <filesystem>
#include <stdexcept>
#include <string>

#include <glaze/json.hpp>
#include <glaze/exceptions/json_exceptions.hpp>


namespace Serializer {

    inline constexpr
    const glz::opts glz_options{
        .error_on_unknown_keys = false,
        .prettify = true,
    };


    template<typename T>
    void
    load(T& obj,
         const std::filesystem::path& filename)
    {
        std::filesystem::path filename_old = filename.string() + ".old";
        if (exists(filename))
            glz::ex::read_file_json<glz_options>(obj, filename.c_str(), std::string{});
        else if (exists(filename_old))
            glz::ex::read_file_json<glz_options>(obj, filename_old.c_str(), std::string{});
        else
            throw std::runtime_error{"\"" + filename.string() + "\" not found"};
    }


    template<typename T>
    void
    save(const T& obj,
         const std::filesystem::path& filename)
    {
        std::filesystem::path filename_new = filename.string() + ".new";
        glz::ex::write_file_json<glz_options>(obj, filename_new.c_str(), std::string{});

#ifdef __WIIU__
        // WORKAROUND: wut+newlib cannot rename when destination file already exists.
        std::filesystem::path filename_old = filename.string() + ".old";
        if (exists(filename_old))
            remove(filename_old);
        rename(filename, filename_old);
        rename(filename_new, filename);
        remove(filename_old);
#else
        rename(filename_new, filename);
#endif
    }

} // namespace Serializer

#endif
