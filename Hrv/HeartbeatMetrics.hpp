#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/rolling_variance.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

#include <chrono>
#include <vector>
namespace ba = boost::accumulators;
namespace fheel
{
// Structures de données représentant les statistiques
// qui nous intéressent, par capteur puis globales
struct excitation
{
  std::string name;
  bool peaking{};
  float peak{};
  float average{};
  float variance{};
  float stddev{};
  float rmssd{};
};

struct synchronization
{
  float correlation{};
  float deviation{};
  float coeff_variation{};
};

// Code de l'objet
class HeartbeatMetrics
{
public:
  // Métadonnées de l'objet
  halp_meta(name, "Heartbeat Metrics")
  halp_meta(c_name, "heartbeat_metrics")
  halp_meta(category, "Mappings")
  halp_meta(
      author,
      "Jean-Michaël Celerier, Rochana Fardon (Société des Arts Technologiques)\n"
      "Marion Cossin (CRITAC)\nLéa Dedola")
  halp_meta(description, "Heartbeat metrics")
  halp_meta(uuid, "20bdd7bf-716d-497e-86b3-34c6b2bd50e1")

  // Combien d'échantillons maximum on veut garder
  static constexpr auto default_capacity = 1000;

  // Quelle durée maximum on veut garder
  static constexpr auto default_duration = std::chrono::seconds(10);

  // Types de données utiles
  using clk = std::chrono::steady_clock;
  using timestamp = clk::time_point;
  using bpm = std::pair<timestamp, int>;

  using stat_accum
      = ba::accumulator_set<float, ba::stats<ba::tag::mean, ba::tag::variance>>;

  // Structure de donnée interne pour stocker l'information reçue d'un capteur donné
  struct heartbeats
  {
    boost::circular_buffer<bpm> data = boost::circular_buffer<bpm>(default_capacity);

    // Statistics for the current window of time (e.g. the last 5 seconds)
    struct running_statistics
    {
      std::vector<float> bpms;
      int count{};
      bool peaking{};
      float peak{};
      float average{};
      float variance{};
      float stddev{};
      float rmssd{};
    } stats;

    // Global statistics from the recording feature
    stat_accum accumulators;

    float average{};
    float stddev{};
  };

  // Messages qu'on veut pouvoir traiter depuis Max
  struct messages
  {
    struct
    {
      halp_meta(name, "input")
      halp_flag(process_any_message);
      void operator()(HeartbeatMetrics& self, std::string name, int bpm)
      {
        self.addRow(name, bpm);
      }
    } heartbeats;
  };

  // Attributs et autres entrées
  struct
  {
    struct : halp::hslider_f32<"Baseline", halp::range{20., 200., 74.}>
    {
      halp_meta(c_name, "baseline")
      halp_meta(description, "Heart-rate baseline in bpm")
      halp_flag(class_attribute);
    } baseline;

    struct : halp::hslider_f32<"Ceil", halp::range{1., 4., 1.25}>
    {
      halp_meta(c_name, "ceil")
      halp_meta(description, "Heart-rate peak ceil")
      halp_flag(class_attribute);
    } ceil;

    struct : halp::hslider_i32<"Window", halp::irange{1, 10000, 1000}>
    {
      halp_meta(c_name, "window")
      halp_meta(description, "Time window in ms upon which the analysis is performed")
      halp_flag(class_attribute);
    } window;

    struct : halp::hslider_f32<"Stddev Range", halp::range{0.1, 5, 3}>
    {
      halp_meta(c_name, "stddev_range")
      halp_meta(description, "Std deviation range")
      halp_flag(class_attribute);
    } stddev;

    struct : halp::val_port<"Recording", bool>
    {
      halp_meta(c_name, "recording")
      halp_meta(description, "Data will be recorded when this is enabled.")
      halp_flag(class_attribute);

      void update(HeartbeatMetrics& self)
      {
        if(value)
          self.startRecording();
        else
          self.stopRecording();
      }
    } recording;
  } inputs;

  // Sorties
  struct
  {
    struct : halp::callback<"Excitation", excitation>
    {
      halp_meta(description, "Excitation values for the last participant")
    } excitation;
    struct : halp::callback<"Synchronization", synchronization>
    {
      halp_meta(description, "Global synchronization metrics")
    } synchronization;
  } outputs;

  void startRecording();
  void stopRecording();

  // Appelé lorsqu'on reçoit une nouvelle donnée d'un capteur
  void addRow(const std::string& name, int bpm);

  void cleanupOldTimestamps();

  // Calcul des métriques d'excitation individuelles
  void computeIndividualMetrics(heartbeats& hb, std::chrono::milliseconds window);
  void outputIndividualMetrics();

  // Calcul des métriques d'excitation du groupe
  void computeGroupMetrics();

private:
  timestamp m_last_point_timestamp{};
  boost::unordered_flat_map<std::string, heartbeats> beats;

  // Global accumulators for mean / variance
  bool is_recording{};
  stat_accum accumulators;
  double global_stddev = 1.;
};
}
