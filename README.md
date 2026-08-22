# Zero-Theory
A word finding game.

# Download

<table>
  <tr>
    <td align="left"><b>Source Code</b>: (<a href="https://github.com/sinhamajoni06-cell/Zero-Theory/tree/main#project-folder"><code>Zero-Theory-main.zip</code></a>)</td>
    <td align="right"><a href="https://github.com/sinhamajoni06-cell/Zero-Theory/archive/refs/heads/main.zip">Download</a></td>
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
- It will create `Game.exe` and other files and folders at-
  ```
  Zero-Theory-core/
  └── bin/
      └── Game.exe
  ```

