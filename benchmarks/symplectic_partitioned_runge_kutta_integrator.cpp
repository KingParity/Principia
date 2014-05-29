﻿#define GLOG_NO_ABBREVIATED_SEVERITIES
#undef TRACE_SYMPLECTIC_PARTITIONED_RUNGE_KUTTA_INTEGRATOR

#include <algorithm>

#include "benchmark/benchmark.h"
#include "integrators/symplectic_partitioned_runge_kutta_integrator.hpp"

using principia::integrators::SPRKIntegrator;

namespace principia {
namespace benchmarks {

namespace {

inline void compute_harmonic_oscillator_force(double const t,
                                              std::vector<double> const& q,
                                              std::vector<double>* result) {
  (*result)[0] = -q[0];
}

inline void compute_harmonice_oscillator_velocity(std::vector<double> const& p,
                                                  std::vector<double>* result) {
  (*result)[0] = p[0];
}

}  // namespace

void SolveHarmonicOscillator(benchmark::State* state,
                             double* q_error,
                             double* p_error) {
  SPRKIntegrator integrator;
  SPRKIntegrator::Parameters parameters;
  SPRKIntegrator::Solution solution;

  parameters.q0 = {1.0};
  parameters.p0 = {0.0};
  parameters.t0 = 0.0;
#ifdef _DEBUG
  parameters.tmax = 100.0;
#else
  parameters.tmax = 1000.0;
#endif
  parameters.Δt = 1.0E-4;
  parameters.coefficients = integrator.Order5Optimal();
  parameters.sampling_period = 1;
  integrator.Solve(&compute_harmonic_oscillator_force,
                    &compute_harmonice_oscillator_velocity,
                    parameters,
                    &solution);

  state->PauseTiming();
  *q_error = 0;
  *p_error = 0;
  for (size_t i = 0; i < solution.time.quantities.size(); ++i) {
    *q_error = std::max(*q_error,
                        std::abs(solution.position[0].quantities[i] -
                                 std::cos(solution.time.quantities[i])));
    *p_error = std::max(*p_error,
                        std::abs(solution.momentum[0].quantities[i] +
                                 std::sin(solution.time.quantities[i])));
  }
  state->ResumeTiming();
}

static void BM_SolveHarmonicOscillator(benchmark::State& state) {
  double q_error;
  double p_error;
  while (state.KeepRunning()) {
    SolveHarmonicOscillator(&state, &q_error, &p_error);
  }
  std::stringstream ss;
  ss << q_error << ", " << p_error;
  state.SetLabel(ss.str());
}
BENCHMARK(BM_SolveHarmonicOscillator);

}  // namespace benchmarks
}  // namespace principia

int main(int argc, const char* argv[]) {
  benchmark::Initialize(&argc, argv);

  benchmark::RunSpecifiedBenchmarks();
}
