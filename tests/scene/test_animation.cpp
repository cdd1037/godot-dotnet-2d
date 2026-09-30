/**************************************************************************/
/*  test_animation.cpp                                                    */
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

TEST_FORCE_LINK(test_animation)

#include "scene/resources/animation.h"

namespace TestAnimation {

TEST_CASE("[Animation] Empty animation getters") {
	const Ref<Animation> animation = memnew(Animation);

	CHECK(animation->get_length() == doctest::Approx(real_t(1.0)));
	CHECK(animation->get_step() == doctest::Approx(real_t(1.0 / 30)));
}

TEST_CASE("[Animation] Create value track") {
	// This creates an animation that makes the node "Enemy" move to the right by
	// 100 pixels in 0.5 seconds.
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_VALUE);
	CHECK(track_index == 0);
	animation->track_set_path(track_index, NodePath("Enemy:position:x"));
	animation->track_insert_key(track_index, 0.0, 0);
	animation->track_insert_key(track_index, 0.5, 100);

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));
	CHECK(int(animation->track_get_key_value(0, 0)) == 0);
	CHECK(int(animation->track_get_key_value(0, 1)) == 100);

	CHECK(animation->value_track_interpolate(0, -0.2) == doctest::Approx(0.0));
	CHECK(animation->value_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	CHECK(animation->value_track_interpolate(0, 0.2) == doctest::Approx(40.0));
	CHECK(animation->value_track_interpolate(0, 0.4) == doctest::Approx(80.0));
	CHECK(animation->value_track_interpolate(0, 0.5) == doctest::Approx(100.0));
	CHECK(animation->value_track_interpolate(0, 0.6) == doctest::Approx(100.0));

	CHECK(animation->track_get_key_transition(0, 0) == doctest::Approx(real_t(1.0)));
	CHECK(animation->track_get_key_transition(0, 1) == doctest::Approx(real_t(1.0)));

	ERR_PRINT_OFF;
	// Nonexistent keys.
	CHECK(animation->track_get_key_value(0, 2).is_null());
	CHECK(animation->track_get_key_value(0, -1).is_null());
	CHECK(animation->track_get_key_transition(0, 2) == doctest::Approx(real_t(-1.0)));
	// Nonexistent track (and keys).
	CHECK(animation->track_get_key_value(1, 0).is_null());
	CHECK(animation->track_get_key_value(1, 1).is_null());
	CHECK(animation->track_get_key_value(1, 2).is_null());
	CHECK(animation->track_get_key_value(1, -1).is_null());
	CHECK(animation->track_get_key_transition(1, 0) == doctest::Approx(real_t(-1.0)));

	// This is a value track, so the methods below should return errors.
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(0.0));
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] Create Bezier track") {
	Ref<Animation> animation = memnew(Animation);
	const int track_index = animation->add_track(Animation::TYPE_BEZIER);
	animation->track_set_path(track_index, NodePath("Enemy:scale"));
	animation->bezier_track_insert_key(track_index, 0.0, -1.0, Vector2(-1, -1), Vector2(1, 1));
	animation->bezier_track_insert_key(track_index, 0.5, 1.0, Vector2(0, 1), Vector2(1, 0.5));

	CHECK(animation->get_track_count() == 1);
	CHECK(!animation->track_is_compressed(0));

	CHECK(animation->bezier_track_get_key_value(0, 0) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_get_key_value(0, 1) == doctest::Approx(real_t(1.0)));

	CHECK(animation->bezier_track_interpolate(0, -0.2) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.0) == doctest::Approx(real_t(-1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.2) == doctest::Approx(real_t(-0.76057207584381)));
	CHECK(animation->bezier_track_interpolate(0, 0.4) == doctest::Approx(real_t(-0.39975279569626)));
	CHECK(animation->bezier_track_interpolate(0, 0.5) == doctest::Approx(real_t(1.0)));
	CHECK(animation->bezier_track_interpolate(0, 0.6) == doctest::Approx(real_t(1.0)));

	// This is a bezier track, so the methods below should return errors.
	ERR_PRINT_OFF;
	CHECK(animation->value_track_interpolate(0, 0.0).is_null());
	ERR_PRINT_ON;
}

