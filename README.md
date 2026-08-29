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
  <td align="left"><b>Zero-Theory (`v0.1.0`)</b>: (<a href="https://github.com/sinhamajoni06-cell/Zero-Theory/releases#release-v0.1.0"><code>ZeroTheory.zip</code></a>)</td>
    <td align="right"><a href="https://github.com/sinhamajoni06-cell/Zero-Theory/releases/download/v0.1.0/ZeroTheory.zip">Download</a></td>
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
  │   │   ├── header/         # Additional C++ headers
  │   │   └── cpp/            # Must contain at least one .cpp file
  │   └── src/
  │       └── engine/
  │           └── NoTEngine.cpp # Main entry point file
  ├── main/                   # All Script and Additional files creates this automatically for output
  ├── build.bat               # Main build script to execute on your system
  ├── .README/                # All the other elements for README.md file
  └── README.md               
```
---
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
- Now you have install `G++` and `SFML`, just run the ***build.bat*** file.
- It will create `ZeroTheory.exe` and other files and folders at-
  ```
  ZeroTheory/
  ├── ZeroTheory.exe         # This is the main exe game file, you need to run to play the game
  ├── *sfml*.dll
  ├── libfreetype-6.dll
  ├── libgcc_s_seh-1.dll
  ├── libstdc++-6.dll
  ├── libwinpthread-1.dll
  └── main/                  # All Games materials are in here
      └── assets/
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
     
   - Under **User variables**, select `Path` and click **Edit**.\

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
  - **Version**- `v0.1.0`
  - **Name**- `Zero Theory`
