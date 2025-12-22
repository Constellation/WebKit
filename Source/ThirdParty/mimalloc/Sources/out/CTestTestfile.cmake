# CMake generated Testfile for 
# Source directory: /Users/yusukesuzuki/dev/mimalloc
# Build directory: /Users/yusukesuzuki/dev/mimalloc/out
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "/Users/yusukesuzuki/dev/mimalloc/out/mimalloc-test-api")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;723;add_test;/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;0;")
add_test(test-api-fill "/Users/yusukesuzuki/dev/mimalloc/out/mimalloc-test-api-fill")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;723;add_test;/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;0;")
add_test(test-stress "/Users/yusukesuzuki/dev/mimalloc/out/mimalloc-test-stress")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;723;add_test;/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "/opt/homebrew/bin/cmake" "-E" "env" "MIMALLOC_VERBOSE=1" "DYLD_INSERT_LIBRARIES=/Users/yusukesuzuki/dev/mimalloc/out/libmimalloc.2.2.dylib" "/Users/yusukesuzuki/dev/mimalloc/out/mimalloc-test-stress-dynamic")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;742;add_test;/Users/yusukesuzuki/dev/mimalloc/CMakeLists.txt;0;")
