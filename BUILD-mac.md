# OSCAR Build Instructions for Mac

## Prerequisites

- [Qt 5.12.2] (the current LTS release as of OSCAR 1.0.0)
- [macOS 10.12 Sierra] or higher for building (required by Qt 5.12)
- [Xcode] 9.2 or later with command-line tools
    - Xcode 9.2 is the last version that runs on macOS 10.12
    - Xcode 10.1 is the last version that runs on macOS 10.13

NOTE: Official builds are currently made with [macOS 10.13 High Sierra] and [Xcode] 10.1.

## Setup
1. Install Mac OS X 10.12.6 Sierra (or later) and apply all updates.
     * Optionally create a "build" user.

2. Install Xcode 9.2 (or later, if using a newer version of macOS) and command-line tools:
    1. Open Xcode_9.2.xip to expand it with Archive Utility. This will take a while.
    2. Delete the .xip archive.
    3. Move Xcode.app into /Applications.
    4. Launch Xcode.app and agree to the license.
    5. Uncheck "Show this window..." and close the window.
    6. Xcode > Quit
    7. Mount Command_Line_Tools_macOS_10.12_for_Xcode_9.2.dmg
    8. Launch Command Line Tools (macOS Sierra version 10.12).pkg
    9. Proceed through all the prompts until the installation is complete, then Close.
    10. Eject Command Line Developer Tools.

3. Install Qt 5.12.2 (as "build" user, if created):
    1. Mount qt-opensource-mac-x64-5.12.2.dmg
    2. Launch qt-opensource-mac-x64-5.12.2
    3. Next, Skip, Continue, Continue
    4. Expand Qt 5.12.2 and select "macOS", Continue
    5. Select "I have read and agree..." and Continue, Install
    6. Uncheck "Launch Qt Creator", Done
    7. Eject qt-opensource-mac-x64-5.12.2

## Build

1. Build OSCAR:

        git clone https://gitlab.com/pholy/OSCAR-code.git
        mkdir build
        cd build
        ~/Qt5.12.2/5.12.2/clang_64/bin/qmake ../OSCAR-code/OSCAR_QT.pro
        make

   The application is in oscar/OSCAR.app.

2. (Optional) Package for distribution:

        cd oscar
        ~/Qt5.12.2/5.12.2/clang_64/bin/macdeployqt OSCAR.app -dmg

   The dmg is at OSCAR.dmg.

[Qt 5.12.2]: http://download.qt.io/archive/qt/5.12/5.12.2/qt-opensource-mac-x64-5.12.2.dmg
[macOS 10.13 High Sierra]: https://itunes.apple.com/us/app/macos-high-sierra/id1246284741?ls=1&mt=12
[macOS 10.12 Sierra]: https://itunes.apple.com/us/app/macos-sierra/id1127487414?ls=1&mt=12
[Xcode]: https://developer.apple.com/download/more/
