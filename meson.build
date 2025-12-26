project(
  'psx-emu',
  'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debugoptimized',
    'warning_level=3',
    'b_lto=true'
  ]
)

cc = meson.get_compiler('cpp')

# Args
cpp_args = []

if cc.get_id() == 'msvc'
  cpp_args += [ '/W4' ]
else
  cpp_args += [
    '-Wpedantic',
    '-Wconversion',
    '-Wsign-conversion',
    '-Wundef',
    '-Wdouble-promotion',
    '-Wformat=2',
    '-march=native',
    '-mtune=native',
    '-O3'
  ]
endif

add_project_arguments(cpp_args, language : 'cpp')

# Dependencies
sdl3_dep = dependency('sdl3', required: true)

# Headers
inc = include_directories('include', 'external/dbg-macro')
pch = 'include/common.h'

# Sources
src_files = files(
  'src/main.cpp',
  'src/cpu.cpp',
  'src/bus.cpp',
  'src/gpu.cpp',
  'src/trace.cpp',
  'src/renderer.cpp',
)

# Executable
executable(
  'psx-emu',
  src_files,
  include_directories: inc,
  cpp_pch: pch,
  install: false,
  dependencies: [sdl3_dep]
)
