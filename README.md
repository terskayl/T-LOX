# T-LOX

### Setup

This Unreal project depends on a static library that needs to be built. 

Open `T-LOX/tlox` and use CMake to build the library with source directory `T-LOX/tlox` and build directory `T-LOX/tlox/build`. Unreal is currently expecting the Release build on x64.

If needed, you can change the filepath for the static library `.lib` in `T-LOX\Source\T_LOX\T_LOX.Build.cs`

### Adding Levels
Put your `.tlox` level files in the `T-LOX/Content/Levels` folder, and add an entry to the data table DT_Levels with the filename of the level.

Levels will be played in the same order as shown in the data table.