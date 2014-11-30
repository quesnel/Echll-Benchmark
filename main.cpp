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
#include <vle/mpi-synchronous.hpp>
#include <vle/vle.hpp>
#include <chrono>
#include "defs.hpp"
#include "timer.hpp"
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
                 "              0 means, no duration\n"
                 "  -c integer  Assigning number of run by simulation\n"
                 "  -t integer  0: no thread\n"
                 "              1: using thread for root\n"
                 "              2: using thread for sub-coupled model\n"
                 "              3: all is threaded\n"
                 "  -q integer  Assigning a default level 0 = no verbose,\n"
                 "              3 = fill terminal mode\n"
                 "  -o file     Output results into output file `file'."
                 "              (Default is standard output)\n"
                 "  -s begin,duration Assign the begin and the duration of\n"
                 "              the simulation. Default 0,10\n"
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

    double simulation_begin = 0.0;
    double simulation_duration = 10.0;
    long int duration = 100;
    long int counter = 1;
    int verbose_mode = 0;
    bool use_thread_root = false;
    bool use_thread_sub = false;
    FILE *output = stdout;

    void print(const vle::Context& ctx)
    {
        vle_info(ctx,
                 "Flags:\n"
                 "- simulation begin at: %f\n"
                 "- simulation duration: %f\n"
                 "- duration: %ld ms\n"
                 "- counter: %ld runs\n"
                 "- use threaded root: %d\n"
                 "- use threaded coupled: %d\n",
                 simulation_begin, simulation_duration,
                 duration, counter, use_thread_root, use_thread_sub);
    }

    ~main_parameter()
    {
        if (output && output != stdout)
            std::fclose(output);
    }
};

