find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

qt_add_executable(test_scaffold
    test_scaffold.cpp
)
target_link_libraries(test_scaffold PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_scaffold COMMAND test_scaffold)
