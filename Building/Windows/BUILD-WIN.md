Creating OSCAR development environment on Windows, compiling, and building installers
=====================================================================================

This document is intended to be a brief description of how to install the necessary components to build OSCAR and create installers for Windows 32-bit and 64-bit versions.

On my computers, I have QT installed in E:\\QT and the OSCAR code base in E:\\oscar\\oscar-code. On another computer, they are on the F: drive. All references in the deploy.bat file are relative, so it should run with Oscar-code installed at any location.

**Required Programs**

The following programs and files are required to create Windows installers:

-   Inno Setup 6.0.2 from <http://www.jrsoftware.org/isdl.php>. Download and install innosetup-qsp-6.0.2.exe.
    
-   GIT for windows, from <https://gitforwindows.org/>. GIT for Windows adds itself to your path.
    
-   Gawk is required. You can use the version included with Git for Windows or install Gawk for Windows from <http://gnuwin32.sourceforge.net/packages/gawk.htm>. The deployment batch file will use the Git for Windows version if gawk.exe is not in your PATH.
    
-   QT Open Source edition from <https://www.qt.io/download>. I use version 5.12.4. More recent versions may also work but I have not tested any.

**Installing Inno Setup 6**

Inno Setup 6.0.2 is found on <http://www.jrsoftware.org/isdl.php>. Download and install innosetup-qsp-6.0.2.exe.

The deployment batch file assumes that Inno Setup is installed into its default location: C:\\Program Files (x86)\\Inno Setup 6. If you put it somewhere else, you will have to change the batch file.

Run the installer, accepting options to install inno script studio (for possible future use) and install Inno Setup Preprocessor. Encryption support is not needed, so do not select it.

**Installing GIT for Windows**

Go to <https://gitforwindows.org/> and click on the Download button. Run the installer, which presents lots of options:

-   Select whichever editor you desire.

-   Select “Use Git and optional Unix tools from the Command Prompt.” If you do this, rather than “Git from the command line and also from 3rd-party software,” you will not need to install Gawk separately, as it is included with Git for Windows.
    
-   Select “Use the OpenSSL library.”

-   Select “Checkout Windows-style, commit Unix-style line endings.”

-   Select “Use Windows’ default console window.” I find the Windows default console to be satisfactory on Windows 10.
    
-   Leave extra options as they default (enable file system caching, enable Git credential manager, but not symbolic links).

GIT for Windows adds itself to your path.

**Installing Gawk (if Git for Windows’ gawk is not used)**

From <http://gnuwin32.sourceforge.net/packages/gawk.htm>, download setup for “Complete package, except sources”. When downloaded, run the setup program. Accept default options and location. The deployment batch file assumes that gawk.exe is in your PATH, so either add c:\\Program Files (x86)\\gnuwin32\\bin to your PATH or copy the executables to some other directory already in your PATH.

**Installing QT**

Go to QT at <https://www.qt.io/download> and download the Open Source edition of the Windows online installer, qt-unified-windows-x86-3.1.1-online.exe. Run the installer:

-   Logon with your QT account or create an account if needed.

-   Click Next to download meta information (this takes a while).

-   Choose your installation directory (I picked E:\\Qt, but there are no dependencies on where QT is located)
    
-   Select components:

    -   In QT 5.12.4:

        -   MinGW 7.3.0 32-bit

        -   MinGW 7.3.0 64-bit
		
		-	Sources

    -   In Developer and Designer Tools:

        -   QT Creator 4.10.0 CDB Debug (this may not be required)

        -   MinGW 7.3.0 32-bit

        -   MinGW 7.3.0 64-bit

And complete the installation (this also takes a while).

**Getting Started Developing Oscar in QT Creator**

In browser, log into your account at gitlab.com. Select the Oscar project at https://gitlab.com/pholy/OSCAR-code. Clone a copy of the repository to a location on your computer.

Start QT. There are two QT Oscar project files: OSCAR_QT.pro in the Oscar-code directory, and Oscar.pro in the Oscar-code\\oscar directory. You may use *either* project file. Both will create a running version of Oscar. I find building with Oscar.pro in the Oscar-code\\oscar directory to be very slightly faster, but the difference is negligible.

QT it will ask you to select your kits and configure them. Select both MinGW 7.3.0 32-bit and 64-bit kits.

Click on Projects in the left panel to show your active project (“oscar”) and **Build & Run** settings. Click on the **Build** line for either 32-bit or 64-bit.

In the Build settings in the center panel, select “Release” rather than the default “Debug” in the pull-down at the top of the Build Settings. Click on Details for the qmake build step. By default, “Enable Qt Quick Compiler” is checked. Remove that check – errors result if it is on. QT will ask if you want to recompile everything now. Don’t, as there is more to do before compiling. Make this same change for the Release build for both 32-bit and 64-bit kits. If you want to use the QT Creator Debug tools, select the Build Debug pull-down and disable the QT Quick Compiler there as well.

With these changes, you can build, debug, and run Oscar from QT Creator.  However, to run Oscar from a Windows shortcut, not in the QT environment, you must create a deployment directory that contains all files required by Oscar.  Creating an installer also requires an additional step.

A deploy.bat file performs both functions.  It creates a release directory and an installer.  You can include this deployment file in QT Creator in one of two ways.  You can include it as part of QT Creator's build process, or you can do this as a separate deployment step.

To include deployment as part of the Release build process, add a custom process step to the configuration. Be sure to select “Release” rather than the default “Debug” in the pull-down at the top of the Build Settings.

Create a custom process step as the final build step. Put the fully qualified path for deploy.bat in the command field. Don’t touch “working directory.” Any string you care to place in the “arguments” field will be appended to the installer executable file name.

Do the same for both 32-bit and 64-bit Build settings.

Now you should be able to build the OSCAR project from the QT Build menu.

To make 32-bit or 64-bit builds, just make sure the correct Build item is selected in the Build & Run section on the left.

If you prefer to run deploy.bat as a separate deployment step, select Run under the kit name in the Build & Run section.  Under Run Settings, select Add Deploy Step.  Now create a custom process step just as described earlier.  Menu item Build/Deploy will now run this deployment script.

**Compiling and building from the command line**

If you prefer to build from the command line and not use QT Creator, a batch script buildall.bat will build and create installers for both 32-bit and 64-bit versions of Windows. This script has some hard-coded paths, so will need to be modified for your system configuration.  This batch file is not well tested, as I prefer to build from QT Creator.

**The Deploy.BAT file**

The deployment batch file creates two folders inside the shadow build folder:

Release – everything needed to run OSCAR. You can run OSCAR from this directory just by clicking on it.

Installer – contains an installer exe file that will install this product.
