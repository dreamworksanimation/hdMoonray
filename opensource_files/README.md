Some configuration files currently have to be different between internal DWA releases and the OS release:

- The session files have to specify 'current-environment' rather than a rez package list, since the OS release does not use Rez

- The pluginInfo.json files are different, because of differences in the release structure.

The affected files are installed from this directory by the CMake build.
