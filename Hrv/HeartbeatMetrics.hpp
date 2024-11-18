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
// qui nous intéressent

// Message de Dimitri documentant ses besoins :

// Sortie 1 : Données brutes des capteurs (capteur et BPM)
// Cette sortie renvoie directement les BPM reçus de chaque capteur.

// Sortie 2 : Écart en pourcentage par rapport à la baseline individuelle
// Calcule et renvoie l'écart en pourcentage entre le BPM actuel d'un capteur et sa baseline.

// Sortie 3 : Écart en pourcentage par rapport à la moyenne des autres capteurs
// Calcule la différence en pourcentage entre la déviation d'un capteur par rapport à sa baseline et la moyenne des déviations des autres capteurs.

// Sortie 4 : Synchronie globale (en pourcentage)
// Calcule et renvoie la synchronie globale des capteurs en pourcentage (100% = synchronie parfaite, 0% = désynchronisation totale).

// Sortie 5
// Ecart de tout le groupe par rapport à la baseline (en pourcentage) (pour mesurer si les gens sont plus ou moins excités par rapport au debut)

// Sortie 6
// Valeur de la baseline
struct excitation
{
  // sortie 0, permet d'identifier à quel capteur les métriques d'excitation appartiennent
  std::string name;
  // sortie 1
  int bpm{};
  // sortie 2
  float percent_of_baseline{};
  // sortie 3
  float distance_from_average{};
  // sortie 6
  float baseline;
  // sortie bonus 1
  bool peaking{};
  // sortie bonus 2
  float peak{};
};

struct synchronization
{
  // sortie 4 (pourcentage de la population qui sont dans 3 écarts types de la moyenne)
  float deviation{};
  // sortie 5 (pourcentage moyen de la baseline que chaque capteur capte en ce moment.
  // Si on a 2 participant, un est à 110 % de sa baseline et l'autre à 100%, on aura une moyenne de 105%
  float average_percent_of_baseline{};
  // sortie bonus 3
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
      bool peaking{};
      float peak{};
      // This is the percentage of the baseline for the last bpm.
      // If the current bpm is 66 and the baseline is 60, this would be 1.1 (110%)
      float current_percent_of_baseline{};
      float bpm;
    } stats;

    // Global statistics from the recording feature
    stat_accum accumulators;
    // 76 is a good guess for average resting heart rate of a human and we need this value
    // to be populated. The results won't be very trustworthy until this is populated by the
    // stopRecording function.
    float baseline = 76;
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
    struct : halp::hslider_f32<"Ceil", halp::range{1., 4., 1.25}>
    {
      halp_meta(c_name, "ceil")
      halp_meta(description, "Heart-rate peak ceil")
      halp_flag(class_attribute);
    } ceil;

    struct : halp::hslider_i32<"Window", halp::irange{1, 10000, 1000}>
    {
      halp_meta(c_name, "window")
      halp_meta(description, "Time window in ms upon which the peaking analysis is performed")
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
      halp_meta(description, "Baseline data will be recorded when this is enabled. To compute the baseline for every sensor, stop the recording.")
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

  // Calcul des métriques d'excitation individuelles
  void computeIndividualMetrics(heartbeats& hb, std::chrono::milliseconds window);

  // Calcul des métriques d'excitation du groupe
  synchronization computeGroupMetrics();

private:
  timestamp m_last_point_timestamp{};
  boost::unordered_flat_map<std::string, heartbeats> beats;
  bool is_recording{};
};
}
