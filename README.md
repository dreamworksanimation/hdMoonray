# hdMoonray
This repository contains the Hydra plugin (delegate) for MoonRay, HdMoonRay.
This allows Moonray to be used to render the viewer of DCC applications,
and as part of the command-line tool to render USD using Moonray.

This repository is part of the larger MoonRay/Arras codebase.  It is included as a submodule in the top-level
OpenMoonRay repository located here: [OpenMoonRay](https://github.com/dreamworksanimation/openmoonray)

## Render Settings
There are a number of switches that control the Render. In usdview these are under
"View/Hydra Settings".  In Houdini the "eye" button in the lower-right of the Viewer
brings these up, look on the first tab and select Moonray. In Maya the empty checkbox
next to Moonray in the Renderer menu does this. Both Houdini and Maya have the
advantage that you can edit the settings before running Moonray, and your settings
are saved for the next time.

To use the render farm, set the number of Hosts first, then turn on "Use Remote
Hosts". Messages from the remote renders are controlled by "Log Level", this is
independent of the "Debug" and "Info" settings.

"Restart" and "Reload Textures" are toggles as Hydra does not support any kind of
"button". Changing the value (on or off) triggers the action.

"Debug mode" is an in-process renderer. If Moonray crashes the host app will crash as
well. The Restart toggle does not work (the Houdini menu item to Restart Render does,
and in usdview you can switch to GL and back).  Currently this mode is not working
with Houdini-19 and locks up Houdini until the render finishes.

### Environment variables

You can set some environment variables to get arras processor allocations, these are not
available as settings:

    HDMOONRAY_STACK=env-dc use "dc" and "env" as args to requestArrasUrl()
    HDMOONRAY_STACK=env same as HDMOONRAY_STACK=env-gld
    HDMOONRAY_PRODUCTION=name set production name

A couple environment variables are provided to debug and recover from crashes that
happen at startup, and (especially for usdview) to get the first render to be with
the desired settings:

    HDMOONRAY_DEBUG=1 turn on debug messages (not debug mode!)
    HDMOONRAY_INFO=1 turn on info messages
    HDMOONRAY_RDLA_OUTPUT=filename dump rdla/rdlb before rendering
    HDMOONRAY_DISABLE=1 do not run the renderer
    HDMOONRAY_DEBUG_MODE=1 run debug (in-process) renderer
    HDMOONRAY_HOSTS=5 enable remote hosts and set count
    HDMOONRAY_LOGLEVEL=5 set remote logging level to N (1 is default, 5 is max)
    HDMOONRAY_DISABLE_LIGHTING=1 Turn off all the lights (and turn on the default dome light)
    HDMOONRAY_DOUBLESIDED=1 Make all geometry doublesided unless moonray:side_type=1

