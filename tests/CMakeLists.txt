add_executable(appmenusearchscoretest
    AppMenuSearchScoreTest.cc
    ../src/AppMenuSearchScore.cc
)

target_include_directories(appmenusearchscoretest PRIVATE ../src)
target_link_libraries(appmenusearchscoretest PRIVATE Qt6::Core)

add_test(NAME appmenusearchscoretest COMMAND appmenusearchscoretest)
