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

#include <lo2s/metric/nvml/process_recorder.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/enumerate.hpp>

#include <cstring>

namespace lo2s
{
namespace metric
{
namespace nvml
{


ProcessRecorder::ProcessRecorder(trace::Trace& trace, Gpu gpu)
: PollMonitor(trace, "gpu " + std::to_string(gpu.as_int()) + " (" + gpu.name() + ")", config().read_interval)
{

    gpu_ = gpu;

    result = nvmlDeviceGetHandleByIndex(gpu.as_int(), &device);

    if (NVML_SUCCESS != result){ 

        Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
        throw_errno();
    }


    nvmlDeviceGetProcessUtilization(device, NULL, &samples_count, lastSeenTimeStamp);
        
    auto samples = std::make_unique<nvmlProcessUtilizationSample_t[]>(samples_count);
        
    result = nvmlDeviceGetProcessUtilization(device, samples.get(), &samples_count, lastSeenTimeStamp);

    for(unsigned int i = 0; i < samples_count; i++)
    {
        result = (NVML_SUCCESS == result ? nvmlSystemGetProcessName(samples[i].pid, proc_name, max_length) : result);

        if (NVML_SUCCESS != result){ 
        
            Log::error() << "Failed to get some process metric or name: " << nvmlErrorString(result);
            throw_errno();

        }

        proc = Process(samples[i].pid);

        Log::debug() << "Found new Process: " << proc.as_pid_t();

        otf2_writers_.emplace_back(&trace_.create_metric_writer(name() + proc_name));
        metric_instances_.emplace(proc, trace_.metric_instance(trace_.metric_class(), otf2_writers_.back()->location(), trace_.system_tree_gpu_node(gpu)));

        auto mc = otf2::definition::make_weak_ref(metric_instances_.at(proc).metric_class());

        mc->add_member(trace_.metric_member(std::string("Decoder Utilization, ") + proc_name, "GPU Decoder Utilization by this Process",
                                            otf2::common::metric_mode::absolute_point,
                                            otf2::common::type::Double, "%"));
        mc->add_member(trace_.metric_member(std::string("Encoder Utilization, ") + proc_name, "GPU Encoder Utilization by this Process",
                                            otf2::common::metric_mode::absolute_point,
                                            otf2::common::type::Double, "%"));
        mc->add_member(trace_.metric_member(std::string("Memory Utilization, ") + proc_name, "GPU Memory Utilization by this Process",
                                            otf2::common::metric_mode::absolute_point,
                                            otf2::common::type::Double, "%"));
        mc->add_member(trace_.metric_member(std::string("SM Utilization, ") + proc_name, "GPU SM Utilization by this Process",
                                            otf2::common::metric_mode::absolute_point,
                                            otf2::common::type::Double, "%"));
        
        events_.emplace_back(std::make_unique<otf2::event::metric>(otf2::chrono::genesis(), metric_instances_.at(proc)));

    }
    
    Log::debug() << "updating timestamp from " << lastSeenTimeStamp << " to " << samples[0].timeStamp;
    lastSeenTimeStamp = samples[0].timeStamp;

}

void ProcessRecorder::monitor([[maybe_unused]] int fd)
{
    Log::debug() << "In Monitor Function";
    for (auto& event : events_)
    {
        event->timestamp(time::now());
    }

    Log::debug() << "updated timestamps of events";

    nvmlDeviceGetProcessUtilization(device, NULL, &samples_count, lastSeenTimeStamp);
        
    auto samples = std::make_unique<nvmlProcessUtilizationSample_t[]>(samples_count);
        
    result = nvmlDeviceGetProcessUtilization(device, samples.get(), &samples_count, lastSeenTimeStamp);

    Log::debug() << "got " << samples_count << " samples";

    // catch nvml not getting correct samples
    if (NVML_SUCCESS != result || samples[0].pid == 0) {return;}


    for(unsigned int i = 0; i < samples_count; i++)
    {
        
        proc = Process(samples[i].pid);
        Log::debug() << "Looking at sample " << i;

        if (metric_instances_.count(proc))
        {
            Log::debug() << "process " << proc.as_pid_t() << " already exists";
            for (long unsigned int j = 0; j < events_.size(); j++)
            {
                Log::debug() << "trying to find matching event"; 
                if (events_.at(j)->resolve_metric_class() == metric_instances_.at(proc).metric_class())
                {
                    Log::debug() << "found matching event at j = " << j; 
                    events_.at(j)->raw_values()[0] = double(samples[i].decUtil);
                    events_.at(j)->raw_values()[1] = double(samples[i].encUtil);
                    events_.at(j)->raw_values()[2] = double(samples[i].memUtil);
                    events_.at(j)->raw_values()[3] = double(samples[i].smUtil);    

                    otf2_writers_.at(j)->write(*events_.at(j));

                    Log::debug() << "writing events of process " << proc.as_pid_t();
                }

            }

        }

        else
        {
            Log::debug() << "Found new Process: " << proc.as_pid_t();

            result = nvmlSystemGetProcessName(samples[i].pid, proc_name, max_length);

            if (NVML_SUCCESS != result){ 
    
                Log::error() << "Failed to get some process name: " << nvmlErrorString(result);
                continue;

            }

            proc = Process(samples[i].pid);

            otf2_writers_.emplace_back(&trace_.create_metric_writer(name() + proc_name));
            metric_instances_.emplace(proc, trace_.metric_instance(trace_.metric_class(), otf2_writers_.back()->location(), trace_.system_tree_gpu_node(gpu_)));

            auto mc = otf2::definition::make_weak_ref(metric_instances_.at(proc).metric_class());

            mc->add_member(trace_.metric_member(std::string("Decoder Utilization, ") + proc_name, "GPU Decoder Utilization by this Process",
                                                otf2::common::metric_mode::absolute_point,
                                                otf2::common::type::Double, "%"));
            mc->add_member(trace_.metric_member(std::string("Encoder Utilization, ") + proc_name, "GPU Encoder Utilization by this Process",
                                                otf2::common::metric_mode::absolute_point,
                                                otf2::common::type::Double, "%"));
            mc->add_member(trace_.metric_member(std::string("Memory Utilization, ") + proc_name, "GPU Memory Utilization by this Process",
                                                otf2::common::metric_mode::absolute_point,
                                                otf2::common::type::Double, "%"));
            mc->add_member(trace_.metric_member(std::string("SM Utilization, ") + proc_name, "GPU SM Utilization by this Process",
                                                otf2::common::metric_mode::absolute_point,
                                                otf2::common::type::Double, "%"));
            
            events_.emplace_back(std::make_unique<otf2::event::metric>(otf2::chrono::genesis(), metric_instances_.at(proc)));

            events_.back()->timestamp(time::now());
            events_.back()->raw_values()[1] = double(samples[i].encUtil);
            events_.back()->raw_values()[0] = double(samples[i].decUtil);
            events_.back()->raw_values()[2] = double(samples[i].memUtil);
            events_.back()->raw_values()[3] = double(samples[i].smUtil);


            otf2_writers_.back()->write(*events_.back());

            Log::debug() << "wrote events of new process " << proc.as_pid_t();
        }

        Log::debug() << "updating timestamp from " << lastSeenTimeStamp << " to " << samples[0].timeStamp;
        lastSeenTimeStamp = samples[0].timeStamp;

    }

    if (NVML_SUCCESS != result){ 
        
        Log::error() << "Failed to get some process metric: " << nvmlErrorString(result);
        throw_errno();

    }

}

ProcessRecorder::~ProcessRecorder()
{

}
} // namespace nvml
} // namespace metric
} // namespace lo2s