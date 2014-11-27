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

#ifndef __Benchmark_models_hpp__
#define __Benchmark_models_hpp__

#include "linpackc.hpp"
#include "defs.hpp"
#include <vle/mpi-synchronous.hpp>
#include <vle/utils.hpp>
#include <fstream>
#include <numeric>

namespace bench {

struct TopPixel : AtomicModel
{
    int m_id;
    std::string m_name;
    long int m_duration;

    TopPixel(const vle::Context& ctx)
        : AtomicModel(ctx, {}, {"0"})
    {}

    virtual ~TopPixel()
    {}

    virtual double init(const vle::Common& common, const double&) override final
    {
        try {
            m_id = boost::any_cast <int>(common.at("id"));
            m_name = std::string("top-") + boost::any_cast <std::string>(common.at("name"));
            m_duration = boost::any_cast <long int>(common.at("duration"));
        } catch (const std::exception &e) {
            throw std::invalid_argument("TopPixel: failed to find name "
                                        "or duration parameters");
        }

        return 0.0;
    }

    virtual double delta(const double&) override final
    {
        if (m_duration > 0)
            bench::sleep_and_work(m_duration);

        return 1.0;
    }

    virtual void lambda() const override final
    {
        vle_dbg(AtomicModel::ctx, "[%s] lambda\n", m_name.c_str());

        y[0] = {m_id};
    }
};

struct NormalPixel : AtomicModel
{
    enum Phase { WAIT, SEND };

    int          m_id;
    std::string  m_name;
    double       m_current_time;
    double       m_last_time;
    long int     m_duration;
    unsigned int m_neighbour_number;
    unsigned int m_received;
    unsigned int m_total_received;
    Phase        m_phase;
    double       m_simulation_duration;

    NormalPixel(const vle::Context& ctx)
        : AtomicModel(ctx, {"0"}, {"0"})
        , m_current_time(Infinity <double>::negative)
        , m_last_time(Infinity <double>::negative)
        , m_neighbour_number(0)
        , m_received(0)
        , m_total_received(0)
        , m_phase(WAIT)
    {
        try {
            m_simulation_duration = boost::any_cast <double>(ctx->get_user_data());
        } catch (const std::exception &e) {
            throw std::invalid_argument("Normal pixel can not read the "
                                        "simulation duration parameter");
        }
    }

    virtual ~NormalPixel()
    {
        if (m_total_received != (m_simulation_duration * m_neighbour_number)) {
            vle_dbg(AtomicModel::ctx, "/!\\ [%s] failure: have received %"
                    PRIuMAX " messages (%" PRIuMAX " expected)\n",
                    m_name.c_str(),
                    static_cast <std::uintmax_t>(m_total_received),
                    static_cast <std::uintmax_t>(m_neighbour_number * 10));
        }
    }

    virtual double init(const vle::Common& common,
                        const double& t) override final
    {
        m_current_time = t;
        m_last_time = Infinity <double>::negative;

        try {
            m_id = boost::any_cast <int>(common.at("id"));
            m_duration = boost::any_cast <long int>(common.at("duration"));
            m_name = std::string("normal-") +
                boost::any_cast <std::string>(common.at("name"));
            m_neighbour_number =
                boost::any_cast <unsigned int>(common.at("neighbour_number"));
        } catch (const std::exception &e) {
            throw std::invalid_argument("NormalPixel: failed to find duration,"
                                        "name or neighbour_number parameters");
        }

        m_received = 0;
        m_total_received = 0;
        m_phase = WAIT;

        return Infinity <double>::positive;
    }

    virtual double delta(const double& time) override final
    {
        m_current_time += time;

        if (x.empty())
            dint(m_current_time);
        else
            dext(m_current_time);

        if (m_phase == WAIT)
            return Infinity <double>::positive;

        return 0.0;
    }

    void dint(const double& time)
    {
        vle_dbg(AtomicModel::ctx, "[%s] dint at %f\n", m_name.c_str(), time);

        if (m_duration > 0)
            bench::sleep_and_work(m_duration);

        if (m_phase == SEND) {
            vle_dbg(AtomicModel::ctx, "[%s] %" PRIuMAX "-%" PRIuMAX
                    " (neighbour_number : %" PRIuMAX " expected)\n",
                    m_name.c_str(),
                    static_cast <std::uintmax_t>(m_received),
                    static_cast <std::uintmax_t>(m_total_received),
                    static_cast <std::uintmax_t>(m_neighbour_number * 10));

            m_phase = WAIT;
            m_total_received += m_received;
            m_received = 0;
            m_last_time = time;
        }
    }

