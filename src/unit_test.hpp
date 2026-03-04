#ifndef UNIT_TEST_HPP
#define UNIT_TEST_HPP

#include <iostream>

using std::cout;
using std::endl;


#define ANSI_FG_GREEN "\e[1;32m"
#define ANSI_FG_RED "\e[1;31m"
#define ANSI_RESET "\e[0m"


#define CHECK_EQUAL(x, y)                                               \
    do {                                                                \
        const auto x_val = x;                                           \
        const auto y_val = y;                                           \
        ++total;                                                        \
        if (x_val == y_val) {                                           \
            ++successes;                                                \
            cout << "    " ANSI_FG_GREEN "PASSED" ANSI_RESET << endl;   \
        } else {                                                        \
            cout << "    " ANSI_FG_RED "FAILED:" ANSI_RESET " "         \
                 << #x << " (" << x_val << ")"                          \
                 << " != "                                              \
                 << #y << " (" << y_val << ")"                          \
                 << endl;                                               \
        }                                                               \
    } while (false)

#define CHECK_EXCEPT(expr, exc)                                                 \
    do {                                                                        \
        ++total;                                                                \
        try {                                                                   \
            expr;                                                               \
            cout << "    " ANSI_FG_GREEN "FAILED: " ANSI_RESET "\"" << #expr    \
                 << "\" should have thrown " << #exc                            \
                 << endl;                                                       \
        }                                                                       \
        catch (exc& e) {                                                        \
            ++successes;                                                        \
            cout << "    " ANSI_FG_GREEN "PASSED:" ANSI_RESET " exception: "    \
                 << e.what()                                                    \
                 << endl;                                                       \
        }                                                                       \
    } while (false)


#endif
