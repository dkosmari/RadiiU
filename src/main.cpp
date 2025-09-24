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
    int status = 0;

    try {
        App::initialize();
        App::run();
    }
    catch (std::exception& e) {
        cout << "ERROR: " << e.what() << endl;
        status = -1;
    }
    App::finalize();
    cout << "Exiting..." << endl;
    return status;
}
