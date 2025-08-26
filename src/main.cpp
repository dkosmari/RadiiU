/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <iostream>

#include "App.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;


int
main(int, char* [])
{
    cout << "Started " << PACKAGE_STRING << endl;

    // cout << "mpg123 decoders:" << endl;
    // const char** decoders = mpg123_decoders();
    // for (unsigned i = 0; decoders[i]; ++i)
    //     cout << "  - " << decoders[i] << endl;

    try {
        App app;
        app.run();
    }
    catch (std::exception& e) {
        cout << "ERROR: " << e.what() << endl;
        return -1;
    }
    cout << "Exiting..." << endl;
    return 0;
}
