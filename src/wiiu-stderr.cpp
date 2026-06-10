/*
 * Automatically redirect stderr to WHBLogWrite().
 * Copyright 2026  Daniel K. O. (dkosmari)
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * SPDX-License-Identifier: MIT
 *
 * Source: https://github.com/dkosmari/wiiu-stdout
 */

#ifdef __WIIU__

#include <coreinit/mutex.h>

#include <sys/iosupport.h>      // devoptab_list, devoptab_t


extern OSMutex* wiiu_whb_log_mutex;

namespace {

    OSMutex the_real_mutex;

} // namespace


__attribute__(( __constructor__ (101) ))
void
wiiu_init_wiiu_whb_log_mutex(void)
    noexcept
{
    OSInitMutex(&the_real_mutex);
    wiiu_whb_log_mutex = &the_real_mutex;
}


extern
ssize_t
wiiu_devoptab_to_whb_log(struct _reent*,
                         void*,
                         const char* buf,
                         size_t len)
    noexcept;


namespace {

    devoptab_t stderr_dev;

} // namespace


__attribute__(( __constructor__ (102) ))
void
wiiu_init_stderr()
    noexcept
{
    stderr_dev.name = "STDERR";
    stderr_dev.structSize = sizeof stderr_dev;
    stderr_dev.write_r = wiiu_devoptab_to_whb_log;
    devoptab_list[STD_ERR] = &stderr_dev;
}

#endif // __WIIU__
