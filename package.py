# Copyright 2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

# -*- coding: utf-8 -*-
import os
import sys

unittestflags = (['@run_all', '--unittest-xml']
                 if os.environ.get('BROKEN_CUSTOM_ARGS_UNITTESTS') else [])
ratsflags = (['@rats'] if os.environ.get('BROKEN_CUSTOM_ARGS_RATS') else [])

name = 'hdMoonray'

if 'early' not in locals() or not callable(early):
    def early(): return lambda x: x

@early()
def version():
    _version = '6.16'
    from rezbuild import earlybind
    return earlybind.version(this, _version)

description = "Hydra delegate for Moonray"

authors = [
    'DreamWorks Animation PSW - Hydra Moonray Team',
    'moonbase-dev@dreamworks.com',
    'hd-moonray@dreamworks.com'
]

help = ('For assistance, '
        "please contact the folio's owner at: moonbase-dev@dreamworks.com")

if 'scons' in sys.argv:
    build_system = 'scons'
    build_system_pbr = 'bart_scons-10'
else:
    build_system = 'cmake'
    build_system_pbr = 'cmake_modules-1.0'

variants = [
    ['os-rocky-9', 'refplat-vfx2023.1', 'usd_imaging-0.22.5.x.4', 'openimageio-2.3.20.0.x', 'opt_level-optdebug', 'python-3.10'],
    ['os-rocky-9', 'refplat-vfx2023.1', 'usd_imaging-0.22.5.x.4', 'openimageio-2.3.20.0.x', 'opt_level-debug',    'python-3.10'],
    ['os-rocky-9', 'refplat-vfx2023.1', 'usd_imaging-0.23.8.x',   'openimageio-2.4.8.0.x',  'opt_level-optdebug', 'python-3.10'],
    ['os-rocky-9', 'refplat-vfx2023.1', 'usd_imaging-0.24.3.x',   'openimageio-2.4.8.0.x',  'opt_level-optdebug', 'python-3.10'],
    ['os-rocky-9', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.4', 'openimageio-2.3.20.0.x', 'opt_level-optdebug', 'python-3.9'],
    ['os-rocky-9', 'refplat-vfx2024.0', 'usd_imaging-0.24.3.x',   'openimageio-2.4.8.0.x',  'opt_level-optdebug', 'python-3.11'],

    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.3', 'openimageio-2.3.20.0.x', 'opt_level-optdebug', 'python-3.9'],
    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.3', 'openimageio-2.3.20.0.x', 'opt_level-debug',    'python-3.9'],
    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.4', 'openimageio-2.3.20.0.x', 'opt_level-optdebug', 'python-3.9'],
    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.4', 'openimageio-2.3.20.0.x', 'opt_level-debug',    'python-3.9'],
]
conf_CI_variants = variants

sconsTargets = {
    'refplat-vfx2022.0': ['@install'] + unittestflags + ratsflags,
    'refplat-vfx2023.0': ['@install'] + unittestflags + ratsflags,
}

requires = [
    'moonray-17.14',
    'moonshine_dwa-14.16',
    'moonshine-14.15',
    'mcrt_computation-15.14',
    'arras4_core-4.10',
    'mcrt_messages-14.2',
    'mcrt_dataio-15.7',
    'mkl'
]

private_build_requires = [
    build_system_pbr,
    'gcc-6.3.x|9.3.x|11.x',
    'cppunit'
]

tests = {
    # "rats-debug": {
    #     "command": "rats -a --rco=2 --nohtml --rac --var res 14 --ofwc hd_render",
    #     "requires": ["rats", "opt_level-debug", "usd_core_dwa_plugin"]
    #     },
    "rats-opt-debug": {
        "command": "rats -a --rco=2 --nohtml --rac --maxConcurrentTests=10",
        "requires": ["rats", "opt_level-optdebug", "usd_core_dwa_plugin", "moonshine_usd", "usd_imaging-0.22.5", "moonshine_dwa", "houdini_dwa-19", "python-3.9", "gcc", "refplat-vfx2022"]
        }
    }

def commands():
    prependenv('PXR_PLUGINPATH_NAME', '{root}/plugin/pxr') # usd_core-0.23.5 and later
    prependenv('PXR_PLUGIN_PATH', '{root}/plugin/pxr') # usd_core-0.23.2 and earlier
    prependenv('PATH', '{root}/bin')
    prependenv('LD_LIBRARY_PATH', '{root}/lib64')
    prependenv('ARRAS_SESSION_PATH', '{root}/sessions/dwa')
    prependenv('HOUDINI_PATH', '{root}/plugin/houdini')
    prependenv('HDMOONRAY_DOUBLESIDED', '1')
    setenv('RATS_CANONICAL_PATH', '/work/rd/raas/hydra/rats/canonicals/')
    setenv('RATS_TESTSUITE_PATH', '{root}/testSuite')

config_version = 0
