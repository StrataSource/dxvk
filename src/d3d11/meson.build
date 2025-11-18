d3d11_res = wrc_generator.process('version.rc')

dxgi_common_src = [
  '../dxgi/dxgi_format.cpp',
]

d3d10_src = [
  '../d3d10/d3d10_blend.cpp',
  '../d3d10/d3d10_buffer.cpp',
  '../d3d10/d3d10_depth_stencil.cpp',
  '../d3d10/d3d10_device.cpp',
  '../d3d10/d3d10_input_layout.cpp',
  '../d3d10/d3d10_multithread.cpp',
  '../d3d10/d3d10_query.cpp',
  '../d3d10/d3d10_rasterizer.cpp',
  '../d3d10/d3d10_sampler.cpp',
  '../d3d10/d3d10_texture.cpp',
  '../d3d10/d3d10_util.cpp',
  '../d3d10/d3d10_view_dsv.cpp',
  '../d3d10/d3d10_view_rtv.cpp',
  '../d3d10/d3d10_view_srv.cpp',
]

d3d11_src = [
  'd3d11_annotation.cpp',
  'd3d11_blend.cpp',
  'd3d11_buffer.cpp',
  'd3d11_class_linkage.cpp',
  'd3d11_cmdlist.cpp',
  'd3d11_context.cpp',
  'd3d11_context_def.cpp',
  'd3d11_context_ext.cpp',
  'd3d11_context_imm.cpp',
  'd3d11_cuda.cpp',
  'd3d11_depth_stencil.cpp',
  'd3d11_device.cpp',
  'd3d11_enums.cpp',
  'd3d11_features.cpp',
  'd3d11_fence.cpp',
  'd3d11_gdi.cpp',
  'd3d11_initializer.cpp',
  'd3d11_input_layout.cpp',
  'd3d11_interop.cpp',
  'd3d11_main.cpp',
  'd3d11_on_12.cpp',
  'd3d11_options.cpp',
  'd3d11_query.cpp',
  'd3d11_rasterizer.cpp',
  'd3d11_resource.cpp',
  'd3d11_sampler.cpp',
  'd3d11_shader.cpp',
  'd3d11_state.cpp',
  'd3d11_state_object.cpp',
  'd3d11_swapchain.cpp',
  'd3d11_texture.cpp',
  'd3d11_util.cpp',
  'd3d11_video.cpp',
  'd3d11_view_dsv.cpp',
  'd3d11_view_rtv.cpp',
  'd3d11_view_srv.cpp',
  'd3d11_view_uav.cpp',
]

spvtools_src = [
  'spirv_reflect.cpp',
  'opt/assembly_grammar.cpp',
  'opt/basic_block.cpp',
  'opt/binary.cpp',
  'opt/bit_vector.cpp',
  'opt/build_module.cpp',
  'opt/cfg.cpp',
  'opt/constants.cpp',
  'opt/const_folding_rules.cpp',
  'opt/debug_info_manager.cpp',
  'opt/decoration_manager.cpp',
  'opt/def_use_manager.cpp',
  'opt/diagnostic.cpp',
  'opt/disassemble.cpp',
  'opt/dominator_analysis.cpp',
  'opt/dominator_tree.cpp',
  'opt/extensions.cpp',
  'opt/ext_inst.cpp',
  'opt/feature_manager.cpp',
  'opt/fold.cpp',
  'opt/folding_rules.cpp',
  'opt/function.cpp',
  'opt/graph.cpp',
  'opt/instruction.cpp',
  'opt/instruction_list.cpp',
  'opt/ir_context.cpp',
  'opt/ir_loader.cpp',
  'opt/liveness.cpp',
  'opt/loop_descriptor.cpp',
  'opt/module.cpp',
  'opt/name_mapper.cpp',
  'opt/opcode.cpp',
  'opt/operand.cpp',
  'opt/optimizer.cpp',
  'opt/parsed_operand.cpp',
  'opt/pass.cpp',
  'opt/pass_manager.cpp',
  'opt/print.cpp',
  'opt/register_pressure.cpp',
  'opt/scalar_analysis.cpp',
  'opt/scalar_analysis_simplification.cpp',
  'opt/spirv_endian.cpp',
  'opt/spirv_optimizer_options.cpp',
  'opt/spirv_target_env.cpp',
  'opt/spirv_validator_options.cpp',
  'opt/string_utils.cpp',
  'opt/strip_nonsemantic_info_pass.cpp',
  'opt/struct_cfg_analysis.cpp',
  'opt/table.cpp',
  'opt/table2.cpp',
  'opt/timer.cpp',
  'opt/to_string.cpp',
  'opt/types.cpp',
  'opt/type_manager.cpp',
  'opt/value_number_table.cpp',
]

d3d11_shaders = files([
  'shaders/d3d11_video_blit_frag.frag',
  'shaders/d3d11_video_blit_vert.vert',
])

d3d11_ld_args      = []
d3d11_link_depends = []

if platform == 'windows'
  d3d11_dxgi_dep = lib_dxgi
else
  d3d11_ld_args      += [ '-Wl,--version-script', join_paths(meson.current_source_dir(), 'd3d11.sym') ]
  d3d11_link_depends += files('d3d11.sym')
  d3d11_dxgi_dep = dxgi_dep
endif

d3d11_dll = shared_library(dxvk_name_prefix+'d3d11', dxgi_common_src + d3d11_src + spvtools_src + d3d10_src,
    glsl_generator.process(d3d11_shaders), d3d11_res,
  dependencies        : [ d3d11_dxgi_dep, dxbc_dep, dxvk_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d11'+def_spec_ext,
  link_args           : d3d11_ld_args,
  link_depends        : [ d3d11_link_depends ],
  kwargs              : dxvk_so_version,
)

d3d11_dep = declare_dependency(
  link_with           : [ d3d11_dll ],
  include_directories : [ dxvk_include_path ],
)

if platform != 'windows'
  pkg.generate(d3d11_dll,
    filebase: dxvk_pkg_prefix + 'd3d11',
    subdirs:  'dxvk',
  )
endif
