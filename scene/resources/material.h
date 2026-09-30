/**************************************************************************/
/*  material.h                                                            */
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

#pragma once

#include "core/io/resource.h"
#include "core/templates/self_list.h"
#include "scene/resources/shader.h"
#include "servers/rendering/rendering_server_enums.h"

class Material : public Resource {
	GDCLASS(Material, Resource);
	RES_BASE_EXTENSION("material")
	OBJ_SAVE_TYPE(Material);

	mutable RID material;
	Ref<Material> next_pass;
	int render_priority;

	enum {
		INIT_STATE_UNINITIALIZED,
		INIT_STATE_INITIALIZING,
		INIT_STATE_READY,
	} init_state = INIT_STATE_UNINITIALIZED;

	void inspect_native_shader_code();

protected:
	_FORCE_INLINE_ void _set_material(RID p_material) const { material = p_material; }
	_FORCE_INLINE_ RID _get_material() const { return material; }
	static void _bind_methods();
	virtual bool _can_do_next_pass() const;
	virtual bool _can_use_render_priority() const;

	void _validate_property(PropertyInfo &p_property) const;

	void _mark_ready();
	void _mark_initialized(const Callable &p_add_to_dirty_list, const Callable &p_update_shader);

	GDVIRTUAL0RC_REQUIRED(RID, _get_shader_rid)
	GDVIRTUAL0RC_REQUIRED(Shader::Mode, _get_shader_mode)
	GDVIRTUAL0RC(bool, _can_do_next_pass)
	GDVIRTUAL0RC(bool, _can_use_render_priority)
public:
	enum {
		RENDER_PRIORITY_MAX = RSE::MATERIAL_RENDER_PRIORITY_MAX,
		RENDER_PRIORITY_MIN = RSE::MATERIAL_RENDER_PRIORITY_MIN,
	};

	bool _is_initialized() { return init_state == INIT_STATE_READY; }

	void set_next_pass(const Ref<Material> &p_pass);
	Ref<Material> get_next_pass() const;

	void set_render_priority(int p_priority);
	int get_render_priority() const;

	virtual RID get_rid() const override;
	virtual RID get_shader_rid() const;
	virtual Shader::Mode get_shader_mode() const;

	virtual Ref<Resource> create_placeholder() const;

	Material();
	virtual ~Material();
};

class ShaderMaterial : public Material {
	GDCLASS(ShaderMaterial, Material);
	Ref<Shader> shader;

	mutable HashMap<StringName, StringName> remap_cache;
	mutable HashMap<StringName, Variant> param_cache;
	mutable Mutex material_rid_mutex;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

	static void _bind_methods();

#ifdef TOOLS_ENABLED
	void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	virtual bool _can_do_next_pass() const override;
	virtual bool _can_use_render_priority() const override;

	void _shader_changed();
	void _check_material_rid() const;

public:
	void set_shader(const Ref<Shader> &p_shader);
	Ref<Shader> get_shader() const;

	void set_shader_parameter(const StringName &p_param, const Variant &p_value);
	Variant get_shader_parameter(const StringName &p_param) const;

	virtual Shader::Mode get_shader_mode() const override;

	virtual RID get_rid() const override;
	virtual RID get_shader_rid() const override;

	ShaderMaterial();
	~ShaderMaterial();
};

class PlaceholderMaterial : public Material {
	GDCLASS(PlaceholderMaterial, Material)
public:
	virtual RID get_shader_rid() const override { return RID(); }
	virtual Shader::Mode get_shader_mode() const override { return Shader::MODE_CANVAS_ITEM; }
};

//////////////////////
