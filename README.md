# Zero-Theory
A undesigned game.

# Download

<table>
  <tr>
    <td align="left"><b>Source Code</b>: (<a href="https://github.com/sinhamajoni06-cell/Zero-Theory/tree/main#project-folder"><code>Zero-Theory-main.zip</code></a>)</td>
    <td align="right"><a href="https://github.com/sinhamajoni06-cell/Zero-Theory/archive/refs/heads/main.zip">Download</a></td>
  </tr>
  <tr>
    <td align="left"><b>All Assets</b>: (<a href="https://github.com/sinhamajoni06-cell/Zero-Theory/blob/main/main/assets/images/temp.zip"><code>temp.zip</code></a>)</td>
    <td align="right"><a href="https://github.com/sinhamajoni06-cell/Zero-Theory/raw/refs/heads/main/main/assets/images/temp.zip">Download</a></td>
  </tr>
  <td align="left"><b>Zero-Theory (`v0.2.0`)</b>: (<a href="https://github.com/sinhamajoni06-cell/Zero-Theory/releases#release-v0.2.0"><code>ZeroTheory.zip</code></a>)</td>
    <td align="right"><a href="https://github.com/sinhamajoni06-cell/Zero-Theory/releases/download/v.0.2.0/ZeroTheory.zip">Download</a></td>zz
  </tr>
</table>


