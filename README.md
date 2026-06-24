# AeroFlip

Windows Vista/7 Flip3D Win/Alt Tabbing for Windows

### PREREQUISITES

**Requires Windows 8+**
*May work on Windows Vista/7, however never tested (And honestly not even needed)*

**Requires Visual Studio 2013**

**Install git submodules**
*after cloning, inside the repository*
`git submodule update --init`

### BUILDING

Open the solution file, and select your target architecture. Then select either Debug or Release build. Enjoy!

**Note: you MUST select exactly your architecture. If you are x64, you cannot run a x86 build, D3D9 will fail to load** 


### SCREENSHOTS

![AeroFlip Desktop Example, Activated With Alt Tab](screenshots/preview.jpg "AeroFlip Activated")
_(Running on Windows 10, using other styling tools such as [RetroBar](https://github.com/dremin/Retrobar) and [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass), these libraries are NOT included!)_

## Implementation

_AeroFlip_ uses D3D9Ex to render the virtual Flip environment, and overlays it as a topmost window. It uses `uiAccess=true` in the manifest file to ensure the application always intercepts keybinds, and overlays on top of all windows (including task manager); for this reason the installer comes with a signing script that signs the executable.
