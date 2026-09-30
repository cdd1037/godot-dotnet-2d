/**************************************************************************/
/*  material_editor_plugin.cpp                                            */
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

#include "material_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/blit_material.h"
#include "scene/resources/canvas_item_material.h"
#include "scene/resources/particle_process_material.h"
#include "servers/rendering/rendering_server.h"

// 3D.

Ref<ShaderMaterial> MaterialEditor::make_shader_material(const Ref<Material> &p_from, bool p_copy_params) {
	ERR_FAIL_COND_V(p_from.is_null(), Ref<ShaderMaterial>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(p_from->get_shader_rid());
	shader->set_code(code);
	smat->set_shader(shader);

	if (p_copy_params) {
		List<PropertyInfo> params;
		RS::get_singleton()->get_shader_parameter_list(p_from->get_shader_rid(), &params);

		for (const PropertyInfo &E : params) {
			Variant value = RS::get_singleton()->material_get_param(p_from->get_rid(), E.name);
			smat->set_shader_parameter(E.name, value);
		}
	}

	smat->set_render_priority(p_from->get_render_priority());
	smat->set_local_to_scene(p_from->is_local_to_scene());
	smat->set_name(p_from->get_name());
	return smat;
}

void MaterialEditor::edit(Ref<Material> p_material) {
	rect_instance->set_material(p_material);
	rect_instance->set_visible(p_material.is_valid() && p_material->get_shader_mode() == Shader::MODE_CANVAS_ITEM);
}

MaterialEditor::MaterialEditor() {
	set_custom_minimum_size(Size2(160, 160) * EDSCALE);
	rect_instance = memnew(ColorRect);
	rect_instance->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	rect_instance->set_color(Color(1, 1, 1, 1));
	add_child(rect_instance);
	rect_instance->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
}

bool EditorInspectorPluginMaterial::can_handle(Object *p_object) {
	Material *material = Object::cast_to<Material>(p_object);
	if (!material) {
		return false;
	}
	Shader::Mode mode = material->get_shader_mode();
	return mode == Shader::MODE_CANVAS_ITEM;
}

void EditorInspectorPluginMaterial::parse_begin(Object *p_object) {
	Material *material = Object::cast_to<Material>(p_object);
	if (!material) {
		return;
	}
	Ref<Material> m(material);

	MaterialEditor *editor = memnew(MaterialEditor);
	editor->edit(m);
	add_custom_control(editor);
}

MaterialEditorPlugin::MaterialEditorPlugin() {
	Ref<EditorInspectorPluginMaterial> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

String ParticleProcessMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool ParticleProcessMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<ParticleProcessMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> ParticleProcessMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	return MaterialEditor::make_shader_material(p_resource);
}

String CanvasItemMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool CanvasItemMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<CanvasItemMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> CanvasItemMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	ERR_FAIL_COND_V(!Object::cast_to<CanvasItemMaterial>(*p_resource) || !Object::cast_to<CanvasItemMaterial>(*p_resource)->_is_initialized(), Ref<CanvasItemMaterial>());
	return MaterialEditor::make_shader_material(p_resource);
}

String BlitMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool BlitMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<BlitMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> BlitMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<BlitMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}
