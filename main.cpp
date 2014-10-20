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

#include <vle/dsde.hpp>
#include <vle/common.hpp>
#include <vle/mpi.hpp>
#include <vle/vle.hpp>
#include <chrono>
#include "defs.hpp"
#include "models.hpp"
#include <cstdlib>
#include <unistd.h>

static void main_show_version()
{
    std::fprintf(stdout, "Echll_Benchmark version 0.0.0\n");

    std::exit(EXIT_SUCCESS);
}

static void main_show_help()
{
    std::fprintf(stdout, "Echll_Benchmark [-v][-h][-d real]\n"
                 "Options:\n"
                 "  -h          This help\n"
                 "  -v          Version of Echll_Benchmark\n"
                 "  -d integer  Assigning minimal duration to internal transition\n"
                 "  -c integer  Assigning number of run by simulation\n"
                 "  -t integer  0: no thread\n"
                 "              1: using thread for root\n"
                 "              2: using thread for sub-coupled model\n"
                 "              3: all is threaded\n"
                 "\n"
                 "Examples:\n"
                 "$ Echll_Benchmark -d 100 -c 42 -t 3 root.tgf\n"
                 " -> Launch root.tgf 42 times and 100ms in internal transition"
                 " of atomic models using a linpack mode. Each coupled model uses"
                 " a thread to parallelize DSDE's bags\n\n"
                 "$ Echll_Benchmark root.tgf\n"
                 " -> Launch root.tgf 1 time and using 100 ms in internal transition"
                 " of atomic models. Coupled models uses mono thread approach\n\n");

    std::exit(EXIT_SUCCESS);
}

struct main_parameter
{
    main_parameter() = default;

    long int duration = 100;
    long int counter = 1;
    bool use_thread_root = false;
    bool use_thread_sub = false;

    void print()
    {
        std::fprintf(stdout,
                     "Flags:\n"
                     "- duration: %ld ms\n"
                     "- counter: %ld runs\n"
                     "- use threaded root: %d\n"
                     "- use threaded coupled: %d\n",
                     duration, counter, use_thread_root, use_thread_sub);
    }
};

