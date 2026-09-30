/**************************************************************************/
/*  material.cpp                                                          */
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

#include "material.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/version.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/texture.h"
#include "servers/rendering/rendering_server.h"

void Material::set_next_pass(const Ref<Material> &p_pass) {
	for (Ref<Material> pass_child = p_pass; pass_child.is_valid(); pass_child = pass_child->get_next_pass()) {
		ERR_FAIL_COND_MSG(pass_child == this, "Can't set as next_pass one of its parents to prevent crashes due to recursive loop.");
	}

	if (next_pass == p_pass) {
		return;
	}

	next_pass = p_pass;

	if (material.is_valid()) {
		RID next_pass_rid;
		if (next_pass.is_valid()) {
			next_pass_rid = next_pass->get_rid();
		}

		RS::get_singleton()->material_set_next_pass(material, next_pass_rid);
	}
}

Ref<Material> Material::get_next_pass() const {
	return next_pass;
}

void Material::set_render_priority(int p_priority) {
	ERR_FAIL_COND(p_priority < RENDER_PRIORITY_MIN);
	ERR_FAIL_COND(p_priority > RENDER_PRIORITY_MAX);

	render_priority = p_priority;

	if (material.is_valid()) {
		RS::get_singleton()->material_set_render_priority(material, p_priority);
	}
}

int Material::get_render_priority() const {
	return render_priority;
}

RID Material::get_rid() const {
	return material;
}

void Material::_validate_property(PropertyInfo &p_property) const {
	if (!_can_do_next_pass() && p_property.name == "next_pass") {
		p_property.usage = PROPERTY_USAGE_NONE;
	} else if (!_can_use_render_priority() && p_property.name == "render_priority") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void Material::_mark_ready() {
	init_state = INIT_STATE_READY;
}

void Material::_mark_initialized(const Callable &p_add_to_dirty_list, const Callable &p_update_shader) {
	// If this is happening as part of resource loading, it is not safe to queue the update
	// as an addition to the dirty list. It would be if the load is happening on the main thread,
	// but even so we'd rather perform the update directly instead of using the dirty list.
	if (ResourceLoader::is_within_load()) {
		DEV_ASSERT(init_state != INIT_STATE_READY);
		if (init_state == INIT_STATE_UNINITIALIZED) { // Prevent queueing twice.
			if (p_update_shader.is_valid()) {
				init_state = INIT_STATE_INITIALIZING;
				callable_mp(this, &Material::_mark_ready).call_deferred();
				p_update_shader.call_deferred();
			} else {
				init_state = INIT_STATE_READY;
			}
		}
	} else {
		// Straightforward conditions.
		init_state = INIT_STATE_READY;
		p_add_to_dirty_list.call();
	}
}

void Material::inspect_native_shader_code() {
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	RID shader = get_shader_rid();
	if (st && shader.is_valid()) {
		st->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, "_native_shader_source_visualizer", "_inspect_shader", shader);
	}
}

RID Material::get_shader_rid() const {
	RID ret;
	GDVIRTUAL_CALL(_get_shader_rid, ret);
	return ret;
}
Shader::Mode Material::get_shader_mode() const {
	Shader::Mode ret = Shader::MODE_MAX;
	GDVIRTUAL_CALL(_get_shader_mode, ret);
	return ret;
}

bool Material::_can_do_next_pass() const {
	bool ret = false;
	GDVIRTUAL_CALL(_can_do_next_pass, ret);
	return ret;
}

bool Material::_can_use_render_priority() const {
	bool ret = false;
	GDVIRTUAL_CALL(_can_use_render_priority, ret);
	return ret;
}

Ref<Resource> Material::create_placeholder() const {
	Ref<PlaceholderMaterial> placeholder;
	placeholder.instantiate();
	return placeholder;
}

