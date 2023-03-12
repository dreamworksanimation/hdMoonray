# DWA build notes
These notes are relevant only to internal DWA builds.

[Wiki Documenation](http://mydw.dreamworks.net/display/MOONRAY/Hydra)

## Building instructions
    rez2
    rez-env buildtools
    rez-build -i --variant 0

This will build the usd-20.8 version needed by Houdini-18. To build for usd-21.8 needed by Houdini-19
and Maya, use

    rez-build -i --variant 2

### Faster rebuilding:

You must recreate the environment when any dependencies change:

    rez-build -i -s --variant 0
    build/refplat-vfx2020.3/usd_imaging-0.20.8.x.2/opt_level-optdebug/build-env
    $REZ_SCONS_CMD -s

Repeat the last line to rebuild. Type `^D` to exit out of the build environment. This environment also
allows you to run `rdl2_print` and `iv`, both of which are pretty useful.

### Debug Builds

Add 1 to the variant number to get a debug build.

    rez-build -i --variant 1

To use it, add `opt_level-debug` to the `rez-env` command. This requires debug builds of usd and moonray.

## Usage

Add `hdMoonray` to your `rez-env1` command for anything using Hydra, such as
usd_view, Houdini, or Maya.  Moonray should show up in the renderer options alongside
the GL version.

You will need some more packages to render DWA production usd files. At minimum to
get a usdview that works with DWA usd files:

    rez2
    rez-env usd_view hdMoonray usd_core_dwa_plugin usd_alembic_plugin

If Houdini-19 produces library errors and fails to start, edit the `LD_LIBRARY_PATH` and move the
houdini dsolib directory to the first location.

If you are testing a local build of hdMoonray the setup for remote Arras will not
work. A fix (but it makes Arras start a lot slower) is `rm $REZ_RXT_FILE`.

## Regression Testing

Like moonray and moonshine, hdMoonray makes use of the RaTS testing framework.
The individual tests live in the testSuite directory.  Each test typically contains
a scene file (scene.usd) and an xml file (test.xml) that specify the commands
that the test execute.  These tests usually run hd_render to produce a result.exr
file that is compared using oiiotool against an existing canonical.exr.  The tests must
be run on a `teal` class machine.

### Install the RaTS tests

The rats tests must be installed alongside the hdMoonray package. Typially this is in ~/rez
or /usr/pic1/packages.

    rez2
    rez-env buildtools
    rez-build -i -- @install @rats

If you use the build environment:

    $REZ_SCONS_CMD @install @rats

### Create a RaTS directory

RaTS must be run in an empty directory.  The test results are placed in this directory.
Do not try to run RaTS in the hdMoonray workspace.

    mkdir -p /usr/pic1/rats_run
    cd /usr/pic1/rats_run

### Setup a Rez Environment

    rez2
    rez-env hdMoonray rats usd_core_dwa_plugin python-2.7 moonshine_dwa

### Run All the Tests

    rats -a --noRetryIfDiff

### Run a Particular Test

    rats -t geometry/two_triangles --noRetryIfDiff

### Update a Canonical Image

Canonical images are kept in the path specified by the RATS_CANONICAL_PATH
environment variable.  To update a canonical use the --uic rats command.

    rats -t geometry/two_triangles --uic
