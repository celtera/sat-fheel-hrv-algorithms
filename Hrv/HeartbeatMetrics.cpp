#include "HeartbeatMetrics.hpp"

#include <iostream>
namespace fheel
{

void HeartbeatMetrics::startRecording()
{
  // destruct and reconstruct the accumulators for every sensor.
  for(auto& [name, hb] : this->beats)
  {
    std::destroy_at(&hb.accumulators);
    std::construct_at(&hb.accumulators);
  }
  is_recording = true;
}

void HeartbeatMetrics::stopRecording()
{
  if(is_recording)
  {
    for(auto& [name, hb] : this->beats)
    {
      hb.baseline = ba::extract::mean(hb.accumulators);
    }
  }
  is_recording = false;
}

void HeartbeatMetrics::addRow(const std::string& name, int bpm)
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

  if(inputs.recording)
  {
    hb.accumulators(bpm);
  }

  computeIndividualMetrics(hb, std::chrono::milliseconds(inputs.window.value));
  auto sync = computeGroupMetrics();

  this->outputs.excitation(excitation{
      .name = it->first,
      .bpm = bpm,
      .percent_of_baseline = hb.stats.current_percent_of_baseline,
      .distance_from_average = hb.stats.current_percent_of_baseline - sync.average_percent_of_baseline,
      .baseline = hb.baseline,
      .peaking = hb.stats.peaking,
      .peak = hb.stats.peak,
  });
  this->outputs.synchronization(sync);
}

void HeartbeatMetrics::computeIndividualMetrics(
    heartbeats& hb, std::chrono::milliseconds window)
{
  auto& stats = hb.stats;
  stats.bpms.clear();
  stats.peak = 0.;

  // Compute basic statistics
  for(auto& [t, bpm] : hb.data)
  {
    if((this->m_last_point_timestamp - t) < window)
    {
      if(bpm > 0)
      {
        const float percent_of_baseline = (bpm / hb.baseline);
        stats.current_percent_of_baseline = percent_of_baseline;
        stats.bpms.push_back(bpm);
        if(std::abs(bpm) > std::abs(stats.peak))
          stats.peak = bpm;
      }
    }
  }

  // Method 1. Compare peak
  const float last_bpm = hb.data.back().second;
  stats.peaking = false;
  if(last_bpm >= hb.baseline)
  {
    // 140 / 74 > 2 ?
    if(last_bpm / hb.baseline > inputs.ceil)
      stats.peaking = true;
  }
  else if(last_bpm < hb.baseline)
  {
    // Q. pour détection de peak: peut-on vraiment prendre 2 * / 0.5* la baseline?
    // (e.g. être symmétrique par rapport à la lenteur / excitation dans les deux sens)
    // Ça supposerait que les battements de coeur c'est un phénomène linéaire et ça mesemble pas très réaliste
    // comme hypothèse

    // 50 / 74 < 0.5 ?
    if(last_bpm / hb.baseline < 1 / inputs.ceil)
      stats.peaking = true;
  }
}

synchronization HeartbeatMetrics::computeGroupMetrics()
{
  synchronization sync{};

  // Method 2. Standard deviation distance
  // 1. Global mean
  int total_samples = 0;
  // average percent of baseline
  float avg = 0.f;
  float var = 0.f;
  float stddev = 0.f;

  // Global average
  for(auto& [name, b] : beats)
  {
    auto& stats = b.stats;
    total_samples ++;
    avg += stats.current_percent_of_baseline;
  }

  if(total_samples == 0)
    return;

  avg /= total_samples;

  // Global variance
  for(auto& [name, b] : beats)
  {
    auto& stats = b.stats;
    var += std::pow(stats.current_percent_of_baseline - avg, 2.f);
  }

  stddev = std::sqrt(var / total_samples);

  int pop = total_samples;
  int pop_within_stddev = 0;
  for(auto& [name, hb] : beats)
  {
    if(std::abs(hb.stats.current_percent_of_baseline) <= (std::abs(inputs.stddev * stddev) + avg))
    {
      pop_within_stddev++;
    }
  }

  sync.deviation = float(pop_within_stddev) / float(pop);
  sync.average_percent_of_baseline = avg;
  // Method 3. Coefficient of variation
  sync.coeff_variation = stddev / avg;
  return sync;
}
}
