/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/writer/local.hpp>

#include<map>

extern "C"
{
#include <nvml.h>
}

namespace lo2s
{
namespace metric
{
namespace nvml
{
class ProcessRecorder : public monitor::PollMonitor
{
public:
    ProcessRecorder(trace::Trace& trace, Gpu gpu);
    ~ProcessRecorder();

protected:
    void monitor(int fd) override;

    std::string group() const override
    {
        return "nvml::ProcessMonitor";
    }

private:
    std::vector<otf2::writer::local*> otf2_writers_;

    std::map<Process, otf2::definition::metric_instance> metric_instances_;
    std::vector<std::unique_ptr<otf2::event::metric>> events_;

    Gpu gpu_;

    nvmlReturn_t result;
    nvmlDevice_t device;
    unsigned long long lastSeenTimeStamp = 0;
    unsigned int samples_count;
    unsigned int max_length = 64;
    Process proc;
    char proc_name[64];
    
};
} // namespace nvml
} // namespace metric
} // namespace lo2s