static main_parameter main_getopt(const vle::Context& ctx,
                                  int argc, char* argv[])
{
    main_parameter ret;
    int opt;

    while ((opt = ::getopt(argc, argv, "vhq:d:c:t:o:s:")) != -1) {
        switch (opt) {
        case 'v':
            main_show_version();
            break;
        case 'h':
            main_show_help();
            break;
        case 'q':
            {
                char *nptr;
                ret.verbose_mode = ::strtol(::optarg, &nptr, 10);
                if (nptr == ::optarg) {
                    std::fprintf(stderr,
                                 "-q: Failed to convert %s into verbose mode"
                                 " (integer)\n", ::optarg);
                    exit(EXIT_FAILURE);
                }

                if (ret.verbose_mode < 0) {
                    std::fprintf(stderr, "-q: Can not assign negative verbose"
                                 " mode\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
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

                if (ret.duration < 0) {
                    std::fprintf(stderr, "-d: Can not assign a negative duration\n");
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
        case 'o':
            {
                char *output = ::optarg;
                FILE *file = std::fopen(output, "w");
                if (!file) {
                    std::fprintf(stderr, "-o: Failed to open output file %s",
                                 output);
                    exit(EXIT_FAILURE);
                }

                ret.output = file;
                break;
            }
        case 's':
            {
                char *nptr;
                errno = 0;
                ret.simulation_begin = strtod(::optarg, &nptr);
                if (errno) {
                    std::fprintf(stderr, "-s: Failed to convert %s into"
                                 " two reals (double,double)\n", ::optarg);
                    exit(EXIT_FAILURE);
                }

                if (nptr != NULL) {
                    if (*nptr != ',') {
                        std::fprintf(stderr, "-s: Failed to convert %s into "
                                     " two reals (double,double)\n", ::optarg);
                        exit(EXIT_FAILURE);
                    }
                    nptr++;
                    errno = 0;
                    ret.simulation_duration = strtod(nptr, &nptr);
                    if (errno) {
                        std::fprintf(stderr, "-s: Failed to convert %s into"
                                     " two reals (double, double)\n", ::optarg);
                        exit(EXIT_FAILURE);
                    }
                }

                if (ret.simulation_duration <= 0.0) {
                    std::fprintf(stderr, "-s: simulation duration must be a"
                                 " positive real not null\n");
                    exit(EXIT_FAILURE);
                }

                break;
            }
        }
    }

    if (::optind >= argc) {
        std::fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }

    ctx->set_user_data(ret.simulation_duration);

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
main_factory_new(const vle::Context& ctx,
                 const main_parameter& mp,
                 bool mpi_mode_and_root)
{
    std::shared_ptr <bench::Factory> ret = std::make_shared <bench::Factory>();

    typedef bench::Factory::modelptr modelptr;

    ret->functions.emplace("normal",
                           [&ctx]() -> modelptr
                           {
                               return modelptr(new bench::NormalPixel(ctx));
                           });
    ret->functions.emplace("top",
                           [&ctx]() -> modelptr
                           {
                               return modelptr(new bench::TopPixel(ctx));
                           });

    if (mpi_mode_and_root) {
        ret->functions.emplace("coupled",
                               [&ctx]() -> modelptr
                               {
                                   return modelptr(
                                       new bench::SynchronousProxyModel(ctx));
                               });
    } else {
        if (mp.use_thread_sub) {
            ret->functions.emplace("coupled",
                                   [&ctx]() -> modelptr
                                   {
                                       return modelptr(
                                           new bench::CoupledThread(ctx));
                                   });
        } else {
            ret->functions.emplace("coupled",
                                   [&ctx]() -> modelptr
                                   {
                                       return modelptr(
                                           new bench::CoupledMono(ctx));
                                   });
        }
    }

    return std::move(ret);
}

struct Sample
{
    struct result
    {
        double mean;
        double variance;
        double standard_deviation;
    };

    Sample(std::size_t nb)
        : sample(nb)
    {}

    Sample::result compute() const
    {
        Sample::result ret;

        ret.mean = std::accumulate(sample.cbegin(), sample.cend(), 0.0,
                                   [](double init, double duration)
                                   {
                                       return init + duration;
                                   }) / static_cast <double>(sample.size());

        ret.variance = std::accumulate(sample.cbegin(), sample.cend(), 0.0,
                                       [&ret](double init, double duration)
                                       {
                                           return init + std::pow(duration -
                                                                  ret.mean,
                                                                  2.0);
                                       }) / static_cast <double>(sample.size());

        ret.standard_deviation = std::sqrt(ret.variance);

        return std::move(ret);
    }

    std::vector <double> sample;
};

static int main_mono_mode(const vle::Context& ctx, int argc, char *argv[])
{
    main_parameter mp = main_getopt(ctx, argc, argv);

    vle_info(ctx, "No MPI mode activated\n");
    mp.print(ctx);

    std::shared_ptr <bench::Factory> factory = main_factory_new(ctx, mp, false);
    vle::CommonPtr common = main_common_new(mp.duration, factory);

    for (int i = ::optind; i < argc; ++i) {
        vle_info(ctx, "Run for %s\n", argv[i]);

        common->at("tgf-filesource") = std::string(argv[i]);

        double total_duration = 0.0;
        Sample sample(mp.counter);

        for (long int run = 0; run < mp.counter; ++run) {
            bench::DSDE dsde_engine(common);

            if (mp.use_thread_root) {             // TODO improve !
                bench::Timer timer(&sample.sample[run]);
                bench::RootThread root(ctx);
                vle::Simulation <bench::DSDE> sim(ctx, dsde_engine, root);
                sim.run(mp.simulation_begin,
                        mp.simulation_duration + mp.simulation_begin);
            } else {
                bench::Timer timer(&sample.sample[run]);
                bench::RootMono root(ctx);
                vle::Simulation <bench::DSDE> sim(ctx, dsde_engine, root);
                sim.run(mp.simulation_begin,
                        mp.simulation_duration + mp.simulation_begin);
            }

            if (sample.sample[run] < 0.0) {
                vle_info(ctx, "Simulation failure\n");
                return -ECANCELED;
            }

            total_duration += sample.sample[run];
        }

        auto result = sample.compute();

        std::fprintf(mp.output, "%f;%f;%f;%f\n",
                     total_duration, result.mean, result.variance,
                     result.standard_deviation);
    }

    return 0;
}

static int main_mpi_mode(const vle::Context& ctx, int rank, int size,
                         int argc, char *argv[])
{
    main_parameter mp = main_getopt(ctx, argc, argv);

    if (rank == 0) {
        vle_info(ctx, "MPI mode activated: %d/%d\n", rank, size);
        mp.print(ctx);

        std::shared_ptr <bench::Factory> factory = main_factory_new(ctx, mp, true);
        vle::CommonPtr common = main_common_new(mp.duration, factory);

        common->at("tgf-filesource") = std::string(argv[::optind]);
        bench::DSDE dsde_engine(common);

        if (mp.use_thread_root) {
            bench::RootMPIThread root(ctx);
            vle::Simulation <bench::DSDE> sim(ctx, dsde_engine, root);
            sim.run(mp.simulation_begin,
                    mp.simulation_duration - mp.simulation_begin);
        } else {
            bench::RootMPIMono root(ctx);
            vle::Simulation <bench::DSDE> sim(ctx, dsde_engine, root);
            sim.run(mp.simulation_begin,
                    mp.simulation_duration - mp.simulation_begin);
        }
    } else {
        vle_info(ctx, "Need to start SynchronousProxyModel %d", rank);
        std::shared_ptr <bench::Factory> factory = main_factory_new(ctx, mp, false);
        vle::CommonPtr common = main_common_new(mp.duration, factory);

        common->operator[]("name") = vle::stringf("S%d", rank - 1);
        common->operator[]("tgf-filesource") = vle::stringf("S%d.tgf", rank - 1);

        bench::SynchronousLogicalProcessor sp(common);
        bench::Factory::modelptr coupled = factory->get("coupled");
        sp.parent = 0;
        sp.run(*coupled);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(false);

    boost::mpi::environment env(argc, argv);
    boost::mpi::communicator comm;

    vle::Context ctx = std::make_shared <vle::ContextImpl>();
    ctx->set_log_priority(3);
    ctx->set_thread_number(8);

    int ret;

    if (comm.size() == 1)
        ret = main_mono_mode(ctx, argc, argv);
    else
        ret = main_mpi_mode(ctx, comm.rank(), comm.size(), argc, argv);

    return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