static main_parameter main_getopt(int argc, char* argv[])
{
    main_parameter ret;
    int opt;

    while ((opt = ::getopt(argc, argv, "vhd:c:t:")) != -1) {
        switch (opt) {
        case 'v':
            main_show_version();
            break;
        case 'h':
            main_show_help();
            break;
        case 'd':
            {
                char *nptr;
                ret.duration = ::strtol(::optarg, &nptr, 10);
                if (nptr == ::optarg) {
                    std::fprintf(stderr,
                                 "-d: Failed to convert %s into millisecond"
                                 " (integer)\n", ::optarg);
                    exit(EXIT_FAILURE);
                }

                if (ret.duration <= 0) {
                    std::fprintf(stderr, "-d: Can not assign zero or a"
                                 " negative duration\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
        case 'c':
            {
                char *nptr;
                ret.counter = ::strtol(::optarg, &nptr, 10);
                if (nptr == ::optarg) {
                    std::fprintf(stderr, "-c: Failed to convert %s into"
                                 " a number (integer)\n", ::optarg);
                    exit(EXIT_FAILURE);
                }

                if (ret.counter <= 0) {
                    std::fprintf(stderr, "-c: Can not assign zero or a negative"
                                 " counter\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
        case 't':
            {
                char *nptr;
                int threadmode = ::strtol(::optarg, &nptr, 10);
                if (nptr == ::optarg) {
                    std::fprintf(stderr, "-c: Failed to convert %s into"
                                 " a number (integer)\n", ::optarg);
                    exit(EXIT_FAILURE);
                }

                if (threadmode < 0 || threadmode > 3) {
                    std::fprintf(stderr, "-t: thread mode [0..3]\n");
                    exit(EXIT_FAILURE);
                }
                ret.use_thread_root = (threadmode == 1 || threadmode == 3);
                ret.use_thread_sub = (threadmode >= 2);
                break;
            }
            break;
        }
    }

    if (::optind >= argc) {
        std::fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }

    return std::move(ret);
}

static vle::CommonPtr main_common_new(long int duration,
                                      std::shared_ptr <bench::Factory> factory)
{
    std::shared_ptr <vle::Common> ret = std::make_shared <vle::Common>();

    ret->emplace("duration", duration);
    ret->emplace("name", std::string("name"));
    ret->emplace("tgf-factory", factory);
    ret->emplace("tgf-source", (int)0);
    ret->emplace("tgf-format", (int)1);
    ret->emplace("tgf-filesource", std::string());

    return std::move(ret);
}

static std::shared_ptr <bench::Factory>
main_factory_new(const main_parameter& mp, bool mpi_mode_and_root)
{
    std::shared_ptr <bench::Factory> ret = std::make_shared <bench::Factory>();

    typedef bench::Factory::modelptr modelptr;

    ret->functions.emplace("normal",
                           []() -> modelptr
                           {
                               return modelptr(new bench::NormalPixel);
                           });
    ret->functions.emplace("top",
                           []() -> modelptr
                           {
                               return modelptr(new bench::TopPixel);
                           });

    if (mpi_mode_and_root) {
        ret->functions.emplace("coupled",
                               []() -> modelptr
                               {
                                   std::printf("build a SynchronousProxyModel\n");
                                   return modelptr(new bench::SynchronousProxyModel);
                               });
    } else {
        if (mp.use_thread_sub) {
            ret->functions.emplace("coupled",
                                   []() -> modelptr
                                   {
                                       std::printf("build a CoupledThread\n");
                                       return modelptr(new bench::CoupledThread);
                                   });
        } else {
            ret->functions.emplace("coupled",
                                   []() -> modelptr
                                   {
                                       std::printf("build a CoupledMono\n");
                                       return modelptr(new bench::CoupledMono);
                                   });
        }
    }

    return std::move(ret);
}

static void main_mono_mode(int argc, char *argv[])
{
    main_parameter mp = main_getopt(argc, argv);

    std::fprintf(stdout, "No MPI mode activated\n");
    mp.print();

    std::shared_ptr <bench::Factory> factory = main_factory_new(mp, false);
    vle::CommonPtr common = main_common_new(mp.duration, factory);

    for (int i = ::optind; i < argc; ++i) {
        std::fprintf(stdout, "Run for %s\n", argv[i]);

        common->at("tgf-filesource") = std::string(argv[i]);
        auto start = std::chrono::steady_clock::now();

        std::ifstream ifs(argv[i]);
        if (not ifs) {
            std::fprintf(stdout, "File %s: can not be read\n", argv[i]);
            continue;
        }

        for (long int run = 0; run < mp.counter; ++run) {
            ifs.seekg(0, ifs.beg);

            bench::DSDE dsde_engine(common);

            if (mp.use_thread_root) {             // TODO improve !
                bench::RootThread root;
                vle::Simulation <bench::DSDE> sim(dsde_engine, root);
                sim.run(0.0, 10.0);
            } else {
                bench::RootMono root;
                vle::Simulation <bench::DSDE> sim(dsde_engine, root);
                sim.run(0.0, 10.0);
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;

        double v = std::chrono::duration <double, std::milli>(diff).count();
        std::fprintf(stdout, "- Total simulations duration: %f ms\n", v);

        if (mp.counter > 1) {
            double vc = v / mp.counter;
            std::fprintf(stdout, "- Simulation average duration: %f ms", vc);
        }
    }
}

static void main_mpi_mode(int rank, int size, int argc, char *argv[])
{
    main_parameter mp = main_getopt(argc, argv);

    std::fprintf(stdout, "I am %d\n", rank);

    if (rank == 0) {
        std::fprintf(stdout, "MPI mode activated: %d/%d\n", rank, size);
        mp.print();

        std::shared_ptr <bench::Factory> factory = main_factory_new(mp, true);
        vle::CommonPtr common = main_common_new(mp.duration, factory);

        common->at("tgf-filesource") = std::string(argv[::optind]);
        bench::DSDE dsde_engine(common);

        if (mp.use_thread_root) {
            bench::RootMPIThread root;
            vle::Simulation <bench::DSDE> sim(dsde_engine, root);
            sim.run(0.0, 10.0);
        } else {
            bench::RootMPIMono root;
            vle::Simulation <bench::DSDE> sim(dsde_engine, root);
            sim.run(0.0, 10.0);
        }
    } else {
        std::fprintf(stdout, "Need to start SynchronousProxyModel %d", rank);
        std::shared_ptr <bench::Factory> factory = main_factory_new(mp, false);
        vle::CommonPtr common = main_common_new(mp.duration, factory);

        common->operator[]("name") = vle::stringf("s%d", rank - 1);
        common->operator[]("tgf-filesource") = vle::stringf("s%d.tgf", rank - 1);

        std::string str = vle::common_get <std::string>(*common.get(), "tgf-filesource");
        std::printf("SynchronousLogicalProcessor: %s\n", str.c_str());

        bench::SynchronousLogicalProcessor sp(common);
        bench::Factory::modelptr coupled = factory->get("coupled");
        sp.parent = 0;
        sp.run(*coupled);
    }
}

int main(int argc, char *argv[])
{
    boost::mpi::environment env(argc, argv);
    boost::mpi::communicator comm;

    if (comm.size() == 1) {
        main_mono_mode(argc, argv);
    } else {
        main_mpi_mode(comm.rank(), comm.size(), argc, argv);
    }

    return EXIT_SUCCESS;
}
