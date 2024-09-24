# HaTS (Hydra Acceptance Tests)

HaTS can be used instead of, or in addition to, the Hydra RaTS tests. 
Unlike the RaTs tests, HaTS tests don't actually render the USD input scene : they convert it to RDL and compare the result with a canonical conversion. They can be used to distinguish between problems in HdMoonray itself, versus problems in the Moonray or USD releases.

The HaTS tests are written using CTest, and are (in theory) separate from the HdMoonray build itself. They are built (but not run) by the main HdMoonray CMake build for convenience. What might be confusing is that they actually test whatever installed version of HdMoonray is on the current path : they don't automatically choose the version being built.

This is the process I use to run the tests in the DWA rez environment:

1. Check out, build and install the hdMoonray repo.

2. In a different shell, rez-env to the newly installed hdMoonray. Include cmake if you don't have a modern version locally.

> rez-env hdMoonray cmake

You can alter this to select a specific variant.
In the same shell, cd to the build directory generated in step one:

> cd ~/gitlocal/hdMoonray/build

cd into the hats directory of the variant you want to test:

> cd os-rocky-9/refplat-vfx2023.0/usd_imaging-0.22.5.x.3/opt_level-optdebug/python-3.10/hats

You can run ctest -N to list all tests:

> ctest -N

Test project /usr/home/rwilson/gitlocal/hdm/build/os-rocky-9/refplat-vfx2023.0/usd_imaging-0.22.5.x.3/opt_level-optdebug/python-3.10/hats
  Test  #1: hats_generate_camera_anim_focal
  Test  #2: hats_copy_camera_anim_focal
  Test  #3: hats_compare_camera_anim_focal
  Test  #4: hats_generate_camera_anim_transform
  Test  #5: hats_copy_camera_anim_transform
  Test  #6: hats_compare_camera_anim_transform
  Test  #7: hats_generate_camera_clipping
  Test  #8: hats_copy_camera_clipping
  Test  #9: hats_compare_camera_clipping
  ...

Total Tests: 129

To generate the RDL files, run all the generate tests

> ctest -L generate

Then compare with the canonicals using the compare label

> ctest -L compare

You can do both in one step with

> ctest -L "generate|compare"

Dependencies are set up to ensure the generate step runs before compare in each case.

The hats_copy tests are used to create or update the canonicals : they simply copy the generated RDL file over the current canonical (if there is one). To update the canonical(s) for everyone, merge the changed files into the main branch.