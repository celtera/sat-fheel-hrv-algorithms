#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/circular_buffer.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace fheel
{
class HeartbeatMetrics
{
public:
  halp_meta(name, "Heartbeat Metrics")
  halp_meta(c_name, "heartbeat_metrics")
  halp_meta(category, "Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Heartbeat metrics")
  halp_meta(uuid, "20bdd7bf-716d-497e-86b3-34c6b2bd50e1")

  struct messages
  {
    struct
    {
      halp_meta(name, "input")
      void operator()(HeartbeatMetrics& self, std::string name, int bpm)
      {
        self.addRow(name, bpm);
      }
    } heartbeats;
  };
  struct
  {
    struct : halp::hslider_f32<"Baseline", halp::range{20., 200., 74.}> {
      halp_meta(c_name, "baseline")
      halp_meta(description, "Heart-rate baseline in bpm")
      halp_flag(class_attribute);
    } baseline;

    struct : halp::hslider_f32<"Ceil", halp::range{1., 4., 1.25}> {
      halp_meta(c_name, "ceil")
      halp_meta(description, "Heart-rate peak ceil")
      halp_flag(class_attribute);
    } ceil;

    struct : halp::hslider_i32<"Window", halp::irange{1, 10000, 1000}> {
      halp_meta(c_name, "window")
      halp_meta(description, "Time window in ms upon which the analysis is performed")
      halp_flag(class_attribute);
    } window;

    struct : halp::hslider_f32<"Stddev Range", halp::range{0.1, 5, 3}> {
      halp_meta(c_name, "stddev_range")
      halp_meta(description, "Std deviation range")
      halp_flag(class_attribute);
    } stddev;

  } inputs;

  struct excitation
  {
    std::string name;
    float peak{};
    float average{};
    float variance{};
    float stddev{};
    float rmssd{};
  };

  struct synchronization
  {
    float correlation;
    float deviation;
    float coeff_variation;
  };

  struct
  {
    struct : halp::val_port<"Excitation", std::vector<excitation>> { 
      halp_meta(description, "Array of excitation values for individual participants")
    } excitation;
    struct : halp::val_port<"Synchronization", synchronization> { 
      halp_meta(description, "Global synchronization metrics")
    } synchronization;
  } outputs;

  void operator()() { }

  static constexpr auto default_capacity = 1000;
  static constexpr auto default_duration = std::chrono::seconds(10);

  using clk = std::chrono::steady_clock;
  using timestamp = clk::time_point;
  using bpm = std::pair<timestamp, int>;
  struct heartbeats
  {
    boost::circular_buffer<bpm> data = boost::circular_buffer<bpm>(default_capacity);
    struct statistics
    {
      std::vector<float> bpms;
      int count{};
      float peak{};
      float average{};
      float variance{};
      float stddev{};
      float rmssd{};
    } stats;
  };

  timestamp m_last_point_timestamp;

  int current_frame_index = 1;
  int current_sensor_index = 1;

  void addRow(std::string_view name, int bpm)
  {
    m_last_point_timestamp = clk::now();
    auto it = beats.find(name);
    if(it == beats.end())
    {
      it = beats.emplace(std::string{name}, heartbeats{}).first;
    }
    auto& vec = it->second.data;
    if(vec.empty())
      vec.resize(default_capacity);
    vec.push_back({m_last_point_timestamp, bpm});
    computeMetrics();
    computeGroupMetrics();
  }

  void cleanupOldTimestamps()
  {
    for(auto& [name, hb] : beats)
    {
      auto& buffer = hb.data;
      for(auto it = buffer.begin(); it != buffer.end();)
      {
        auto& [ts, val] = *it;
        if(m_last_point_timestamp - ts > default_duration)
        {
          it = buffer.erase(it);
        }
        else
        {
          break;
        }
      }
    }
  }

  void computeMetrics()
  {
    outputs.excitation.value.clear();
    auto window = std::chrono::milliseconds(inputs.window.value);
    for(auto& [name, hb] : beats)
    {
      computeIndividualMetrics(hb, window);
      if(hb.stats.count > 1)
      {
        outputs.excitation.value.push_back({
            .name = name,
            .peak = hb.stats.peak,
            .average = hb.stats.average,
            .variance = hb.stats.variance,
            .stddev = hb.stats.stddev,
            .rmssd = hb.stats.rmssd,
        });
      }
    }
  }

  void computeIndividualMetrics(heartbeats& hb, std::chrono::milliseconds window)
  {
    auto& stats = hb.stats;
    auto& bpm_cache = stats.bpms;
    stats.bpms.clear();
    stats.count = 0;

    // Compute basic statistics
    for(auto& [t, bpm] : hb.data)
    {
      if((this->m_last_point_timestamp - t) < window)
      {
        if(bpm > 0)
        {
          const float beats = bpm - inputs.baseline.value;
          bpm_cache.push_back(beats);
          stats.average += beats;
          stats.peak = std::max(stats.peak, beats);
        }
      }
    }

    stats.count = bpm_cache.size();
    if(stats.count <= 1)
      return;

    stats.average /= stats.count;

    for(float bpm : bpm_cache)
    {
      stats.variance += std::pow(bpm - stats.average, 2.f);
    }

    stats.variance /= stats.count;
    stats.stddev = std::sqrt(stats.variance);

    // Compute rmssd
    // 1. BPM to RR
    for(float& v : bpm_cache)
      v = 60 * 1000 / v;

    // 2. From value to interval difference
    stats.rmssd = 0.;

#pragma omp simd
    // 3. Mean
    for(int i = 0; i < stats.count - 1; i++)
      stats.rmssd += (std::pow(bpm_cache[i + 1] - bpm_cache[i], 2));

    // 4. RMSSD
    stats.rmssd = std::sqrt(stats.rmssd / (stats.count - 1));
  }

  void computeGroupMetrics()
  {
    // Method 1. Cross-correlation
    // TODO

    // Method 2. Standard deviation distance
    // 1. Global mean
    int n = 0;
    float avg = 0.f;
    float stddev = 0.f;
    for(auto& [name, b] : beats)
    {
      auto& stats = b.stats;
      if(stats.count <= 1)
        continue;
      avg += stats.average;
      stddev += stats.stddev;
      n++;
    }
    if(n == 0)
      return;

    avg /= n;
    stddev /= n;

    int pop = 0;
    int pop_within_stddev = 0;
    for(auto& [name, hb] : beats)
    {
      if(hb.stats.count > 1)
      {
        pop++;
        if(std::abs(hb.stats.average) <= std::abs(inputs.stddev.value * stddev) + avg)
        {
          pop_within_stddev++;
        }
      }
    }
    outputs.synchronization.value.deviation = float(pop_within_stddev) / float(pop);

    // Method 3. Coefficient of variation
    outputs.synchronization.value.coeff_variation = stddev / avg;
  }

  struct string_hash
  {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const { return hash_type{}(str); }
    std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
    std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
  };
  struct string_equal
  {
    using is_transparent = std::true_type;

    bool operator()(std::string_view l, std::string_view r) const noexcept
    {
      return l == r;
    }
  };

  std::unordered_map<std::string, heartbeats, string_hash, string_equal> beats;
};
}
