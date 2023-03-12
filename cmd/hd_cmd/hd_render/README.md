# hd_render
Renders a usd scene using hydra.

# Usage
## Setup your environment to include hdMoonray and any other renderers you may want to use (Embree in this case)
    rez2
    rez-env hdMoonray usd_embree_plugin

## To see all options
    hd_render -h

## To render a usd file using Moonray
    hd_render -in file.usd -out file.exr

## To render a usd file using an alternate renderer
    hd_render -renderer Embree -in file.usd -out file.exr

## To see a list of available renderers
    hd_render -renderer

## To set a renderer setting
    hd_render -renderer Embree -set "Samples To Convergence" 16 -in file.usd -out file.exr

## To see a list of all render settings for a particular renderer
    hd_render -renderer Embree -set

## To render a particular aov (normal in this example)
    hd_render -aov normal -in file.usd -out file.exr
