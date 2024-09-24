# Copyright 2023 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0


# Add a new HaTS test.
# ---------------------
#
# Each call to this function will produce several CTests that test HdMoonray's translation of USD to RDL.
# They can be used instead of, or in addition to, the Hydra RaTS tests, which also translate but then actually render the RDL.
#
# Each test is be named according to the following convention:
#   hats_<stage>_<test_basename>
#
#   * the <stage> token is generate|compare|copy
#
# The generate test loads the file <test_basename>.usd from the source directory and translates it to <test_basename>.rdla in the binary directory.
# The compare test compares the generated file <test_basename>.rdla to the canonical file <test_basename>.canonical.rdla in the source directory.
# Compare only passes if the generated and canonical files match. Details of any differences will be listed in the log.
# The copy test copies the generated file <test_basename>.rdla in the binary directory to the canonical <test_basename>.canonical.rdla in the source directory.
# Copy is used to create or update canonicals : new canonical are released by merging the new/changed file into main.
#
# ---------------------
function(add_hats_test test_basename)
    # test_basename:                 basename of tests. By convention includes relative folder structure, example: geometry_basis_curves

    cmake_parse_arguments(ARG "" "CAMERA" "" ${ARGN})
    if (DEFINED ARG_CAMERA)
        set(camera_opt -camera "${ARG_CAMERA}")
    endif()

    set(input_usd ${CMAKE_CURRENT_SOURCE_DIR}/${test_basename}.usd)
    set(generated_rdl ${CMAKE_CURRENT_BINARY_DIR}/${test_basename}.rdla)
    set(canonical_rdl ${CMAKE_CURRENT_SOURCE_DIR}/${test_basename}.canonical.rdla)

    set(generate_test_name hats_generate_${test_basename})
    add_test(NAME ${generate_test_name}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
             COMMAND hd_usd2rdl -in "${input_usd}" -out "${generated_rdl}" ${camera_opt}
    )
    set_tests_properties(${generate_test_name} PROPERTIES
            LABELS "generate"
    )

    set(copy_test_name hats_copy_${test_basename})
    add_test(NAME ${copy_test_name}
             COMMAND cp ${generated_rdl} ${canonical_rdl}
    )
    set_tests_properties(${copy_test_name} PROPERTIES
            LABELS "copy"
    )

    set(compare_test_name hats_compare_${test_basename})
    add_test(NAME ${compare_test_name}
             COMMAND rdl2_compare ${generated_rdl} ${canonical_rdl}
    )
    set_tests_properties(${compare_test_name} PROPERTIES
            LABELS "compare"
            DEPENDS ${generate_test_name}
    )
endfunction()
