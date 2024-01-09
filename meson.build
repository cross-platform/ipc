project('ipc', 'cpp',
    default_options: [
        'cpp_std=c++17',
        'warning_level=3',
        'werror=true'
    ]
)

if host_machine.system() == 'darwin'
    add_project_arguments(['-mmacosx-version-min=10.15', '-arch', 'arm64', '-arch', 'x86_64'], language: 'cpp')
    add_project_link_arguments(['-mmacosx-version-min=10.15', '-arch', 'arm64', '-arch', 'x86_64'], language: 'cpp')
else
    add_project_arguments(['-D_CRT_SECURE_NO_WARNINGS'], language: 'cpp')
endif

# Add formatting

format_first = []
if find_program('clang-format', required: false).found()
    if host_machine.system() == 'windows'
        format_command = meson.current_source_dir() + '/scripts/clang-format.bat'
    else
        format_command = ['bash', meson.current_source_dir() + '/scripts/clang-format.sh']
    endif

    format_first = custom_target(
        output: 'formatting',
        command: format_command,
        build_always: true
    )
endif

# Add static code analysis

if find_program('cppcheck', required: false).found()
    if host_machine.system() == 'windows'
        extra_cppcheck_args = ['-D_WIN32']
    else
        extra_cppcheck_args = []
    endif

    custom_target(
        input: format_first,
        output: 'static code analysis',
        command: [
            'cppcheck',

            '--error-exitcode=1',
            '--enable=all',
            '--inconclusive',
            '--inline-suppr',
            '--force',
            '--quiet',
            '--suppress=missingIncludeSystem',

            '-i', meson.current_build_dir(),
            '-i', meson.current_source_dir() + '/subprojects',

            '-I', meson.current_source_dir() + '/src',

            extra_cppcheck_args,

            meson.current_source_dir()
        ],
        build_always: true
    )
endif

# Configure ipc_lib

ipc_src = [
    'src/IpcClient.cpp',
    'src/IpcMessage.cpp',
    'src/IpcServer.cpp'
]

ipc_inc = include_directories(
    'src'
)

ipc_lib = static_library(
    'Ipc',
    format_first,
    ipc_src,
    include_directories: ipc_inc,
)

ipc_dep = declare_dependency(
  link_with : ipc_lib,
  include_directories : ipc_inc
)

# Add tests

subdir('tests')
