/**************************************************************************/
/*  test_extension_reload.cpp                                       */
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

TEST_FORCE_LINK(test_extension_reload)

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/dir_access.h"
#include "core/object/callable_mp.h"
#include "tests/test_utils.h"

namespace TestExtensionReload {

static int reload_count = 0;
static void count_reload() {
	reload_count++;
}

struct IsolatedProject {
	String previous_path = TestProjectSettingsInternalsAccessor::resource_path();
	IsolatedProject() {
		TestProjectSettingsInternalsAccessor::resource_path() = TestUtils::get_temp_path("fourth_batch_extension_reload");
		DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton()->globalize_path(ProjectSettings::get_singleton()->get_project_data_path()));
	}
	~IsolatedProject() {
		TestProjectSettingsInternalsAccessor::resource_path() = previous_path;
	}
};

TEST_CASE("[GDExtension][FourthFixBatch] Failed extension loads do not trigger reload loops") {
	GDExtensionManager *manager = GDExtensionManager::get_singleton();
	REQUIRE(manager != nullptr);
	REQUIRE(manager->get_loaded_extensions().is_empty());
	IsolatedProject project;
	reload_count = 0;
	const Callable counter = callable_mp_static(count_reload);
	REQUIRE(manager->connect("extensions_reloaded", counter) == OK);
	HashSet<String> extensions;
	extensions.insert("res://missing_for_this_platform.gdextension");
	ERR_PRINT_OFF;
	for (int i = 0; i < 3; i++) {
		CHECK_FALSE(manager->ensure_extensions_loaded(extensions));
	}
	ERR_PRINT_ON;
	manager->disconnect("extensions_reloaded", counter);
	CHECK(reload_count == 0);
	CHECK(manager->get_loaded_extensions().is_empty());
}

} // namespace TestExtensionReload
#endif // TOOLS_ENABLED