    void dext(const double& time)
    {
        vle_dbg(AtomicModel::ctx, "[%s] dext at %f (x[0].size: %" PRIuMAX " received: %" PRIuMAX " neighbour_number: %" PRIuMAX ")\n",
                m_name.c_str(), time,
                static_cast <std::uintmax_t>(x[0].size()),
                static_cast <std::uintmax_t>(m_received),
                static_cast <std::uintmax_t>(m_neighbour_number));
        if (m_last_time == time)
            vle_dbg(AtomicModel::ctx, "/!\\ [%s] oups at %f", m_name.c_str(), time);

        for (size_t i = 0, e = x[0].size(); i != e; ++i)
            vle_dbg(AtomicModel::ctx, "value: %d\n", x[0][i]);

        m_received += x[0].size();

        if (m_received == m_neighbour_number)
            m_phase = SEND;
    }

    virtual void lambda() const override final
    {
        if (m_phase == SEND) {
            vle_dbg(AtomicModel::ctx, "[%s] lambda\n", m_name.c_str());

            y[0] = {m_id};
        }
    }
};

template <typename T>
struct Coupled : T
{
    std::string m_name;

    Coupled(const vle::Context& ctx)
        : T(ctx)
    {}

    virtual ~Coupled()
    {}

    virtual void apply_common(const vle::Common& common) override
    {
        m_name = vle::common_get <std::string>(common, "name");
    }

    virtual vle::Common update_common(const vle::Common& common,
                                      const typename Coupled::vertices& v,
                                      const typename Coupled::edges& e,
                                      int child) override
    {
       auto mdl = v[child].get();

        unsigned int nb = std::accumulate(
            e.cbegin(), e.cend(),
            0u, [&mdl](unsigned int x, const typename Coupled::edges::value_type& edge)
            {
                return edge.second.first == mdl ? x + 1u : x;
            });

        vle::Common ret(common);

        vle_dbg(T::ctx, "[%s] init %s (%p) with %" PRIuMAX " neightbour\n",
                m_name.c_str(),
                vle::stringf("%s-%d", m_name.c_str(), child).c_str(),
                mdl,
                static_cast <std::uintmax_t>(nb));

        ret["id"] = child;
        ret["name"] = vle::stringf("%s-%d", m_name.c_str(), child);
        ret["neighbour_number"] = nb;

        return std::move(ret);
    }
};

template <typename T>
struct RootMPI : T
{
    boost::mpi::communicator com;

    RootMPI(const vle::Context& ctx)
        : T(ctx)
        , com()
    {}

    virtual ~RootMPI()
    {}

    virtual vle::Common update_common(const vle::Common& common,
                                      const typename RootMPI::vertices& v,
                                      const typename RootMPI::edges& e,
                                      int child) override
    {
        (void)v;
        (void)e;

        if (com.size() <= child + 1)
            throw std::invalid_argument("MPI size < children size");

        bench::SynchronousProxyModel* mdl =
            dynamic_cast
            <bench::SynchronousProxyModel*>(T::m_children[child].get());

        if (!mdl)
            throw std::invalid_argument("RootMPI without SynchronousProxyModel");

        mdl->rank = child + 1;

        vle_info(T::ctx, "RootMPI assign %d to child %d\n", mdl->rank, child);

        vle::Common ret(common);

        return std::move(ret);
    }
};

template <typename T>
struct Root : T
{
    Root(const vle::Context& ctx)
        : T(ctx)
    {}

    virtual ~Root()
    {}

    virtual vle::Common update_common(const vle::Common& common,
                                      const typename Root::vertices& v,
                                      const typename Root::edges& e,
                                      int child) override
    {
        vle::Common ret(common);

        auto mdl = v[child].get();
        unsigned int nb = std::accumulate(
            e.cbegin(), e.cend(),
            0u, [&mdl](unsigned int x, const typename Root::edges::value_type& edge)
            {
                return edge.second.first == mdl ? x + 1u : x;
            });

        ret["id"] = child;
        ret["name"] = vle::stringf("S%d", child);
        ret["neighbour_number"] = nb;
        ret["tgf-filesource"] = vle::stringf("S%d.tgf", child);
        ret["tgf-format"] = (int)1;

        return std::move(ret);
    }
};

using RootThread = Root <GenericCoupledModelThread>;
using RootMono = Root <GenericCoupledModelMono>;
using RootMPIThread = RootMPI <GenericCoupledModelThread>;
using RootMPIMono = RootMPI <GenericCoupledModelMono>;
using CoupledThread = Coupled <GenericCoupledModelThread>;
using CoupledMono = Coupled <GenericCoupledModelMono>;

}

#endif
