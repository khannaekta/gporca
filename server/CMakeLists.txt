# Copyright (c) 2015, Pivotal Software, Inc. All Rights Reserved.

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/libgpdbcost/include"
                    "${PROJECT_SOURCE_DIR}/libgpopt/include"
                    "${PROJECT_SOURCE_DIR}/libnaucrates/include")
# for the generated config.h file.
include_directories(${PROJECT_BINARY_DIR}/libgpos/include/)
# for the auto generated minidump test classes
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/)

# Some tests use C99 long long integer constants, but the C++ dialect may not
# explicitly include C99. We supress such errors here.
check_cxx_compiler_flag("-Wno-long-long"
                        COMPILER_HAS_WNO_LONG_LONG)
if (COMPILER_HAS_WNO_LONG_LONG)
    set_source_files_properties(src/unittest/dxl/statistics/CStatisticsTest.cpp
                                src/unittest/dxl/statistics/CBucketTest.cpp
                                src/unittest/dxl/statistics/CPointTest.cpp
                                src/unittest/dxl/statistics/CHistogramTest.cpp
                                src/unittest/dxl/statistics/CMCVTest.cpp
                                src/unittest/dxl/statistics/CJoinCardinalityTest.cpp
                                src/unittest/dxl/statistics/CFilterCardinalityTest.cpp
                                src/unittest/gpopt/base/CConstraintTest.cpp
                                src/unittest/gpopt/metadata/CPartConstraintTest.cpp
                                PROPERTIES COMPILE_FLAGS "-Wno-long-long")
endif()

option(ENABLE_EXTENDED_TESTS
       "Enable extended tests for fault-injection and timing that may take a long time to run."
       OFF)

# Convenience function to add the test specified by 'TEST_NAME' to the set of
# tests to be run by CTest.
function(add_orca_test TEST_NAME)
  # Unit tests have many hard-coded paths to test data in optimizer/data. We
  # explicitly set the WORKING_DIRECTORY so that the files can be found
  # properly.
  add_test(NAME gporca_test_${TEST_NAME}
           COMMAND gporca_test -U ${TEST_NAME}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# minidump tests must follow rule:
# "CxxxTest: file1 file2; CxxxTest: file3 file4"
# each test case should end with ";", EXCEPT LAST ONE.
# comments with format /*...*/ are allowed at any place.


set(mdp_dir "../data/dxl/minidump/")
set(mdp_incl_dir "unittest/gpopt/minidump")
set(mdp_test_hdr_tpl "${CMAKE_CURRENT_SOURCE_DIR}/include/${mdp_incl_dir}/MinidumpTest.h.in")
set(mdp_test_src_tpl "${CMAKE_CURRENT_SOURCE_DIR}/src/${mdp_incl_dir}/MinidumpTest.cpp.in")
set(mdp_test_hdr_dir "${CMAKE_CURRENT_BINARY_DIR}/include/${mdp_incl_dir}")
set(mdp_test_src_dir "${CMAKE_CURRENT_BINARY_DIR}/src/${mdp_incl_dir}")
set(mdp_headers "${mdp_test_hdr_dir}/MinidumpTestHeaders")
set(mdp_tests "${mdp_test_hdr_dir}/MinidumpTestArray")
set(mdp_headers_h "${mdp_headers}.h")
set(mdp_tests_inl "${mdp_tests}.inl")

# if the old file exists and the new file is the same with old file,
# do nothing. otherwise, write the old file with new file's content.
function(update_file old_file new_file)
  if(NOT EXISTS ${old_file})
    file(RENAME ${new_file} ${old_file})
  else()
    file(READ ${old_file} old_hex HEX)
    file(READ ${new_file} new_hex HEX)
    if("${old_hex}" STREQUAL "${new_hex}")
      file(REMOVE ${new_file})
    else()
      file(RENAME ${new_file} ${old_file})
    endif()
  endif()
endfunction()

file(WRITE ${mdp_headers} "")
file(WRITE ${mdp_tests} "")

foreach(mdp_group IN LISTS MDP_GROUPS)

  # remove comments /*...*/ from minidump test group string
  string(REGEX REPLACE "\\/\\*.*\\*\\/" "" mdp_group ${mdp_group})

  string(STRIP ${mdp_group} mdp_group)

  # get minidump test name
  string(REGEX MATCH "C.+Test" test_name ${mdp_group})

  # get the length of "CxxxTest:"
  string(LENGTH "${test_name}:" name_len)

  # get minidump file names (without .mdp) concatenated by spaces
  string(SUBSTRING ${mdp_group} ${name_len} -1 mdp_names)

  string(STRIP ${mdp_names} mdp_names)

  # generate full minidump paths array
  string(REGEX REPLACE "([A-Za-z0-9_\-]+)" "\"${mdp_dir}\\1.mdp\"," mdp_names ${mdp_names})
  string(REGEX REPLACE "[ \t\n]+" "\n" mdp_files ${mdp_names})

  # generate header/src file name for current test
  set(header_file ${mdp_test_hdr_dir}/${test_name})
  set(source_file ${mdp_test_src_dir}/${test_name})

  # auto generate test class from template
  configure_file(${mdp_test_hdr_tpl} ${header_file})
  configure_file(${mdp_test_src_tpl} ${source_file})

  update_file(${header_file}.h ${header_file})
  update_file(${source_file}.cpp ${source_file})

  # add test header file into include file used in main.cpp
  file(APPEND ${mdp_headers} "#include \"${mdp_incl_dir}/${test_name}.h\"\n")

  # add unittest into test array in main.cpp
  file(APPEND ${mdp_tests} "GPOS_UNITTEST_STD(${test_name}),\n")

  add_orca_test(${test_name})
endforeach()

update_file(${mdp_headers_h} ${mdp_headers})
update_file(${mdp_tests_inl} ${mdp_tests})

# The ordering of tests and the conditions by which tests are enabled or
# disabled matches the static array of tests in "src/startup/main.cpp".

# Naucrates tests.

file(GLOB_RECURSE hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/include/*.inl
                       ${mdp_test_hdr_dir}/*.h
                       ${mdp_test_hdr_dir}/*.inl)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
                       ${mdp_test_src_dir}/*.cpp)

# Add headers to make them visible in some IDEs (Clion, VS, Xcode)
list(APPEND srcs ${hdrs})

add_executable(gporca_test ${srcs})

target_link_libraries(gporca_test
                      gpdbcost
                      gpopt
                      naucrates
                      gpos)
