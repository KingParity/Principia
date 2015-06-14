﻿#pragma once

#include <algorithm>
#include <limits>
#include <vector>

#include "physics/continuous_trajectory.hpp"
#include "testing_utilities/numerics.hpp"

namespace principia {

using testing_utilities::ULPDistance;

namespace physics {

namespace {

int const kMaxDegree = 17;
int const kMinDegree = 3;
int const kMaxDegreeAge = 100;

// Only supports 8 divisions for now.
int const kDivisions = 8;

}  // namespace

template<typename Frame>
ContinuousTrajectory<Frame>::ContinuousTrajectory(Time const& step,
                                                  Length const& low_tolerance,
                                                  Length const& high_tolerance)
    : step_(step),
      low_tolerance_(low_tolerance),
      high_tolerance_(high_tolerance),
      adjusted_low_tolerance_(low_tolerance_),
      adjusted_high_tolencenc_(high_tolerance_),
      degree_(kMinDegree) {
  CHECK_LT(low_tolerance_, high_tolerance_);
}

template<typename Frame>
bool ContinuousTrajectory<Frame>::empty() const {
  return series_.empty();
}

template<typename Frame>
Instant ContinuousTrajectory<Frame>::t_min() const {
  CHECK(!empty()) << "Empty trajectory";
  return *first_time_;
}

template<typename Frame>
Instant ContinuousTrajectory<Frame>::t_max() const {
  CHECK(!empty()) << "Empty trajectory";
  return series_.back().t_max();
}

template<typename Frame>
void ContinuousTrajectory<Frame>::Append(
    Instant const& time,
    DegreesOfFreedom<Frame> const& degrees_of_freedom) {
  // Consistency checks.
  if (first_time_ == nullptr) {
    first_time_ = std::make_unique<Instant>(time);
  } else {
    Instant const t0;
    CHECK_GE(1,
             ULPDistance((last_points_.back().first + step_ - t0) /
                             SIUnit<Time>(),
                         (time - t0) / SIUnit<Time>()))
        << "Append at times that are not equally spaced";
  }

  if (last_points_.size() == kDivisions) {
    // These vectors are static to avoid deallocation/reallocation each time we
    // go through this code path.
    static std::vector<Displacement<Frame>> q(kDivisions + 1);
    static std::vector<Velocity<Frame>> v(kDivisions + 1);
    q.clear();
    v.clear();

    for (auto const& pair : last_points_) {
      DegreesOfFreedom<Frame> const& degrees_of_freedom = pair.second;
      q.push_back(degrees_of_freedom.position() - Frame::origin);
      v.push_back(degrees_of_freedom.velocity());
    }
    q.push_back(degrees_of_freedom.position() - Frame::origin);
    v.push_back(degrees_of_freedom.velocity());

    // If the |degree_| is too old, restart from the lowest degree.
    if (degree_age_ > kMaxDegreeAge) {
      degree = kMinDegree;
      degree_age_ = 0;
    }

    // Compute the approximation with the current |degree_|.
    series_.push_back(
        ЧебышёвSeries<Displacement<Frame>>::NewhallApproximation(
            degree_, q, v, last_points_.cbegin()->first, time));

    Length error_estimate = series_.back().last_coefficient().Norm();
    Length previous_error_estimate = error_estimate + error_estimate;

    // If we are in the zone of numerical instabilities and we exceeded the high
    // tolerance, restart from the lowest degree.
    if (is_unstable_ && error_estimate > adjusted_high_tolerance_) {
      degree_ = kMinDegree - 1;
      degree_age_ = 0;
    }

    // Increase the degree if the approximation is not accurate enough.  Stop
    // when we reach the maximum degree or when the error estimate is not
    // decreasing.
    while (error_estimate > adjusted_high_tolerance_ &&
           error_estimate < previous_error_estimate &&
           degree_ < kMaxDegree) {
      ++degree_;
      VLOG(1) << "Increasing degree for " << this << " to " <<degree_
              << " because error estimate was " << error_estimate;
      series_.back() =
          ЧебышёвSeries<Displacement<Frame>>::NewhallApproximation(
              degree_, q, v, last_points_.cbegin()->first, time);
      previous_error_estimate = error_estimate;
      error_estimate = series_.back().last_coefficient().Norm();
    }

    if (error_estimate < high_tolerance_) {
      adjusted_low_tolerance_ = std::min(adjusted_low_tolerance_,
                                         error_estimate);
    } else if (error_estimate >= previous_error_estimate) {
      // We have entered the zone of numerical instability.  Go back to the
      // point where the error was decreasing and nudge the high tolerance since
      // we won't be able to reliably do better than that.
      is_unstable_ = true;
      error_estimate = previous_error_estimate;
      --degree_;
      adjusted_high_tolerance_ = std::max(adjusted_high_tolerance_,
                                          error_estimate);
    } else {
      adjusted_low_tolerance_ 
    }

    ++degree_age_;

    // Wipe-out the vector.
    last_points_.clear();
  }

  // Note that we only insert the new point in the map *after* computing the
  // approximation, because clearing the map is much more efficient than erasing
  // every element but one.
  last_points_.emplace_back(time, degrees_of_freedom);
}

template<typename Frame>
void ContinuousTrajectory<Frame>::ForgetBefore(Instant const& time) {
  series_.erase(series_.begin(), FindSeriesForInstant(time));

  // If there are no |series_| left, clear everything.  Otherwise, update the
  // first time.
  if (series_.empty()) {
    first_time_.reset();
    last_points_.clear();
  } else {
    *first_time_ = time;
  }
}

template<typename Frame>
Position<Frame> ContinuousTrajectory<Frame>::EvaluatePosition(
    Instant const& time,
    Hint* const hint) const {
  CHECK_LE(t_min(), time);
  CHECK_GE(t_max(), time);
  if (MayUseHint(time, hint)) {
    return series_[hint->index_].Evaluate(time) + Frame::origin;
  } else {
    auto const it = FindSeriesForInstant(time);
    if (hint != nullptr) {
      hint->index_ = it - series_.cbegin();
    }
    return it->Evaluate(time) + Frame::origin;
  }
}

template<typename Frame>
Velocity<Frame> ContinuousTrajectory<Frame>::EvaluateVelocity(
    Instant const& time,
    Hint* const hint) const {
  CHECK_LE(t_min(), time);
  CHECK_GE(t_max(), time);
  if (MayUseHint(time, hint)) {
    return series_[hint->index_].EvaluateDerivative(time);
  } else {
    auto const it = FindSeriesForInstant(time);
    if (hint != nullptr) {
      hint->index_ = it - series_.cbegin();
    }
    return it->EvaluateDerivative(time);
  }
}

template<typename Frame>
DegreesOfFreedom<Frame> ContinuousTrajectory<Frame>::EvaluateDegreesOfFreedom(
    Instant const& time,
    Hint* const hint) const {
  CHECK_LE(t_min(), time);
  CHECK_GE(t_max(), time);
  if (MayUseHint(time, hint)) {
    ЧебышёвSeries<Displacement<Frame>> const& series = series_[hint->index_];
    return DegreesOfFreedom<Frame>(series.Evaluate(time) + Frame::origin,
                                   series.EvaluateDerivative(time));
  } else {
    auto const it = FindSeriesForInstant(time);
    if (hint != nullptr) {
      hint->index_ = it - series_.cbegin();
    }
    return DegreesOfFreedom<Frame>(it->Evaluate(time) + Frame::origin,
                                   it->EvaluateDerivative(time));
  }
}

template<typename Frame>
ContinuousTrajectory<Frame>::Hint::Hint()
    : index_(std::numeric_limits<int>::max()) {}

template<typename Frame>
typename std::vector<ЧебышёвSeries<Displacement<Frame>>>::const_iterator
ContinuousTrajectory<Frame>::FindSeriesForInstant(Instant const& time) const {
  // Need to use |lower_bound|, not |upper_bound|, because it allows
  // heterogeneous arguments.
  auto const it = std::lower_bound(series_.begin(), series_.end(), time,
                      [](ЧебышёвSeries<Displacement<Frame>> const& left,
                         Instant const& right) {
                        return left.t_max() < right;
                      });
  CHECK(it != series_.end());
  return it;
}

template<typename Frame>
bool ContinuousTrajectory<Frame>::MayUseHint(Instant const& time,
                                             Hint* const hint) const {
  if (hint != nullptr) {
    // A shorthand for the index held by the |hint|.
    int& index = hint->index_;
    if (index < series_.size() && series_[index].t_min() <= time) {
      if (time <= series_[index].t_max()) {
        // Use this interval.
        return true;
      } else if (index < series_.size() - 1 &&
                 time <= series_[index + 1].t_max()) {
        // Move to the next interval.
        ++index;
        return true;
      }
    }
  }
  return false;
}

}  // namespace physics
}  // namespace principia
