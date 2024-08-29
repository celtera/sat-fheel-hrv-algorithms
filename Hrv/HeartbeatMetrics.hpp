#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

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

  // We need multiple inputs as
  struct
  {
    struct
    {
      halp_meta(name, "Input")
      std::vector<std::pair<std::string, int>> value;
      void update(HeartbeatMetrics& self) { self.addRow(); }
    } heartbeats;

    // Heart-rate baseline
    halp::hslider_f32<"Baseline", halp::range{20., 120., 74.}> baseline;

    // Time window upopn which the analysis is performed
    halp::hslider_f32<"Window", halp::range{0., 10000., 1000.}> window;

  } inputs;

  struct
  {
    halp::val_port<"Peak", std::vector<float>> peak;
    halp::val_port<"Average", std::vector<float>> average;
    halp::val_port<"HRV", std::vector<float>> rmssd;
  } outputs;

  void operator()() { }

  std::chrono::steady_clock::time_point m_last_point_timestamp;

  hmdf::StdDataFrame<unsigned long> ul_df1;

  std::map<std::string, int> id_to_index;

  int current_frame_index = 1;
  int current_sensor_index = 1;

  void addRow()
  {
    m_last_point_timestamp = std::chrono::steady_clock::now();
    for(const auto& [name, bpm] : inputs.heartbeats.value)
    {
      if(ul_df1.append_row(0))
      {
        //nan_policy::dont_pad_with_nans);
      }
    }
  }
  HeartbeatMetrics()
  {
    std::vector<unsigned long> idx_col1;
    ul_df1.load_index(std::move(idx_col1));
  }
};
}
