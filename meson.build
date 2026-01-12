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
  cpp_args += [
    '/W4',
    '/GL'
  ]
else
  cpp_args += [
    '-Wpedantic',
    '-Wconversion',
    '-Wsign-conversion',
    '-Wundef',
    '-Wdouble-promotion',
    '-Wformat=2',
    '-Wno-implicit-int-conversion', # Clang is overzealous
    '-O3',
    '-ffast-math',
    '-funroll-loops',
    '-fomit-frame-pointer' # Free up a register
  ]

  # GCC-specific optimisations
  if cc.get_id() == 'gcc'
    cpp_args += [
      '-ftree-vectorize', # Auto-vectorization
      '-fno-semantic-interposition' # Better inlining
    ]
  endif

  # Clang-specific optimisations
  if cc.get_id() == 'clang'
    cpp_args += [
      '-fvectorize',
      '-fslp-vectorize'
    ]
  endif
endif

add_project_arguments(cpp_args, language : 'cpp')

# Link-time optimization flags
link_args = []
if cc.get_id() != 'msvc'
  link_args += ['-flto']
endif

# Dependencies
sdl3_dep = dependency('sdl3', required: true)
libcue_dep = dependency('libcue', required: true)

# Headers
inc = include_directories('include', 'external/dbg-macro')
pch = 'include/common.h'

# Sources
src_files = files(
  'src/main.cpp',
  'src/cpu.cpp',
  'src/bus.cpp',
  'src/gpu.cpp',
  'src/gte.cpp',
  'src/drive.cpp',
  'src/cue.cpp',
  'src/io.cpp',
  'src/trace.cpp',
  'src/renderer.cpp'
)

# Executable
executable(
  'psx-emu',
  src_files,
  include_directories: inc,
  cpp_pch: pch,
  install: false,
  dependencies: [sdl3_dep, libcue_dep],
  link_args: link_args
)