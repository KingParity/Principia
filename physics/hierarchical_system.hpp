#pragma once

#include <map>
#include <vector>

#include "base/not_null.hpp"
#include "geometry/frame.hpp"
#include "geometry/identity.hpp"
#include "physics/degrees_of_freedom.hpp"
#include "physics/jacobi_coordinates.hpp"
#include "physics/kepler_orbit.hpp"
#include "physics/massive_body.hpp"

namespace principia {

using base::not_null;

namespace physics {

template<typename Frame>
class HierarchicalSystem {
 public:
  struct BarycentricSystem {
    std::vector<not_null<std::unique_ptr<MassiveBody const>>> bodies;
    std::vector<DegreesOfFreedom<Frame>> degrees_of_freedom;
  };

  explicit HierarchicalSystem(
      not_null<std::unique_ptr<MassiveBody const>> primary);

  // Adds the given |body| with the given |parent|.  |parent| must already have
  // been inserted.  |jacobi_osculating_elements| must be a valid argument to
  // the constructor of |KeplerOrbit|.
  void Add(not_null<std::unique_ptr<MassiveBody const>> body,
           not_null<MassiveBody const*> const parent,
           KeplerianElements<Frame> const& jacobi_osculating_elements);

  // Puts the barycentre of the system at the motionless origin of |Frame|;
  // |ConsumeBarycentricSystem().bodies| is in preorder, where the satellites of
  // a body are ordered by increasing semimajor axis.
  // |*this| is invalid after a call to |ConsumeBarycentricSystem()|.
  BarycentricSystem ConsumeBarycentricSystem();

 private:
  struct Subsystem;
  // A |System| represents a |primary| body with orbiting |Subsystem|s, e.g.,
  // the sun and its orbiting planetary systems.  |satellites| may be empty,
  // representing the body |primary| with no satellites, e.g., Venus.
  struct System {
    explicit System(not_null<std::unique_ptr<MassiveBody const>> primary)
        : primary(std::move(primary)) {}
    virtual ~System() = default;
    not_null<std::unique_ptr<MassiveBody const>> primary;
    std::vector<not_null<std::unique_ptr<Subsystem>>> satellites;
  };
  // A |Subsystem| is a |System| with osculating elements, seen as the
  // osculating elements of its barycentre around the inner parent subsystem,
  // e.g., the elements of the Jovian |Subsystem| would be the osculating
  // elements of the barycentre of the Jovian system around the barycentre of
  // the Sun and inner planets.
  struct Subsystem : public System {
    explicit Subsystem(not_null<std::unique_ptr<MassiveBody const>> primary)
        : System(std::move(primary)) {}
    KeplerianElements<Frame> jacobi_osculating_elements;
  };

  // Data about a |Subsystem|.
  struct BarycentricSubystem {
    // A |MassiveBody| with the mass of the whole subsystem.
    std::unique_ptr<MassiveBody> equivalent_body;
    // The bodies composing the subsystem, in preorder, where the satellites
    // are ordered by increasing semimajor axis.
    std::vector<not_null<std::unique_ptr<MassiveBody const>>> bodies;
    // Their |DegreesOfFreedom| relative to the barycentre of the subsystem, in
    // the same order.
    std::vector<RelativeDegreesOfFreedom<Frame>> barycentric_degrees_of_freedom;
  };

  // Invalidates its argument.
  static BarycentricSubystem ToBarycentric(System& system);

  System system_;
  // Invariant: |subsystems_[p]->primary.get() == p|.
  // None of these pointers should be null, but I want to use operator[].
  std::map<not_null<MassiveBody const*>, System*> systems_;
};

}  // namespace physics
}  // namespace principia

#include "physics/hierarchical_system_body.hpp"