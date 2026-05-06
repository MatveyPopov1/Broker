add_executable(run_tests
    main_test.cpp
    test_storage.cpp
    test_index.cpp
    test_subscriptions.cpp
    test_user.cpp
    test_queue.cpp
    test_offset.cpp
    ../index/index.cpp
    ../storage/storage.cpp
)

target_include_directories(run_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(run_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)
