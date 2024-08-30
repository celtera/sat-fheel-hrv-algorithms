#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/circular_buffer.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

#include <algorithm>
#include <chrono>
#include <map>
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

  struct
  {
    struct
    {
      halp_meta(name, "Input")
      std::pair<std::string, int> value;
      void update(HeartbeatMetrics& self) { self.addRow(); }
    } heartbeats;

    // Heart-rate baseline
    halp::hslider_f32<"Baseline", halp::range{20., 200., 74.}> baseline;

    // Heart-rate peak ceil
    halp::hslider_f32<"Ceil", halp::range{1., 4., 1.25}> ceil;

    // Time window upon which the analysis is performed
    halp::hslider_i32<"Window", halp::irange{1, 10000, 1000}> window;

    // Std deviation range
    halp::hslider_f32<"Stddev Range", halp::range{0.1, 5, 3}> stddev;

  } inputs;

  struct
  {
    halp::val_port<"Peak", std::vector<std::pair<std::string, float>>> peak;
    halp::val_port<"Average", std::vector<std::pair<std::string, float>>> average;
    halp::val_port<"RMSSD", std::vector<std::pair<std::string, float>>> rmssd;
    halp::val_port<"Correlation", float> correlation;
    halp::val_port<"Stddev ratio", float> stddev_ratio;
    halp::val_port<"Var coeff", float> var_coeff;
  } outputs;

  void operator()() { }

  using clk = std::chrono::steady_clock;
  using timestamp = clk::time_point;
  using bpm = std::pair<timestamp, int>;
  struct heartbeats
  {
    boost::circular_buffer<bpm> data;
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

  static constexpr auto default_capacity = 1000;
  static constexpr auto default_duration = std::chrono::seconds(10);

  int current_frame_index = 1;
  int current_sensor_index = 1;

  void addRow()
  {
    m_last_point_timestamp = clk::now();
    const auto& [name, bpm] = inputs.heartbeats.value;
    auto& vec = beats[name].data;
    if(vec.empty())
      vec.resize(default_capacity);
    vec.push_back({m_last_point_timestamp, bpm});
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
    auto window = std::chrono::milliseconds(inputs.window.value);
    for(auto& [name, hb] : beats)
    {
      computeIndividualMetrics(hb, window);
    }
    computeGroupMetrics();
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
#pragma omp simd
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
    outputs.stddev_ratio = float(pop_within_stddev) / float(pop);

    // Method 3. Coefficient of variation
  }

  std::map<std::string, heartbeats> beats;
};
}
