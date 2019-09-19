# OSCAR Build Instructions for Mac

## Prerequisites

- [Qt 5.12.5] (the current LTS release as of OSCAR 1.1.0)
- [macOS 10.12 Sierra] or higher for building (required by Qt 5.12)
- Command-Line Tools for Xcode 9.2, and optionally [Xcode] itself
    - Xcode 9.2 is the last version that runs on macOS 10.12
    - Xcode 10.1 is the last version that runs on macOS 10.13
    - Xcode 10.3 is the latest version that runs on macOS 10.14

NOTE: Official builds are currently made with [macOS 10.14 Mojave] and Command-Line Tools for [Xcode] 10.3.

## Setup
1. Install Mac OS X 10.12.6 Sierra (or later) and apply all updates.
     * Optionally create a "build" user.

2. (Optional) Install Xcode 9.2 (or later, if using a newer version of macOS), approx. 7GB:
    1. Open Xcode_9.2.xip to expand it with Archive Utility. This will take a while.
    2. Delete the .xip archive.
    3. Move Xcode.app into /Applications.
    4. Launch Xcode.app and agree to the license.
    5. Uncheck "Show this window..." and close the window.
    6. Xcode > Quit

3. Install the command-line developer tools, approx. 0.6GB:

    1. Launch Terminal.app and run:

            xcode-select --install

    2. Click "Install".
    3. Click "Agree".

   This will download and install the latest version of the Command-Line Tools for Xcode for your version of macOS, without requiring a developer account.

   _Alternatively, the command-line tools installer .dmg can be downloaded from the [Xcode] download site, but you will need a (free) developer account and will
   need to pick the appropriate download for your version of macOS._

4. Install Qt (as "build" user, if created), approx. 3GB:
    1. Mount qt-opensource-mac-x64-5.12.5.dmg
    2. Launch qt-opensource-mac-x64-5.12.5
    3. Next, Skip, Continue, (optionally change the installation directory), Continue
        * Qt is entirely self-contained and can be installed anywhere. It defaults to ~/Qt5.12.5.
        * If you only have the command-line tools installed, the Qt installer will complain that "You need to install Xcode and set up Xcode command line tools." Simply click OK.
    4. Expand Qt 5.12.5 and select "macOS", Continue
    5. Select "I have read and agree..." and Continue, Install
    6. Uncheck "Launch Qt Creator", Done
    7. Eject qt-opensource-mac-x64-5.12.5

## Build

1. Build OSCAR:

        git clone https://gitlab.com/pholy/OSCAR-code.git
        cd OSCAR-code
        mkdir build
        cd build
        ~/Qt5.12.5/5.12.5/clang_64/bin/qmake ../oscar/oscar.pro
        make

   The application is in OSCAR.app.

2. (Optional) Package for distribution:

        ~/Qt5.12.5/5.12.5/clang_64/bin/macdeployqt OSCAR.app -dmg

   The dmg is at OSCAR.dmg.

## (Optional) Using Qt Creator

1. Launch Qt Creator where you installed Qt above, by default ~/Qt5.12.5/Qt Creator.app.
2. File > Open File or Project... and select ~/OSCAR-code/oscar/oscar.pro (or wherever you cloned it above), then click "Configure Project".
3. Configure building:
    1. Click on "Projects" in the left panel.
    2. Under **Build Settings**, in the "Edit build configuration" drop-down menu, select "Release".
    3. Click to expand "Details" for the **qmake** build step.
    4. Uncheck "Enable Qt Quick Compiler", click "No" to defer recompiling.
4. Configure packaging for distribution:
    1. Copy the "Build directory" path from the **Build Settings** panel above. (Default is "/Users/build/OSCAR-code/build-oscar-Desktop_Qt_5_12_5_clang_64bit-Release")
    2. Tools > External > Configure...
    3. Select "Add Tool" from the "Add" drop-down menu near the bottom of the window.
    4. Set the name to "Deploy".
    5. Set the Description to "Creates a distributable .dmg".
    6. Set the Executable to the full path where you installed Qt: "/Users/build/Qt5.12.5/5.12.5/clang_64/bin/macdeployqt".
    7. Set the Arguments to "OSCAR.app -dmg".
    8. Set the working directory to the build directory path copied in step 1.
    9. Click OK.
5. To compile, select Build > Build Project "oscar". The application is in OSCAR.app.
6. To create a .dmg, select Tools > External > Deploy. The dmg is at OSCAR.dmg.

[Qt 5.12.5]: http://download.qt.io/archive/qt/5.12/5.12.5/qt-opensource-mac-x64-5.12.5.dmg
[macOS 10.14 Mojave]: https://apps.apple.com/us/app/macos-mojave/id1398502828?ls=1&mt=12
[macOS 10.13 High Sierra]: https://apps.apple.com/us/app/macos-high-sierra/id1246284741?ls=1&mt=12
[macOS 10.12 Sierra]: https://apps.apple.com/us/app/macos-sierra/id1127487414?ls=1&mt=12
[Xcode]: https://developer.apple.com/download/more/