void Material::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_next_pass", "next_pass"), &Material::set_next_pass);
	ClassDB::bind_method(D_METHOD("get_next_pass"), &Material::get_next_pass);

	ClassDB::bind_method(D_METHOD("set_render_priority", "priority"), &Material::set_render_priority);
	ClassDB::bind_method(D_METHOD("get_render_priority"), &Material::get_render_priority);

	ClassDB::bind_method(D_METHOD("inspect_native_shader_code"), &Material::inspect_native_shader_code);
	ClassDB::set_method_flags(get_class_static(), StringName("inspect_native_shader_code"), METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

	ClassDB::bind_method(D_METHOD("create_placeholder"), &Material::create_placeholder);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority", PROPERTY_HINT_RANGE, itos(RENDER_PRIORITY_MIN) + "," + itos(RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "next_pass", PROPERTY_HINT_RESOURCE_TYPE, Material::get_class_static()), "set_next_pass", "get_next_pass");

	BIND_CONSTANT(RENDER_PRIORITY_MAX);
	BIND_CONSTANT(RENDER_PRIORITY_MIN);

	GDVIRTUAL_BIND(_get_shader_rid)
	GDVIRTUAL_BIND(_get_shader_mode)
	GDVIRTUAL_BIND(_can_do_next_pass)
	GDVIRTUAL_BIND(_can_use_render_priority)
}

Material::Material() {
	render_priority = 0;
}

Material::~Material() {
	if (material.is_valid()) {
		ERR_FAIL_NULL(RenderingServer::get_singleton());
		RenderingServer::get_singleton()->free_rid(material);
	}
}

///////////////////////////////////

bool ShaderMaterial::_set(const StringName &p_name, const Variant &p_value) {
	if (shader.is_valid()) {
		const StringName *sn = remap_cache.getptr(p_name);
		if (sn) {
			set_shader_parameter(*sn, p_value);
			return true;
		}
		String s = p_name;
		if (s.begins_with("shader_parameter/")) {
			String param = s.replace_first("shader_parameter/", "");
			remap_cache[s] = param;
			set_shader_parameter(param, p_value);
			return true;
		}
#ifndef DISABLE_DEPRECATED
		// Compatibility remaps are only needed here.
		if (s.begins_with("param/")) {
			s = s.replace_first("param/", "shader_parameter/");
		} else if (s.begins_with("shader_param/")) {
			s = s.replace_first("shader_param/", "shader_parameter/");
		} else if (s.begins_with("shader_uniform/")) {
			s = s.replace_first("shader_uniform/", "shader_parameter/");
		} else {
			return false; // Not a shader parameter.
		}

		WARN_PRINT("This material (containing shader with path: '" + shader->get_path() + "') uses an old deprecated parameter names. Consider re-saving this resource (or scene which contains it) in order for it to continue working in future versions.");
		String param = s.replace_first("shader_parameter/", "");
		remap_cache[s] = param;
		set_shader_parameter(param, p_value);
		return true;
#endif
	}

	return false;
}

bool ShaderMaterial::_get(const StringName &p_name, Variant &r_ret) const {
	if (shader.is_valid()) {
		const StringName *sn = remap_cache.getptr(p_name);
		if (sn) {
			// Only return a parameter if it was previously set.
			r_ret = get_shader_parameter(*sn);
			return true;
		}
	}

	return false;
}

