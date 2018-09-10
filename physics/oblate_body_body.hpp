﻿
#pragma once

#include "physics/oblate_body.hpp"

#include <algorithm>
#include <set>
#include <vector>

#include "astronomy/epoch.hpp"
#include "numerics/legendre.hpp"
#include "quantities/constants.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"

namespace principia {
namespace physics {
namespace internal_oblate_body {

using astronomy::J2000;
using geometry::AngularVelocity;
using geometry::Instant;
using numerics::LegendreNormalizationFactor;
using quantities::Angle;
using quantities::SIUnit;
using quantities::si::Radian;
using quantities::si::Second;

template<typename Frame>
OblateBody<Frame>::Parameters::Parameters(double const j2,
                                          Length const& reference_radius)
    : j2_(j2),
      j2_over_μ_(j2 * reference_radius * reference_radius),
      reference_radius_(reference_radius) {
  CHECK_LT(0.0, j2) << "Oblate body must have positive j2";
}

template<typename Frame>
OblateBody<Frame>::Parameters::Parameters(double const j2,
                                          double const c22,
                                          double const s22,
                                          Length const& reference_radius)
    : reference_radius_(reference_radius),
      j2_(j2),
      j2_over_μ_(j2 * reference_radius * reference_radius),
      c22_over_μ_(c22 * reference_radius * reference_radius),
      s22_over_μ_(s22 * reference_radius * reference_radius) {
  CHECK_LT(0.0, j2) << "Oblate body must have positive j2";
  CHECK_NE(0.0, c22) << "Oblate body cannot have zero c22";
  CHECK_NE(0.0, s22) << "Oblate body cannot have zero s22";
}

template<typename Frame>
OblateBody<Frame>::Parameters::Parameters(double const j2,
                                          double const c22,
                                          double const s22,
                                          double const j3,
                                          Length const& reference_radius)
    : reference_radius_(reference_radius),
      j2_(j2),
      j2_over_μ_(j2 * reference_radius * reference_radius),
      c22_over_μ_(c22 * reference_radius * reference_radius),
      s22_over_μ_(s22 * reference_radius * reference_radius),
      j3_over_μ_(j3 * reference_radius * reference_radius * reference_radius) {
  CHECK_LT(0.0, j2) << "Oblate body must have positive j2";
  CHECK_NE(0.0, c22) << "Oblate body cannot have zero c22";
  CHECK_NE(0.0, s22) << "Oblate body cannot have zero s22";
  CHECK_NE(0.0, j3) << "Oblate body cannot have zero j3";
}

template<typename Frame>
OblateBody<Frame>::Parameters::Parameters(Length const& reference_radius)
    : reference_radius_(reference_radius),
      cos_(typename OblateBody<Frame>::GeopotentialCoefficients()),
      sin_(typename OblateBody<Frame>::GeopotentialCoefficients()),
      degree_(0) {}

template<typename Frame>
typename OblateBody<Frame>::Parameters
OblateBody<Frame>::Parameters::ReadFromMessage(
    serialization::OblateBody::Geopotential const& message,
    Length const& reference_radius) {
  Parameters parameters(reference_radius);
  std::set<int> degrees_seen;
  for (auto const& row : message.row()) {
    const int n = row.degree();
    CHECK_LE(n, OblateBody<Frame>::max_geopotential_degree);
    CHECK(degrees_seen.insert(n).second)
        << "Degree " << n << " specified multiple times";
    CHECK_LE(row.column_size(), n + 1)
        << "Degree " << n << " has " << row.column_size() << " coefficients";
    std::set<int> orders_seen;
    for (auto const& column : row.column()) {
      const int m = column.order();
      CHECK_LE(m, n);
      CHECK(orders_seen.insert(m).second)
          << "Degree " << n << " order " << m << " specified multiple times";
      (*parameters.cos_)[n][m] = column.cos();
      (*parameters.sin_)[n][m] = column.sin();
    }
  }
  parameters.degree_ = *degrees_seen.crbegin();

  // Unnormalization.
  parameters.j2_ =
      -(*parameters.cos_)[2][0] * LegendreNormalizationFactor(2, 0);
  parameters.j2_over_μ_ = -(*parameters.cos_)[2][0] *
                          LegendreNormalizationFactor(2, 0) * reference_radius *
                          reference_radius;
  parameters.c22_over_μ_ = (*parameters.cos_)[2][2] *
                           LegendreNormalizationFactor(2, 2) *
                           reference_radius * reference_radius;
  parameters.s22_over_μ_ = (*parameters.sin_)[2][2] *
                           LegendreNormalizationFactor(2, 2) *
                           reference_radius * reference_radius;
  parameters.j3_over_μ_ = -(*parameters.cos_)[3][0] *
                          LegendreNormalizationFactor(3, 0) * reference_radius *
                          reference_radius * reference_radius;

  return parameters;
}

template<typename Frame>
void OblateBody<Frame>::Parameters::WriteToMessage(
    not_null<serialization::OblateBody::Geopotential*> const message) const {
  for (int n = 0; n <= *degree_; ++n) {
    auto const row = message->add_row();
    row->set_degree(n);
    for (int m = 0; m <= n; ++m) {
      auto const column = row->add_column();
      column->set_order(m);
      column->set_cos((*cos_)[n][m]);
      column->set_sin((*sin_)[n][m]);
    }
  }
}

#define PRINCIPIA_FILL_OBLATE_BODY_PARAMETERS(name)                    \
  if (parameters_.name##_) {                                           \
    parameters_.name##_over_μ_ =                                       \
        *parameters_.name##_ / this->gravitational_parameter();        \
  }                                                                    \
  if (parameters_.name##_over_μ_) {                                    \
    parameters_.name##_ =                                              \
        *parameters_.name##_over_μ_ * this->gravitational_parameter(); \
  }

template<typename Frame>
OblateBody<Frame>::OblateBody(
    MassiveBody::Parameters const& massive_body_parameters,
    typename RotatingBody<Frame>::Parameters const& rotating_body_parameters,
    Parameters const& parameters)
    : RotatingBody<Frame>(massive_body_parameters, rotating_body_parameters),
      parameters_(parameters) {
  PRINCIPIA_FILL_OBLATE_BODY_PARAMETERS(c22);
  PRINCIPIA_FILL_OBLATE_BODY_PARAMETERS(s22);
  PRINCIPIA_FILL_OBLATE_BODY_PARAMETERS(j3);
}

#undef PRINCIPIA_FILL_OBLATE_BODY_PARAMETERS

template<typename Frame>
double OblateBody<Frame>::j2() const {
  return *parameters_.j2_;
}

template<typename Frame>
Quotient<Degree2SphericalHarmonicCoefficient,
         GravitationalParameter> const& OblateBody<Frame>::j2_over_μ() const {
  return *parameters_.j2_over_μ_;
}

template<typename Frame>
Degree2SphericalHarmonicCoefficient const OblateBody<Frame>::c22() const {
  return parameters_.c22_.value_or(Degree2SphericalHarmonicCoefficient());
}

template<typename Frame>
Quotient<Degree2SphericalHarmonicCoefficient, GravitationalParameter> const
    OblateBody<Frame>::c22_over_μ() const {
  return parameters_.c22_over_μ_.value_or(
      Quotient<Degree2SphericalHarmonicCoefficient, GravitationalParameter>());
}

template<typename Frame>
Degree2SphericalHarmonicCoefficient const OblateBody<Frame>::s22() const {
  return parameters_.s22_.value_or(Degree2SphericalHarmonicCoefficient());
}

template<typename Frame>
Quotient<Degree2SphericalHarmonicCoefficient, GravitationalParameter> const
    OblateBody<Frame>::s22_over_μ() const {
  return parameters_.s22_over_μ_.value_or(
      Quotient<Degree2SphericalHarmonicCoefficient, GravitationalParameter>());
}

template<typename Frame>
Degree3SphericalHarmonicCoefficient const OblateBody<Frame>::j3() const {
  return parameters_.j3_.value_or(Degree3SphericalHarmonicCoefficient());
}

template<typename Frame>
Quotient<Degree3SphericalHarmonicCoefficient, GravitationalParameter> const
    OblateBody<Frame>::j3_over_μ() const {
  return parameters_.j3_over_μ_.value_or(
      Quotient<Degree3SphericalHarmonicCoefficient, GravitationalParameter>());
}

template<typename Frame>
typename OblateBody<Frame>::GeopotentialCoefficients const&
OblateBody<Frame>::cos() const {
  return *parameters_.cos_;
}

template<typename Frame>
typename OblateBody<Frame>::GeopotentialCoefficients const&
OblateBody<Frame>::sin() const {
  return *parameters_.sin_;
}

template<typename Frame>
int OblateBody<Frame>::geopotential_degree() const {
  return *parameters_.degree_;
}

template<typename Frame>
Length const& OblateBody<Frame>::reference_radius() const {
  return parameters_.reference_radius_;
}

template<typename Frame>
bool OblateBody<Frame>::has_c22() const {
  return parameters_.c22_.has_value();
}

template<typename Frame>
bool OblateBody<Frame>::has_s22() const {
  return parameters_.s22_.has_value();
}

template<typename Frame>
bool OblateBody<Frame>::has_j3() const {
  return parameters_.j3_.has_value();
}

template<typename Frame>
bool OblateBody<Frame>::has_geopotential() const {
  return parameters_.cos_.has_value();
}

template<typename Frame>
bool OblateBody<Frame>::is_massless() const {
  return false;
}

template<typename Frame>
bool OblateBody<Frame>::is_oblate() const {
  return true;
}

template<typename Frame>
void OblateBody<Frame>::WriteToMessage(
    not_null<serialization::Body*> const message) const {
  WriteToMessage(message->mutable_massive_body());
}

template<typename Frame>
void OblateBody<Frame>::WriteToMessage(
    not_null<serialization::MassiveBody*> const message) const {
  RotatingBody<Frame>::WriteToMessage(message);
  not_null<serialization::OblateBody*> const oblate_body =
      message->MutableExtension(serialization::RotatingBody::extension)->
               MutableExtension(serialization::OblateBody::extension);
  parameters_.reference_radius_.WriteToMessage(
      oblate_body->mutable_reference_radius());
  if (has_geopotential()) {
    parameters_.WriteToMessage(oblate_body->mutable_geopotential());
  } else {
    CHECK(parameters_.j2_.has_value());
    oblate_body->set_j2(*parameters_.j2_);
  }
}

template<typename Frame>
not_null<std::unique_ptr<OblateBody<Frame>>> OblateBody<Frame>::ReadFromMessage(
      serialization::OblateBody const& message,
      MassiveBody::Parameters const& massive_body_parameters,
      typename RotatingBody<Frame>::Parameters const&
          rotating_body_parameters) {
  std::unique_ptr<Parameters> parameters;
  switch (message.oblateness_case()) {
    case serialization::OblateBody::OblatenessCase::kPreDiophantosJ2: {
      // In the legacy case we didn't record the reference radius, so we use a
      // dummy value to achieve the right effect.
      CHECK(!message.has_reference_radius()) << message.DebugString();
      Length const reference_radius = SIUnit<Length>();
      parameters = std::make_unique<Parameters>(
          Degree2SphericalHarmonicCoefficient::ReadFromMessage(
              message.pre_diophantos_j2()) /
              (massive_body_parameters.gravitational_parameter() *
               reference_radius * reference_radius),
          reference_radius);
      break;
    }
    case serialization::OblateBody::OblatenessCase::kJ2:
      CHECK(message.has_reference_radius()) << message.DebugString();
      parameters = std::make_unique<Parameters>(
                       message.j2(),
                       Length::ReadFromMessage(message.reference_radius()));
      break;
    case serialization::OblateBody::OblatenessCase::kGeopotential:
      CHECK(message.has_reference_radius()) << message.DebugString();
      parameters = std::make_unique<Parameters>(Parameters::ReadFromMessage(
          message.geopotential(),
          Length::ReadFromMessage(message.reference_radius())));
      break;
    case serialization::OblateBody::OblatenessCase::OBLATENESS_NOT_SET:
      LOG(FATAL) << message.DebugString();
      base::noreturn();
  }
  return std::make_unique<OblateBody<Frame>>(massive_body_parameters,
                                             rotating_body_parameters,
                                             *parameters);
}

}  // namespace internal_oblate_body
}  // namespace physics
}  // namespace principia
