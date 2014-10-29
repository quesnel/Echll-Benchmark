/*
 * Copyright (C) 2014 INRA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "global.hpp"
#include <cerrno>

namespace bench {

static constexpr int fix_default_level(int level)
{
    return level < 0 ? 0 : level > 3 ? 3 : level;
}

static constexpr bool is_in_level(int default_level, int log_level)
{
    return default_level >= log_level;
}

logger::logger(int default_level)
    : m_default_level(fix_default_level(default_level))
    , m_fd(::fileno(stderr))
{
}

logger::~logger()
{
}

bool logger::log_to_fd(int fd)
{
    if (fd >= 0) {
        m_fd = fd;
        return true;
    }

    return false;
}

int logger::write(int level, const char* format, ...) const
{
    int ret;

    if (is_in_level(m_default_level, level)) {
        va_list ap;
        va_start(ap, format);
        ret = ::vdprintf(m_fd, format, ap);
        va_end(ap);
    } else
        ret = -ECANCELED;

    return ret;
}

int logger::write(int level, const char* format, va_list ap) const
{
    if (is_in_level(m_default_level, level))
        return ::vdprintf(m_fd, format, ap);
    else
        return -ECANCELED;
}

}