void ShaderMaterial::_get_property_list(List<PropertyInfo> *p_list) const {
	if (shader.is_valid()) {
		List<PropertyInfo> list;
		shader->get_shader_uniform_list(&list, true);

		HashMap<String, List<PropertyInfo>> groups;
		LocalVector<String> vgroups;

		String last_group = "<None>";

		bool is_none_group_undefined = true;
		bool is_none_group = true;

		for (const PropertyInfo &pi : list) {
			if (pi.usage == PROPERTY_USAGE_GROUP) {
				if (!pi.name.is_empty()) {
					last_group = pi.name;
					is_none_group = false;

					if (!groups.has(last_group)) {
						PropertyInfo info;
						info.usage = PROPERTY_USAGE_GROUP;
						info.name = last_group.capitalize();
						info.hint_string = "shader_parameter/";

						List<PropertyInfo> props;
						props.push_back(info);

						groups.insert(last_group, props);
						vgroups.push_back(last_group);
					}
				} else {
					last_group = "<None>";
					is_none_group = true;
				}
				continue; // Pass group.
			}

			if (is_none_group_undefined && is_none_group) {
				is_none_group_undefined = false;

				PropertyInfo info;
				info.usage = PROPERTY_USAGE_GROUP;
				info.name = "Shader Parameters";
				info.hint_string = "shader_parameter/";

				List<PropertyInfo> props;
				props.push_back(info);

				groups.insert("<None>", props);
				vgroups.push_back("<None>");
			}

			const bool is_uniform_cached = param_cache.has(pi.name);
			bool is_uniform_type_compatible = true;

			if (is_uniform_cached) {
				// Check if the uniform Variant type changed, for example vec3 to vec4.
				const Variant &cached = param_cache.get(pi.name);

				if (cached.is_array()) {
					// Allow some array conversions for backwards compatibility.
					is_uniform_type_compatible = Variant::can_convert(pi.type, cached.get_type());
				} else {
					is_uniform_type_compatible = pi.type == cached.get_type();
				}

#ifndef DISABLE_DEPRECATED
				// PackedFloat32Array -> PackedVector4Array conversion.
				if (!is_uniform_type_compatible && pi.type == Variant::PACKED_VECTOR4_ARRAY && cached.get_type() == Variant::PACKED_FLOAT32_ARRAY) {
					PackedVector4Array varray;
					PackedFloat32Array array = (PackedFloat32Array)cached;

					for (int i = 0; i + 3 < array.size(); i += 4) {
						varray.push_back(Vector4(array[i], array[i + 1], array[i + 2], array[i + 3]));
					}

					param_cache.insert(pi.name, varray);
					is_uniform_type_compatible = true;
				}
#endif

				if (is_uniform_type_compatible && pi.type == Variant::OBJECT && cached.get_type() == Variant::OBJECT) {
					// Check if the Object class (hint string) changed, for example Texture2D sampler to Texture3D.
					// Allow inheritance, Texture2D type sampler should also accept CompressedTexture2D.
					Object *cached_obj = cached;
					if (!cached_obj->is_class(pi.hint_string)) {
						is_uniform_type_compatible = false;
					}
				}
			}

			PropertyInfo info = pi;
			info.name = "shader_parameter/" + info.name;
			if (!is_uniform_cached || !is_uniform_type_compatible) {
				// Property has never been edited or its type changed, retrieve with default value.
				Variant default_value = RenderingServer::get_singleton()->shader_get_parameter_default(shader->get_rid(), pi.name);
				param_cache.insert(pi.name, default_value);
				remap_cache.insert(info.name, pi.name);
			}
			groups[last_group].push_back(info);
		}

		for (const String &group : vgroups) {
			List<PropertyInfo> &prop_infos = groups[group];
			for (const PropertyInfo &item : prop_infos) {
				p_list->push_back(item);
			}
		}
	}
}

bool ShaderMaterial::_property_can_revert(const StringName &p_name) const {
	if (shader.is_valid()) {
		if (remap_cache.has(p_name)) {
			return true;
		}
		const String sname = p_name;
		return sname == "render_priority" || sname == "next_pass";
	}
	return false;
}

bool ShaderMaterial::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (shader.is_valid()) {
		const StringName *pr = remap_cache.getptr(p_name);
		if (pr) {
			r_property = RenderingServer::get_singleton()->shader_get_parameter_default(shader->get_rid(), *pr);
			return true;
		} else if (p_name == "render_priority") {
			r_property = 0;
			return true;
		} else if (p_name == "next_pass") {
			r_property = Variant();
			return true;
		}
	}
	return false;
}

void ShaderMaterial::set_shader(const Ref<Shader> &p_shader) {
	// Only connect/disconnect the signal when running in the editor.
	// This can be a slow operation, and `notify_property_list_changed()` (which is called by `_shader_changed()`)
	// does nothing in non-editor builds anyway. See GH-34741 for details.
	if (shader.is_valid() && Engine::get_singleton()->is_editor_hint()) {
		shader->disconnect_changed(callable_mp(this, &ShaderMaterial::_shader_changed));
	}

	shader = p_shader;

	RID rid;
	if (shader.is_valid()) {
		rid = shader->get_rid();

		if (Engine::get_singleton()->is_editor_hint()) {
			shader->connect_changed(callable_mp(this, &ShaderMaterial::_shader_changed));
		}
	}

	RID material_rid = _get_material();
	if (material_rid.is_valid()) {
		RS::get_singleton()->material_set_shader(material_rid, rid);
	}

	notify_property_list_changed(); //properties for shader exposed
	emit_changed();
}

Ref<Shader> ShaderMaterial::get_shader() const {
	return shader;
}

