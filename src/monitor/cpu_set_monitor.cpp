#include <lo2s/monitor/cpu_set_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/topology.hpp>

#include <csignal>

namespace lo2s
{
namespace monitor
{
CpuSetMonitor::CpuSetMonitor(const MonitorConfig& config) : MainMonitor(config)
{
    for (const auto& cpu : Topology::instance().cpus())
    {
        Log::debug() << "Create cstate recorder for cpu #" << cpu.id;
        auto ret = monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu.id),
                                     std::forward_as_tuple(cpu.id, config, trace_));
        assert(ret.second);
    }
}

void CpuSetMonitor::run()
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);

    auto ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
    if (ret)
    {
        Log::error() << "Failed to set pthread_sigmask: " << ret;
        throw std::runtime_error("Failed to set pthread_sigmask");
    }

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.start();
    }

    int sig;
    ret = sigwait(&ss, &sig);
    if (ret)
    {
        throw make_system_error();
    }

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.start();
    }
}
}
}
