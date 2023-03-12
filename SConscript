Import('env')

env.Tool('component')
env.Tool('dwa_install')
env.Tool('dwa_run_test')
env.Tool('dwa_utils')
env.Tool('python_sdk')
#env.Tool('dwa_class_proto')
#env.Tool('dwa_codecov')

import os
import SCons.Script
import sys

class ShutUp(object):
    class devnull():
        def write(*args): pass
    def __enter__(self):
        sys.stdout.flush(); sys.stderr.flush()
        self.old_stdout, self.old_stderr = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = self.devnull()
    def __exit__(self, exc_type, exc_value, traceback):
        sys.stdout = self.old_stdout
        sys.stderr = self.old_stderr

from dwa_sdk import DWALoadSDKs
with ShutUp(): DWALoadSDKs(env)

def addFlag(flag):
    env.AppendUnique(CXXFLAGS=[flag])

def removeFlag(flag):
    for comp in ['CXXFLAGS', 'CCFLAGS', 'CFLAGS']:
        if flag in env[comp]:
            env[comp].remove(flag)

# Stolen from moonshine's SConscript
# Install the RaTS test suite data to the package dir so that RaTS can be run.
def CopyTestSuite(self, testSuiteRoot='#testSuite'):
    '''
    Copy the test suite test data to the moonshine package so that it can be
    used to run the RaTS test suite.
    '''
    ratstarget = '@rats' in SCons.Script.COMMAND_LINE_TARGETS
    source_dir = self.Dir(testSuiteRoot).srcnode().abspath
    for dir_path, dirs, files in os.walk(source_dir):
        rel_path = dir_path.replace(source_dir, '')
        if len(rel_path) > 0 and rel_path[0] == '/':
            rel_path = rel_path[1:]
        targets = self.Flatten([
            self.InstallAs(target=os.path.join('$INSTALL_DIR', 'testSuite',
                                               rel_path, f),
                           source=os.path.join(dir_path, f))
            for f in files])
        self.DWAAlias('@rats', targets)
        self.NoCache(targets)
        if ratstarget:
            self.DWAAlias('@install', targets)

CopyTestSuite(env)

# Suppress depication warning from tbb-2020.
env.AppendUnique(CPPDEFINES='TBB_SUPPRESS_DEPRECATED_MESSAGES')

# Minimal optimization level when debugging
if env['TYPE_LABEL'] == 'debug': addFlag('-O0')

if 'icc' in env['CC']: # Settings for ICC compiler

    # All warnings are errors:
    addFlag('-Werror-all')

    # Disable warnings about deprecated api for hash_set which is used by usd
    addFlag('-Wno-deprecated')

    # Disable warning #1 (new line at the end of file)
    addFlag('-wd1')

    # Do not error if "constexpr static" is used instead of "static constexpr"
    # Triggered by usd headers
    addFlag('-wd82')

    # unreachable code (triggerd by tfDebug)
    addFlag('-wd111')

    # Disable warning #177 (variable declared but not referenced)
    # GCC does not produce this warning.
    # Disabled to allow prototype code to assign calculated (but not yet used)
    # values to local variables. Probably should be re-enabled for production code
    addFlag('-wd177')

    # Disable warning #304 (access control not specified ("private" by default))
    addFlag('-wd304')

    # Disable "class defines no constructor to initialize the following" (for pxr::VtArray)
    addFlag('-wd411')

    # Disable warning #444 (base class with non-virtual dtor)
    addFlag('-wd444')

    # Don't error on "redeclared "inline" after being called"
    addFlag('-wd522')

    # Disable warning #869 (parameter "foo" was never referenced)
    addFlag('-wd869')

    # Disable warning #1572 (float equality/inequality comparisons)
    addFlag('-wd1572')

    # Disable warning #1599 (declaration hides variable)
    #addFlag('-wd1599')

    # Disable warning #1684 (conversion from pointer to same-sized integral type)
    # results in build errors even for reinterpret_cast
    addFlag('-wd1684')

    # Do not error on writes to static vars
    #removeFlag('-we1711') # is this a typo?
    addFlag('-wd1711')

    # Disable warning #1875 (offsetof applied to non-POD types is nonstandard)
    #addFlag('-wd1875')

    # Disable warning #2203: cast discards qualifiers from target type
    addFlag('-wd2203')

    # Disable warning #2282 (unrecognized GCC pragma)
    addFlag('-wd2282')

    # Disable warning #3280 (declaration hides member)
    # Same as GCC's -Wshadow option, this is too strict and errors on
    # perfectly legal C++ idioms (e.g. getter/setter named after member
    # variable)
    addFlag('-wd3280')

    # Disable "nonstandard use of "auto"" (for "auto * foo = ..." in pxr::VtArray)
    addFlag('-wd3373')

    # Disable warning #3532 (comparison between two different enum types)
    #addFlag('-wd3532')

else:
    # Settings for non-ICC compilers (assumed to be GCC)

    # Extra debugging information, specific to GCC/GDB and only in debug mode:
    if env['TYPE_LABEL'] == 'debug':
        addFlag('-ggdb3')

    # Treat warnings as errors
    addFlag('-Werror')

    # Enable lots of warnings
    addFlag('-Wall')
    addFlag('-Wextra')
    #addFlag('-Wuninitialized') # turned on by -Wall
    #addFlag('-Wmaybe-uninitialized') # turned on by -Wall
    #addFlag('-Wunused-but-set-variable') # turned on by -Wall
    #addFlag('-Wunused-label') # turned on by -Wall
    #addFlag('-Wunused-local-typedefs') # turned on by -Wall
    #addFlag('-Wunused-variable') # turned on by -Wall
    #addFlag('-Wunused-value') # turned on by -Wall
    #addFlag('-Wempty-body') # turned on by -Wextra

    # Turn off warning(error) for:
    #addFlag('-Wno-error=conversion') # Usd triggers a lot of these.#
    #addFlag('-Wno-conversion') # Usd triggers a lot of these.
    #addFlag('-Wno-float-conversion')
    addFlag('-Wno-deprecated') # Usd uses hash_set
    addFlag('-Wno-missing-field-initializers') # pxr::Vt_ShapeData::otherDims
    addFlag('-Wno-unknown-pragmas') # "warning" pragma from moonray AttributeKey.h
    addFlag('-Wno-class-memaccess') # W: with no trivial copy-assignment
    addFlag('-Wno-deprecated-copy')
    #addFlag('-fpermissive')
    removeFlag('-Wconversion') # lots in Usd and our code
    removeFlag('-Wcast-qual') # Usd weak ptr triggers this, unfortunately
    #removeFlag('-Wfloat-equal')

env.DWASConscriptWalk('#src', ignore=[])
env.DWASConscriptWalk('#cmd', ignore=[])
env.DWASConscriptWalk('#sessions', ignore=[])
env.DWASConscriptWalk('#houdini', ignore=[])

env.Default(env.Alias('@install'))

#env.DWAFillInMissingInitPy()
env.DWAResolveUndefinedComponents()
env.DWAFreezeComponents()
