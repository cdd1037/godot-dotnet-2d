/**************************************************************************/
/*  test_godot_physics_2d.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_godot_physics_2d)

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GODOT_PHYSICS_2D_ENABLED

#include "modules/godot_physics_2d/godot_body_2d.h"
#include "modules/godot_physics_2d/godot_body_direct_state_2d.h"
#include "modules/godot_physics_2d/godot_space_2d.h"

namespace TestGodotPhysics2D {


TEST_CASE("[SceneTree][GodotPhysics2D][ThirdFixBatch] Point velocity uses transformed center of mass") {
	GodotBody2D body;
	body.set_linear_velocity(Vector2(4, 5));
	body.set_angular_velocity(2);
	body.set_param(PhysicsServer2D::BODY_PARAM_CENTER_OF_MASS, Vector2(3, 2));
	CHECK(body.get_direct_state()->get_velocity_at_local_position(Vector2(3, 2)).is_equal_approx(Vector2(4, 5)));
	CHECK(body.get_direct_state()->get_velocity_at_local_position(Vector2(4, 4)).is_equal_approx(Vector2(0, 7)));
	body.set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, Transform2D(Math::PI / 2, Vector2(100, 200)));
	const Vector2 center = body.get_center_of_mass();
	CHECK(center.is_equal_approx(Vector2(-2, 3)));
	CHECK(body.get_direct_state()->get_velocity_at_local_position(center).is_equal_approx(Vector2(4, 5)));
	CHECK(body.get_direct_state()->get_velocity_at_local_position(center + Vector2(1, 2)).is_equal_approx(Vector2(0, 7)));
	body.set_param(PhysicsServer2D::BODY_PARAM_CENTER_OF_MASS, Vector2());
	CHECK(body.get_direct_state()->get_velocity_at_local_position(Vector2(1, 2)).is_equal_approx(Vector2(0, 7)));
}

struct KinematicFixture {
	GodotRectangleShape2D shape;
	GodotArea2D default_area;
	GodotSpace2D space;
	GodotBody2D body;

	KinematicFixture() {
		shape.set_data(Vector2(1, 1));
		space.set_default_area(&default_area);
		body.set_mode(PhysicsServer2D::BODY_MODE_KINEMATIC);
		body.set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, Transform2D());
		body.add_shape(&shape);
		body.set_space(&space);
	}

	~KinematicFixture() {
		body.set_space(nullptr);
		body.remove_shape(0);
	}

	int candidates_at(const Vector2 &p_position) {
		GodotCollisionObject2D *results[1];
		return space.get_broadphase()->cull_aabb(Rect2(p_position - Vector2(0.1, 0.1), Vector2(0.2, 0.2)), results, 1);
	}

	int collisions_at(const Vector2 &p_position) {
		PhysicsDirectSpaceState2D::PointParameters parameters;
		parameters.position = p_position;
		PhysicsDirectSpaceState2D::ShapeResult results[1];
		return space.get_direct_state()->intersect_point(parameters, results, 1);
	}
};

TEST_CASE("[SceneTree][GodotPhysics2D][SecondFixBatch] Kinematic teleport without CCD keeps bounds local") {
	KinematicFixture fixture;
	const Vector2 destination(1000, 0);
	const Vector2 midpoint = destination * 0.5;
	const real_t step = 1.0 / 60.0;

	CHECK_EQ(fixture.collisions_at(Vector2()), 1);
	fixture.body.set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, Transform2D(0, destination));
	fixture.body.integrate_forces(step);

	// Non-CCD bodies must not send the entire teleport path to the broadphase.
	CHECK_EQ(fixture.candidates_at(midpoint), 0);
	CHECK(fixture.body.get_shape_aabb(0).size.x < 3);
	CHECK(fixture.body.get_linear_velocity().is_equal_approx(destination / step));

	fixture.body.integrate_velocities(step);
	CHECK_EQ(fixture.collisions_at(destination), 1);
	CHECK_EQ(fixture.collisions_at(Vector2()), 0);
	CHECK_EQ(fixture.candidates_at(midpoint), 0);
	CHECK_EQ(fixture.candidates_at(Vector2()), 0);
	CHECK(fixture.body.get_shape_aabb(0).size.x < 3);

	// A second teleport must refresh the bounds again, not leave stale candidates.
	const Vector2 return_position(-1000, 1000);
	fixture.body.set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, Transform2D(0, return_position));
	fixture.body.integrate_forces(step);
	fixture.body.integrate_velocities(step);
	CHECK_EQ(fixture.collisions_at(return_position), 1);
	CHECK_EQ(fixture.candidates_at(destination), 0);
	CHECK_EQ(fixture.candidates_at(Vector2(0, 500)), 0);
}

TEST_CASE("[SceneTree][GodotPhysics2D][SecondFixBatch] Kinematic CCD retains swept bounds") {
	KinematicFixture fixture;
	PhysicsServer2D::CCDMode mode = PhysicsServer2D::CCD_MODE_CAST_RAY;
	SUBCASE("Ray CCD") {}
	SUBCASE("Shape CCD") {
		mode = PhysicsServer2D::CCD_MODE_CAST_SHAPE;
	}
	fixture.body.set_continuous_collision_detection_mode(mode);
	const Vector2 destination(1000, 0);
	const Vector2 midpoint = destination * 0.5;
	const real_t step = 1.0 / 60.0;
	fixture.body.set_state(PhysicsServer2D::BODY_STATE_TRANSFORM, Transform2D(0, destination));
	fixture.body.integrate_forces(step);

	CHECK_EQ(fixture.candidates_at(Vector2()), 1);
	CHECK_EQ(fixture.candidates_at(midpoint), 1);
	CHECK_EQ(fixture.candidates_at(destination), 1);
	CHECK(fixture.body.get_shape_aabb(0).size.x >= 1002);
	CHECK(fixture.body.get_linear_velocity().is_equal_approx(destination / step));

	fixture.body.integrate_velocities(step);
	// CCD keeps the sweep for this step, but collision queries use the final shape.
	CHECK_EQ(fixture.candidates_at(midpoint), 1);
	CHECK_EQ(fixture.collisions_at(midpoint), 0);
	CHECK_EQ(fixture.collisions_at(Vector2()), 0);
	CHECK_EQ(fixture.collisions_at(destination), 1);

	// A stationary following step removes the previous sweep.
	fixture.body.integrate_forces(step);
	fixture.body.integrate_velocities(step);
	CHECK_EQ(fixture.candidates_at(midpoint), 0);
	CHECK_EQ(fixture.collisions_at(destination), 1);
}

} // namespace TestGodotPhysics2D

#endif // MODULE_GODOT_PHYSICS_2D_ENABLED