TEST_CASE("[Animation] 2D property tracks retain interpolation and serialization") {
	Ref<Animation> animation = memnew(Animation);
	bool valid = false;
	animation->set("tracks/0/type", "value", &valid);
	CHECK(valid);
	CHECK(animation->track_get_type(0) == Animation::TYPE_VALUE);
	animation->track_set_path(0, NodePath("Sprite:position"));
	animation->track_insert_key(0, 0.0, Vector2(0, 10));
	animation->track_insert_key(0, 1.0, Vector2(100, 30));
	Vector2 halfway = animation->value_track_interpolate(0, 0.5);
	CHECK(halfway.is_equal_approx(Vector2(50, 20)));
	CHECK(!animation->track_is_compressed(0));
	CHECK(int(Animation::TYPE_METHOD) == 5);
	CHECK(int(Animation::TYPE_BEZIER) == 6);
	CHECK(int(Animation::TYPE_AUDIO) == 7);
	CHECK(int(Animation::TYPE_ANIMATION) == 8);
}

TEST_CASE("[Animation] Deleted track formats are rejected") {
	Ref<Animation> animation = memnew(Animation);
	for (const char *type : { "position_3d", "rotation_3d", "scale_3d", "blend_shape" }) {
		bool valid = true;
		animation->set("tracks/0/type", String(type), &valid);
		CHECK_FALSE(valid);
		CHECK(animation->get_track_count() == 0);
	}
	const int invalid_types[] = { -1, 1, 2, 3, 4, 9, 127, 255 };
	ERR_PRINT_OFF;
	for (const int type : invalid_types) {
		for (const int position : { -1, 0, 100 }) {
			CHECK_EQ(animation->add_track(static_cast<Animation::TrackType>(type), position), -1);
			CHECK_EQ(animation->get_track_count(), 0);
		}
	}
	ERR_PRINT_ON;

	// Every retained type still supports insertion and append, and invalid
	// additions must leave existing track order, paths and key data untouched.
	for (const Animation::TrackType type : { Animation::TYPE_VALUE, Animation::TYPE_METHOD,
			 Animation::TYPE_BEZIER, Animation::TYPE_AUDIO, Animation::TYPE_ANIMATION }) {
		Ref<Animation> populated = memnew(Animation);
		CHECK_EQ(populated->add_track(Animation::TYPE_VALUE), 0);
		populated->track_set_path(0, NodePath("Anchor:position"));
		populated->track_insert_key(0, 0.0, 42);
		CHECK_EQ(populated->add_track(type, 0), 0);
		CHECK_EQ(populated->track_get_type(0), type);
		ERR_PRINT_OFF;
		for (const int invalid : invalid_types) {
			for (const int position : { -1, 0, 1, 100 }) {
				CHECK_EQ(populated->add_track(static_cast<Animation::TrackType>(invalid), position), -1);
				CHECK_EQ(populated->get_track_count(), 2);
			}
		}
		ERR_PRINT_ON;
		CHECK_EQ(populated->track_get_type(0), type);
		CHECK_EQ(populated->track_get_type(1), Animation::TYPE_VALUE);
		CHECK_EQ(populated->track_get_path(1), NodePath("Anchor:position"));
		CHECK_EQ(int(populated->track_get_key_value(1, 0)), 42);
		CHECK_EQ(populated->add_track(type), 2);
		CHECK_EQ(populated->add_track(type, 100), 3);
		CHECK_EQ(populated->track_get_type(2), type);
		CHECK_EQ(populated->track_get_type(3), type);
		CHECK_EQ(populated->get_track_count(), 4);
	}
}

TEST_CASE("[Animation][FirstFixBatch] Moving tracks respects both boundaries") {
	Ref<Animation> animation = memnew(Animation);
	// Empty animations and invalid indices must be harmless.
	animation->track_move_up(-1);
	animation->track_move_up(0);
	animation->track_move_down(0);
	animation->track_move_down(1);
	CHECK_EQ(animation->get_track_count(), 0);

	animation->add_track(Animation::TYPE_VALUE);
	animation->track_set_path(0, NodePath("First:position"));
	animation->add_track(Animation::TYPE_VALUE);
	animation->track_set_path(1, NodePath("Second:position"));

	for (int invalid : { -1, 2, 100 }) {
		animation->track_move_up(invalid);
		animation->track_move_down(invalid);
	}
	animation->track_move_up(1);
	animation->track_move_down(0);
	CHECK_EQ(animation->track_get_path(0), NodePath("First:position"));
	CHECK_EQ(animation->track_get_path(1), NodePath("Second:position"));

	animation->track_move_up(0);
	CHECK_EQ(animation->track_get_path(0), NodePath("Second:position"));
	CHECK_EQ(animation->track_get_path(1), NodePath("First:position"));
	animation->track_move_down(1);
	CHECK_EQ(animation->track_get_path(0), NodePath("First:position"));
	CHECK_EQ(animation->track_get_path(1), NodePath("Second:position"));
}

} // namespace TestAnimation