void ShaderMaterial::set_shader_parameter(const StringName &p_param, const Variant &p_value) {
	RID material_rid = _get_material();
	if (p_value.get_type() == Variant::NIL) {
		param_cache.erase(p_param);
		if (material_rid.is_valid()) {
			RS::get_singleton()->material_set_param(material_rid, p_param, Variant());
		}
	} else {
		Variant *v = param_cache.getptr(p_param);
		if (!v) {
			// Never assigned, also update the remap cache.
			remap_cache["shader_parameter/" + p_param.operator String()] = p_param;
			param_cache.insert(p_param, p_value);
		} else {
			*v = p_value;
		}

		if (p_value.get_type() == Variant::OBJECT) {
			RID tex_rid = p_value;
			if (tex_rid == RID()) {
				param_cache.erase(p_param);

				if (material_rid.is_valid()) {
					RS::get_singleton()->material_set_param(material_rid, p_param, Variant());
				}
			} else if (material_rid.is_valid()) {
				RS::get_singleton()->material_set_param(material_rid, p_param, tex_rid);
			}
		} else if (material_rid.is_valid()) {
			RS::get_singleton()->material_set_param(material_rid, p_param, p_value);
		}
	}
}

Variant ShaderMaterial::get_shader_parameter(const StringName &p_param) const {
	if (param_cache.has(p_param)) {
		return param_cache[p_param];
	} else {
		return Variant();
	}
}

void ShaderMaterial::_shader_changed() {
	notify_property_list_changed(); //update all properties
}

void ShaderMaterial::_check_material_rid() const {
	MutexLock lock(material_rid_mutex);
	if (_get_material().is_null()) {
		RID shader_rid = shader.is_valid() ? shader->get_rid() : RID();
		RID next_pass_rid;
		if (get_next_pass().is_valid()) {
			next_pass_rid = get_next_pass()->get_rid();
		}

		_set_material(RS::get_singleton()->material_create_from_shader(next_pass_rid, get_render_priority(), shader_rid));

		for (KeyValue<StringName, Variant> param : param_cache) {
			if (param.value.get_type() == Variant::OBJECT) {
				RID tex_rid = param.value;
				if (tex_rid.is_valid()) {
					RS::get_singleton()->material_set_param(_get_material(), param.key, tex_rid);
				} else {
					RS::get_singleton()->material_set_param(_get_material(), param.key, Variant());
				}
			} else {
				RS::get_singleton()->material_set_param(_get_material(), param.key, param.value);
			}
		}
	}
}

void ShaderMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shader", "shader"), &ShaderMaterial::set_shader);
	ClassDB::bind_method(D_METHOD("get_shader"), &ShaderMaterial::get_shader);
	ClassDB::bind_method(D_METHOD("set_shader_parameter", "param", "value"), &ShaderMaterial::set_shader_parameter);
	ClassDB::bind_method(D_METHOD("get_shader_parameter", "param"), &ShaderMaterial::get_shader_parameter);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shader", PROPERTY_HINT_RESOURCE_TYPE, Shader::get_class_static()), "set_shader", "get_shader");
}

#ifdef TOOLS_ENABLED
void ShaderMaterial::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && (pf == "get_shader_parameter" || pf == "set_shader_parameter")) {
		if (shader.is_valid()) {
			List<PropertyInfo> pl;
			shader->get_shader_uniform_list(&pl);
			for (const PropertyInfo &E : pl) {
				r_options->push_back(E.name.replace_first("shader_parameter/", "").quote());
			}
		}
	}
	Material::get_argument_options(p_function, p_idx, r_options);
}
#endif

bool ShaderMaterial::_can_do_next_pass() const {
	return shader.is_valid() && shader->get_mode() == Shader::MODE_SPATIAL;
}

bool ShaderMaterial::_can_use_render_priority() const {
	return shader.is_valid() && shader->get_mode() == Shader::MODE_SPATIAL;
}

Shader::Mode ShaderMaterial::get_shader_mode() const {
	if (shader.is_valid()) {
		return shader->get_mode();
	} else {
		return Shader::MODE_SPATIAL;
	}
}

RID ShaderMaterial::get_rid() const {
	_check_material_rid();
	return Material::get_rid();
}

RID ShaderMaterial::get_shader_rid() const {
	if (shader.is_valid()) {
		return shader->get_rid();
	} else {
		return RID();
	}
}

ShaderMaterial::ShaderMaterial() {
	// Material RID will be empty until it is required.
}

ShaderMaterial::~ShaderMaterial() {
}

/////////////////////////////////
