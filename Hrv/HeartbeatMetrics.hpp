#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/circular_buffer.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

#include <chrono>
#include <vector>

namespace fheel
{
// Structures de données représentant les statistiques
// qui nous intéressent, par capteur puis globales
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

// Code de l'objet
class HeartbeatMetrics
{
public:
  halp_meta(name, "Heartbeat Metrics")
  halp_meta(c_name, "heartbeat_metrics")
  halp_meta(category, "Mappings")
  halp_meta(
      author,
      "Jean-Michaël Celerier, Rochana Fardon (Société des Arts Technologiques)\n"
      "Marion Cossin (CRITAC)\nLéa Dedola")
  halp_meta(description, "Heartbeat metrics")
  halp_meta(uuid, "20bdd7bf-716d-497e-86b3-34c6b2bd50e1")

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

    struct
    {
      halp_meta(name, "start")
      void operator()(HeartbeatMetrics& self) { self.startRecording(); }
    } start;

    struct
    {
      halp_meta(name, "stop")
      void operator()(HeartbeatMetrics& self) { self.stopRecording(); }
    } stop;
  };

  // Attributs et autres entrées
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

  // Combien d'échantillons maximum on veut garder
  static constexpr auto default_capacity = 1000;

  // Quelle durée maximum on veut garder
  static constexpr auto default_duration = std::chrono::seconds(10);

  // Types de données utiles
  using clk = std::chrono::steady_clock;
  using timestamp = clk::time_point;
  using bpm = std::pair<timestamp, int>;

  // Structure de donnée interne pour stocker l'information reçue d'un capteur donné
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

  timestamp m_last_point_timestamp{};

  void startRecording()
  {
    // TODO
  }
  void stopRecording()
  {
    // TODO
  }

  // Appelé lorsqu'on reçoit une nouvelle donnée d'un capteur
  void addRow(std::string_view name, int bpm)
  {
    m_last_point_timestamp = clk::now();

    // Recherche du capteur et création d'un nouveau dans notre base de données si on ne l'a pas encore rencontré
    auto it = beats.find(name);
    if(it == beats.end())
    {
      it = beats.emplace(std::string{name}, heartbeats{}).first;
      it->second.data.resize(default_capacity);
    }

    auto& hb = it->second;
    auto& vec = hb.data;
    vec.push_back({m_last_point_timestamp, bpm});

    computeIndividualMetrics(hb, std::chrono::milliseconds(inputs.window.value));

    this->outputs.excitation(excitation{
        .name = it->first,
        .peak = hb.stats.peak,
        .average = hb.stats.average,
        .variance = hb.stats.variance,
        .stddev = hb.stats.stddev,
        .rmssd = hb.stats.rmssd,
    });
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

  // Calcul des métriques d'excitation individuelles
  void computeIndividualMetrics(heartbeats& hb, std::chrono::milliseconds window)
  {
    auto& stats = hb.stats;
    auto& bpm_cache = stats.bpms;
    stats.bpms.clear();
    stats.count = 0;
    stats.peak = 0.;
    stats.average = 0.;
    stats.rmssd = 0.;
    stats.variance = 0.;

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

          if(std::abs(beats) > std::abs(stats.peak))
            stats.peak = beats;
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
    stats.rmssd = 0.;

#pragma omp simd
    for(int i = 0; i < stats.count - 1; i++)
    {
      // 1. BPM to RR
      float v0 = 60. * 1000. / bpm_cache[i];
      float v1 = 60. * 1000. / bpm_cache[i + 1];

      // 2. Mean
      stats.rmssd += std::pow(v1 - v0, 2);
    }

    // 3. RMSSD
    stats.rmssd = std::sqrt(stats.rmssd / (stats.count - 1));
  }

  void outputIndividualMetrics()
  {
#if 0 // If we want to output all the participants at once
    static thread_local std::vector<excitation> exc;
    exc.clear();
    exc.reserve(this->beats.size());

    for(auto& [name, hb] : beats)
    {
      if(hb.stats.count > 1)
      {
        exc.push_back({
            .name = name,
            .peak = hb.stats.peak,
            .average = hb.stats.average,
            .variance = hb.stats.variance,
            .stddev = hb.stats.stddev,
            .rmssd = hb.stats.rmssd,
        });
      }
    }

    if(!exc.empty())
      this->outputs.excitation(std::move(exc));
#endif
  }

  void computeGroupMetrics()
  {
    synchronization sync{};
    // Method 1. Cross-correlation
    // TODO

    // Method 2. Standard deviation distance
    // 1. Global mean
    int n = 0;
    int total_samples = 0;
    float avg = 0.f;
    float var = 0.f;
    float stddev = 0.f;

    // Global average
    for(auto& [name, b] : beats)
    {
      auto& stats = b.stats;
      if(stats.count <= 1)
        continue;
      total_samples += stats.count;
      avg += stats.average * stats.count;
      n++;
    }

    if(n == 0 || total_samples == 0)
      return;

    avg /= total_samples;

    // Global variance
    for(auto& [name, b] : beats)
    {
      auto& stats = b.stats;
      if(stats.count <= 1)
        continue;
      for(float bpm : b.stats.bpms)
      {
        var += std::pow(bpm - avg, 2.f);
      }
    }

    stddev = std::sqrt(var / n);

    int pop = 0;
    int pop_within_stddev = 0;
    for(auto& [name, hb] : beats)
    {
      if(hb.stats.count > 1)
      {
        pop++;
        if(std::abs(hb.stats.average) <= (std::abs(inputs.stddev.value * stddev) + avg))
        {
          pop_within_stddev++;
        }
      }
    }

    sync.deviation = float(pop_within_stddev) / float(pop);

    // Method 3. Coefficient of variation
    sync.coeff_variation = stddev / avg;

    outputs.synchronization(sync);
  }

  // Optimisation de la table de hachage pour le stockage
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

  boost::unordered_flat_map<std::string, heartbeats, string_hash, string_equal> beats;
};
}
