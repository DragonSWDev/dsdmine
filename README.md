# dsdmine
This is an cross platform implementation of Minesweeper-like game written from scratch in C++ with SDL2, Dear ImGui and mINI. 

<span style="display:block;text-align:center">![Screenshot](./doc/screenshot.png)

## Build guide

### 1. Clone repository

### 2. Install SDL2, SDL2_image and cmake 

#### Linux:
Install SDL2, SDL2_image development libraries and cmake. For informations about installing packages check manual of distribution you are using.

#### Windows:
Download and install CMake from https://cmake.org site. Also download SDL2 from https://www.libsdl.org site and SDL2_image from https://www.libsdl.org/projects/SDL_image/ site. Make sure to download development libraries for Visual C++. SDL2 and SDL2_image are expected to be present in sdl2 directory in source root. Create sdl2 directory in source root and extract downloaded archives there. Change extracted directories names to SDL2 and SDL2_image respectively. If you wish to use different directories for SDL2 and SDL2_image modify CMakeLists.txt file to point SDL2_PATH and SDL2_IMAGE_PATH variables to selected directories. Build was tested with Visual Studio 2022 Community Edition but probably older versions should be able to build it as well (not tested).

#### macOS:
Build was tested with framework installation. Download and install CMake from https://cmake.org site. Also download SDL2 from https://www.libsdl.org site and SDL2_image from https://www.libsdl.org/projects/SDL_image/ site. Install SDL2 and SDL2_image frameworks in /Library/Frameworks directory. Command line developers tools are also needed.

### 3. Build
#### Linux:
Use CMake to generate Makefile and make for building
```console
mkdir build
cd build
cmake ..
make
```

#### Windows:
Use CMake to generate Visual Studio solution. Open and build generated solution in Visual Studio.

#### macOS:
Use CMake to generate Makefile and use terminal to build with make command.

### 4. Running

On Windows and Linux dsdmine expects assets directory to be present alongside binary. On Windows also SDL2 and SDL2_image (with dependencies) dlls are needed. They are distributed with development libraries downloaded in step 2. On macOS build generates App Bundle (dsdmine.app) and assets directory is automatically copied into generated bundle.

## Manual
### Command line arguments

**-portable** - Run without creating and reading ini configuration file. Best times won't be saved.

### Configuration
Configuration file is located in these directories:

#### Linux
~/.local/share/DragonSWDev/dsdmine/

#### Windows
%appdata%\DragonSWDev\dsdmine\

#### macOS
~/Library/Application Support/dsdmine/

## License
dsdmine is distributed under the terms of MIT License. Project depends on [SDL2](https://www.libsdl.org), [SDL2_image](https://www.libsdl.org/projects/SDL_image/), [Dear ImGui](https://github.com/ocornut/imgui) and [mINI](https://github.com/pulzed/mINI). For information about these dependencies licensing check their respective websites.
