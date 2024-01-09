# Configure gtest

gtest = subproject('gtest')
gtest_dep = gtest.get_variable('gtest_dep')
gmock_dep = gtest.get_variable('gmock_dep')

# Configure tests

ipc_tests_src = [
    'main.cpp'
]

ipc_tests = executable(
    'IpcTests',
    format_first,
    ipc_tests_src,
    dependencies: [gtest_dep, gmock_dep, ipc_dep]
)

# Add code coverage

if opencppcoverage.found()
    test('IpcTests', opencppcoverage,
        args: [
            opencppcoverage_args,
            '--export_type', 'binary:IpcTests.cov',
            ipc_tests
        ]
    )
else
    test('IpcTests', ipc_tests)
endif
