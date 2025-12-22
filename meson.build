project(
  'psx-emu',
  'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debugoptimized',
    'warning_level=3'
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
    '-Wshadow',
    '-Wconversion',
    '-Wsign-conversion',
    '-Wundef',
    '-Wdouble-promotion',
    '-Wformat=2',
  ]
endif

add_project_arguments(cpp_args, language : 'cpp')

# Headers
inc = include_directories('include', 'external/dbg-macro')
pch = 'include/common.h'

# Sources
src_files = files(
  'src/main.cpp',
  'src/cpu.cpp'
)

# Executable
executable(
  'psx-emu',
  src_files,
  include_directories: inc,
  cpp_pch: pch,
  install: false
)