## Requirements
- **Operating System**: *Windows* `10` or `11` (Recommended).
- **Compiler**: [g++](https://www.msys2.org/) supporting at least `C++17` or more (standard). ~*added to your system's or user's PATH environment variable*.
- **MSYS2 Environment**: Specifically installed at `C:\msys64\` using the *UCRT64* environment, as hardcoded in the script ***paths***.
- **Step-by-Step Installation**: For g++ [Continue](https://github.com/sinhamajoni06-cell/Zero-Theory#g-installation-and-setup)

# Project Folder

```
Zero-Theory-core/
├── core/
│   ├── lib/
│   │   ├── header/                    # C++ header files (-I core/lib/header)
│   │   └── cpp/                       # Shared engine implementation files (*.cpp)
│   └── src/
│       ├── editor/
│       │   ├── zero_map.cpp           # Map Editor main entry point
│       │   └── zero_editor_session.cpp# Editor session, UI, and tool interaction logic
│       └── engine/
│           └── NoTEngine.cpp          # Main Game entry point
├── main/
│   └── assets/                        # Game resources and material assets
│       └── map/                       # Map workspace assets synced during build
├── .README/                           # Additional assets for README rendering
├── build.bat                          # Automated compilation and launcher script
└── README.md                          # Primary repository documentation
```
---

## Build Guide:- 

```
Map_Editor_Architecture/
├── Map_Editor_Source_Files/
│   ├── core/src/editor/zero_map.cpp              # Entry point; initializes application window and main loop
│   ├── core/src/editor/zero_editor_session.cpp   # Active session logic, editing tools, UI, and state management
│   └── core/lib/cpp/*.cpp                        # Core engine implementations shared between game and editor
├── Include_Headers/
│   ├── core/lib/header/                          # Header declarations for core libraries
│   ├── core/src/                                 # General project header files
│   └── core/src/engine/                          # Engine sub-system and data structure headers
├── External_Deps/
│   ├── SFML (Graphics, Window, System)           # Media library handling rendering, windows, and input
│   ├── OpenGL / GDI32 / FreeType                 # Low-level graphics rendering, Win32 GDI, and font parsing
│   └── main/assets/map/                          # Map asset files copied into the final executable folder
└── Outputs/
    └── ZeroTheory/MapEditor.exe                  # Executable output generated from compiled sources and links
```

## G++ Installation and Setup

- Go to [msys2.org](https://www.msys2.org/) and Install G++. (**Recommended**: C++17)
- double-click the downloaded `installer` and run it.
- Keep the default destination path: `C:\msys64` as it is. 

  <a href="https://www.msys2.org/"><img src="https://www.msys2.org/images/install-2-path-dark.png#gh-dark-mode-only" width="500" height="400" valign="middle"></a>
- Once installed, launch **MSYS2 UCRT64** from your Start menu.

  <a href="https://www.msys2.org/"><img src="https://www.msys2.org/images/install-4-terminal-dark.png#gh-dark-mode-only" width="500" height="400" valign="middle"></a>
- In **MSYS2 terminal** to install *SFML* run this single command inside it-
  
  ```
  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sfml
  ```
- Now you have install `G++` and `SFML`, just run the ***build.bat*** file. (**Recommendation**: Turn of your Windows Defender **/** any Antivirus software)
- It will create `ZeroTheory.exe` **/** `MapEditor.exe` and other files and folders at-
  ```
  ZeroTheory/
  ├── MapEditor.exe          # Executable for launching the Map Editor tool
  ├── ZeroTheory.exe         # Main executable for playing the game
  ├── *sfml*.dll             # SFML runtime libraries (graphics, window, system)
  ├── libfreetype-6.dll      # FreeType font rendering library
  ├── libgcc_s_seh-1.dll     # GCC C runtime library
  ├── libstdc++-6.dll        # C++ Standard Library runtime
  ├── libwinpthread-1.dll    # POSIX threads support library
  └── main/                  # Core game materials and resources
    └── assets/            # Main asset directory
        └── map/           # Map data and editor workspace assets
  ```
  ## Development Worning-

  - if you got an error then-
  
    <a href="https://github.com/sinhamajoni06-cell/Zero-Theory"><img src="https://raw.githubusercontent.com/sinhamajoni06-cell/Zero-Theory/refs/heads/main/.README/Screenshot%202026-08-27%20161355.png" width="800" height="300" valign="middle"></a>

- **then follow**-
  - Press `Win` + `R`, and type
    

    ```
    sysdm.cpl
    ```
   
     <a href="https://github.com/sinhamajoni06-cell/Zero-Theory"><img src="https://raw.githubusercontent.com/sinhamajoni06-cell/Zero-Theory/refs/heads/main/.README/Screenshot%202026-08-27%20163421.png" width="300" height="150" valign="middle"></a>

   - Go to the **Advanced** tab and click **Environment Variables**.
     
     <a href="https://github.com/sinhamajoni06-cell/Zero-Theory"><img src="https://raw.githubusercontent.com/sinhamajoni06-cell/Zero-Theory/refs/heads/main/.README/Screenshot%202026-08-27%20180006.png" width="450" height="400" valign="middle"></a>
     
   - Under **User variables**, select `Path` and click **Edit**.

     <a href="https://github.com/sinhamajoni06-cell/Zero-Theory"><img src="https://raw.githubusercontent.com/sinhamajoni06-cell/Zero-Theory/refs/heads/main/.README/Screenshot%202026-08-27%20180756.png" width="450" height="400" valign="middle"></a>
     
   - Click **New** and paste the folder path you copied. e.g.
     ```
     C:\msys64\ucrt64\bin
     ```
     <a href="https://github.com/sinhamajoni06-cell/Zero-Theory"><img src="http://raw.githubusercontent.com/sinhamajoni06-cell/Zero-Theory/refs/heads/main/.README/Screenshot%202026-08-27%20181615.png" width="450" height="400" valign="middle"></a>

     - Click **OK** on all open windows to `save` the changes.

### Confirmation 
* if you want to **check** that is it done perfectly or not, Run-
```
g++ --version
```
in your `Terminal` **/** `cmd`.
* Once the version number appears, re-run your build script.

 
# Project details-
- **Language**: `C/C++`
- **Graphics**: `2D`
- **Art Style**: `Pixel Art`
- **Gameplay**: `Exploration`, `Combat`, `Word Finding`
- **Combat**: `Melee` and `Range`
- **Player count**: `3`
- **Chapter**: `3`
- **Game**-
  - **Version**- `v0.2.0`
  - **Name**- `Zero Theory`
