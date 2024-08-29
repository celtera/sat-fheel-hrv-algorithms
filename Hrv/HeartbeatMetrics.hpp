#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/circular_buffer.hpp>

#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameFinancialVisitors.h>
#include <DataFrame/DataFrameMLVisitors.h>
#include <DataFrame/DataFrameStatsVisitors.h>
#include <DataFrame/Utils/DateTime.h>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
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
    halp::hslider_f32<"Baseline", halp::range{20., 120., 74.}> baseline;

    // Time window upopn which the analysis is performed
    halp::hslider_i32<"Window", halp::irange{1, 10000, 1000}> window;

  } inputs;

  struct
  {
    halp::val_port<"Peak", std::vector<float>> peak;
    halp::val_port<"Average", std::vector<float>> average;
    halp::val_port<"HRV", std::vector<float>> rmssd;
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
      float peak{};
      float average{};
      float variance{};
      float stddev{};
      float rms{};
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
      hb.stats = computeIndividualMetrics(hb.data, window);
    }
    computeGroupMetrics();
  }

  heartbeats::statistics computeIndividualMetrics(
      boost::circular_buffer<bpm>& hb, std::chrono::milliseconds window)
  {
    heartbeats::statistics stats;

    int n = 0;
    for(auto& [t, bpm] : hb)
    {
      if((this->m_last_point_timestamp - t) < window)
      {
        float beats = bpm;
        stats.average += beats;
        stats.peak = std::max(stats.peak, beats);
        n++;
      }
    }
    if(n == 0)
      return {};

    stats.average /= n;

    for(auto& [t, bpm] : hb)
    {
      if((this->m_last_point_timestamp - t) < window)
      {
        stats.variance += std::pow(bpm - stats.average, 2.f);
      }
    }

    stats.variance /= n;
    stats.stddev = std::sqrt(stats.variance);

    return stats;
  }

  void computeGroupMetrics() { }

  std::map<std::string, heartbeats> beats;
};
}
