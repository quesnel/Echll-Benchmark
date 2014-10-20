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

#include "linpackc.hpp"
#include "linpackc.h"
#include <thread>

namespace bench {

struct linpackc
{
    std::thread worker;
    char *mempool;
    bool force_end;

    linpackc()
        : worker(&linpackc::linpackc_run, this)
        , mempool(nullptr)
        , force_end(false)
    {}

    ~linpackc()
    {
        join();
        delete mempool;
    }

    void join()
    {
        if (worker.joinable())
            worker.join();
    }

    void set_end()
    {
        force_end = true;
    }

    void linpackc_run()
    {
        int arsize = 200;
        arsize /= 2;
        arsize *= 2;

        long arsize2d = (long)arsize * (long)arsize;

        size_t memreq = arsize2d*sizeof(double) + (long)arsize*sizeof(double)
         + (long)arsize * sizeof(int);

        mempool = new char[memreq];
        long nreps = 1;

        while (not force_end)
            ::linpackc_run(nreps, arsize, mempool);

        delete mempool;
        mempool = nullptr;
    }
};

void sleep_and_work(long int duration)
{
    bench::linpackc lp;
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    lp.set_end();
}

}
