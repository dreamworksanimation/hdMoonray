# hdMoonray RaTS Tests

These instructions are intended for DWA's internal MoonRay developers and will likely
not work outside of the DWA studio environment.

This directory contains the RaTS tests for hdMoonRay.  The RaTS tests are not in a
separate package like the regular MoonRay RaTS tests.

:warning: The RaTS tests must currently be run on a teal-class machine (e.g. tealsnip) using CentOS 7 for the
canonicals to match correctly.

If you want to run hdMoonRay RaTS against a local MoonRay build, make sure you
first build MoonRay (and possibly scene_rdl2) as variant 3, e.g.:

    rez-build -i --variants 3
    # os-CentOS-7 opt_level-optdebug refplat-vfx2022.0 gcc-9.3.x.1

Also be sure to set REZ_PACKAGES_PATH to the location of your local moonray build
so the hdmoonray build picks it up.

### Building

Build hdMoonRay variant 6, e.g.:

    rez2
    rez-env buildtools
    cd hdmoonray
    rez-build -i --variants 6
    # os-CentOS-7 opt_level-optdebug usd_core_dwa_plugin moonshine_usd usd_imaging-0.22.5 moonshine_dwa houdini_dwa-19 python-3.9 gcc refplat-vfx2022

### Running

In a new shell:

Ensure your REZ_PACKAGES_PATH is set to pick up the local moonray build, if there
is one.

To run the RaTS tests, use the following rez-env:

    rez2
    rez-env hdMoonray rats opt_level-optdebug usd_core_dwa_plugin moonshine_usd usd_imaging-0.22.5 moonshine_dwa houdini_dwa-19 python-3.9 gcc refplat-vfx2022

Then, use the normal RaTS commands **from the hdmoonray rez install dir**, e.g.:

    rats -a -c vectorized

:warning: Currently only the vectorized tests have updated canonicals, scalar is not currently supported

### Running with rez-test

Alternatively, you can run rez-test from the top-level hdMoonray source directory.

It can be run in the build environment or in a different shell that's not in a rez
environment.  rez-test figures out the environment when it runs and will automatically
discover and use locally-built moonray packages.

    rez-test hdMoonray rats-opt-debug

### Finishing up

Ensure there are no zombie `hdmoonray` processes by running 'top'.
These can happen if the rats tests were interrupted (e.g. CTRL-C).  Terminate them with
'kill'.
 
