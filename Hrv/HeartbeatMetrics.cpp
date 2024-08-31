#include "HeartbeatMetrics.hpp"

#include <iostream>
namespace fheel
{

void HeartbeatMetrics::startRecording()
{
  // Recreate the accumulators for stats
  // Global ones
  std::destroy_at(&accumulators);
  std::construct_at(&accumulators);

  // Per-sensor
  for(auto& [name, hb] : this->beats)
  {
    std::destroy_at(&hb.accumulators);
    std::construct_at(&hb.accumulators);
  }
}

void HeartbeatMetrics::stopRecording()
{
  inputs.baseline.value = ba::extract::mean(accumulators);
  global_stddev = std::sqrt(ba::extract::variance(accumulators));

  for(auto& [name, hb] : this->beats)
  {
    hb.average = ba::extract::mean(hb.accumulators);
    hb.stddev = std::sqrt(ba::extract::variance(hb.accumulators));
  }
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
    this->accumulators(bpm);
    hb.accumulators(bpm);
  }

  computeIndividualMetrics(hb, std::chrono::milliseconds(inputs.window.value));

  this->outputs.excitation(excitation{
      .name = it->first,
      .peaking = hb.stats.peaking,
      .peak = hb.stats.peak,
      .average = hb.stats.average,
      .variance = hb.stats.variance,
      .stddev = hb.stats.stddev,
      .rmssd = hb.stats.rmssd,
  });
  computeGroupMetrics();
}

void HeartbeatMetrics::cleanupOldTimestamps()
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

void HeartbeatMetrics::computeIndividualMetrics(
    heartbeats& hb, std::chrono::milliseconds window)
{
  auto& stats = hb.stats;
  double prev_average = stats.average;
  double prev_stddev = stats.stddev;
  if(prev_stddev <= 0)
    prev_stddev = 1.;
  stats.bpms.clear();
  stats.count = 0;
  stats.peak = 0.;
  stats.average = 0.;
  stats.stddev = 0.;
  stats.rmssd = 0.;
  stats.variance = 0.;

  // Compute basic statistics
  for(auto& [t, bpm] : hb.data)
  {
    if((this->m_last_point_timestamp - t) < window)
    {
      if(bpm > 0)
      { // FIXME divide by stddev ? Individual or group ?
        const float beats = (bpm - inputs.baseline) / 1.;
        stats.bpms.push_back(beats);
        stats.average += beats;

        if(std::abs(beats) > std::abs(stats.peak))
          stats.peak = beats;
      }
    }
  }

  stats.count = stats.bpms.size();
  if(stats.count <= 1)
    return;

  stats.average /= stats.count;

  for(float bpm : stats.bpms)
  {
    stats.variance += std::pow(bpm - stats.average, 2.f);
  }

  stats.variance /= stats.count;
  stats.stddev = std::sqrt(stats.variance);

  // Method 1. Compare peak
  const float last_bpm = hb.data.back().second;
  stats.peaking = false;
  if(last_bpm >= inputs.baseline)
  {
    // 140 / 74 > 2 ?
    if(last_bpm / inputs.baseline > inputs.ceil)
      stats.peaking = true;
  }
  else if(last_bpm < inputs.baseline)
  {
    // Q. pour détection de peak: peut-on vraiment prendre 2 * / 0.5* la baseline?
    // (e.g. être symmétrique par rapport à la lenteur / excitation dans les deux sens)
    // Ça supposerait que les battements de coeur c'est un phénomène linéaire et ça mesemble pas très réaliste
    // comme hypothèse

    // 50 / 74 < 0.5 ?
    if(last_bpm / inputs.baseline < 1 / inputs.ceil)
      stats.peaking = true;
  }

  // Method 2. Compute rmssd
  stats.rmssd = 0.;

#pragma omp simd
  for(int i = 0; i < stats.count - 1; i++)
  {
    // 1. BPM to RR
    float v0 = 60. * 1000. / stats.bpms[i];
    float v1 = 60. * 1000. / stats.bpms[i + 1];

    // 2. Mean
    stats.rmssd += std::pow(v1 - v0, 2);
  }

  // 3. RMSSD
  stats.rmssd = std::sqrt(stats.rmssd / (stats.count - 1));
}

void HeartbeatMetrics::outputIndividualMetrics()
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

void HeartbeatMetrics::computeGroupMetrics()
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
      if(std::abs(hb.stats.average) <= (std::abs(inputs.stddev * stddev) + avg))
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
}
