# xzx VST  (VST3 multi-effect for FL Studio)

Signal chain:
Input -> Bitcrusher -> Drive (Soft/Hard/Fold/Tube/Diode + Bias + Tone) -> 3-band EQ
-> Filter (LP/HP/BP) with LFO + envelope follower -> Tremolo/Autopan
-> Delay (Stereo / Ping-Pong, damping, spread) -> Chorus/Flanger -> Reverb
-> Width -> Dry/Wet -> Output.

Every section has an on/off switch. 50+ automatable parameters, 17 factory presets,
user presets, RANDOMIZE button, live spectrum analyzer, 9 colour themes (incl. animated "Color Cycle").

---------------------------------------------------------------------------
## EASIEST: let GitHub build it for you (nothing to install)
1. Make a free account at github.com and create a new repository (private is fine).
2. Upload everything in this folder (drag & drop works - make sure the hidden
   `.github` folder goes up too; or use GitHub Desktop).
3. Open the repo's **Actions** tab -> "Build xzx VST (Windows)" -> wait ~10 min.
4. Click the finished run -> download **xzx-VST-windows-vst3** at the bottom.
5. Unzip, copy the folder `xzx VST.vst3` into `C:\Program Files\Common Files\VST3\`
6. FL Studio: Options > Manage plugins > Find installed plugins.

## OR build locally (Windows)
Install Visual Studio 2022 ("Desktop development with C++") + CMake, then
double-click **build_windows.bat** (right-click > Run as administrator to auto-install).

---------------------------------------------------------------------------
## Customize
- Plugin name/codes: CMakeLists.txt
- Parameters:       createLayout()  in Source/PluginProcessor.cpp
- Factory presets:  factoryPresets() in Source/PluginProcessor.cpp
- DSP:              processBlock()
- Look & colours:   Source/PluginEditor.h (XZXLook) and PluginEditor.cpp
- User presets live in %APPDATA%\XZX\Presets

JUCE is used under its own licence (free for personal/small use, see juce.com/juce-8-licence).
