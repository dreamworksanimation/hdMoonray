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
    _version = '3.7'
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
    build_system_pbr = 'cmake_modules'

variants = [
    ['os-CentOS-7', 'refplat-vfx2020.3', 'usd_imaging-0.20.8.x.2', 'opt_level-optdebug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2020.3', 'usd_imaging-0.20.8.x.2', 'opt_level-debug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-optdebug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-debug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.22.5.x.2', 'opt_level-optdebug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.22.5.x.2', 'opt_level-debug', 'python-3.7'],
    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.2', 'opt_level-optdebug', 'python-3.9'],
    ['os-CentOS-7', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.2', 'opt_level-debug', 'python-3.9'],
    ['os-CentOS-7', 'refplat-vfx2020.3', 'usd_imaging-0.20.8.x.2', 'opt_level-optdebug', 'python-2.7'],
    ['os-CentOS-7', 'refplat-vfx2020.3', 'usd_imaging-0.20.8.x.2', 'opt_level-debug', 'python-2.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-optdebug', 'python-2.7'],
    ['os-CentOS-7', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-debug', 'python-2.7'],
    ['os-rocky-9', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-optdebug', 'python-3.7'],
    ['os-rocky-9', 'refplat-vfx2021.0', 'usd_imaging-0.21.8.x.2', 'opt_level-debug', 'python-3.7'],
    ['os-rocky-9', 'refplat-vfx2021.0', 'usd_imaging-0.22.5.x.3', 'opt_level-optdebug', 'python-3.7'],
    ['os-rocky-9', 'refplat-vfx2021.0', 'usd_imaging-0.22.5.x.3', 'opt_level-debug', 'python-3.7'],
    ['os-rocky-9', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.3', 'opt_level-optdebug', 'python-3.9'],
    ['os-rocky-9', 'refplat-vfx2022.0', 'usd_imaging-0.22.5.x.3', 'opt_level-debug', 'python-3.9'],
]

sconsTargets = {
    'refplat-vfx2020.3': ['@install'] + unittestflags + ratsflags,
    'refplat-vfx2021.0': ['@install'] + unittestflags + ratsflags,
    'refplat-vfx2022.0': ['@install'] + unittestflags + ratsflags,
}

requires = [
    'moonray-14.7',
    'moonshine_dwa-11.7',
    'moonshine-11.7',
    'mcrt_computation-12.7',
    'arras4_core-4.10',
    'mcrt_messages-11.3',
    'mcrt_dataio-12.7',
    'mkl',
    'openimageio-2.3.20.0.x',
]

private_build_requires = [
    build_system_pbr,
    'gcc-6.3.x|9.3.x',
    'cppunit'
]

tests = {
    # "rats-debug": {
    #     "command": "rats -a --rco=2 --nohtml --rac --var res 14 --ofwc hd_render",
    #     "requires": ["rats", "opt_level-debug", "usd_core_dwa_plugin"]
    #     },
    "rats-opt-debug": {
        "command": "rats -a --rco=2 --nohtml --rac --maxConcurrentTests=10",
        "requires": ["rats", "opt_level-optdebug", "usd_core_dwa_plugin", "usd_imaging", "moonshine_dwa", "houdini_dwa-19", "python-2.7", "xml-2.9"]
        }
    }

def commands():
    prependenv('PXR_PLUGIN_PATH', '{root}/plugin')
    prependenv('PXR_PLUGIN_PATH', '{root}/adapters')
    prependenv('PATH', '{root}/bin')
    prependenv('ARRAS_SESSION_PATH', '{root}/sessions')
    prependenv('HOUDINI_PATH', '{root}/houdini')
    prependenv('HDMOONRAY_DOUBLESIDED', '1')
    setenv('RATS_CANONICAL_PATH', '/work/rd/raas/hydra/rats/canonicals/')
    setenv('RATS_TESTSUITE_PATH', '{root}/testSuite')

config_version = 0
