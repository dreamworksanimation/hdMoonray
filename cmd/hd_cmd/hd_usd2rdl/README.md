# hd_usd2rdl

Converts a usd file to .rdla and/or .rdlb using Hydra.

hd_usd2rdl works by performing a Hydra render using the Moonray render delegate. The code is based on hd_render.

You can specify render settings via -set <name> <value> exactly as with hd_render. However, hd_usd2rdl automatically
sets 'rdlaOuput' and 'disableRender', so setting these on the commandline will have no effect.

Hydra needs a camera to render, even though it should not affect the output. If you don't specify a camera on the
commandline, hd_usd2rdl will add a dummy camera called "/app_scene/free_cam" (exactly as hd_render does), and this
camera will be present in the RDL output file.
# Usage

hd_usd2rdl -in <usdfile> -out <name>.rdl[a|b]

If you specify an extension (.rdla or .rdlb) on the output parameter, the extension will determine the output type.
You can also specify a name with no extension : then hd_usd2rdl will generate both .rdla and .rdlb files, placing large
attributes like mesh data in the .rdlb file, everything else in the .rdla.

