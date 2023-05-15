[Setup]
AppName=Apache Age
AppVersion=1.1.1
AppPublisher=Apache
DefaultDirName=C:/PROGRA~1/POSTGR~1/12/lib
DefaultGroupName=PostgreSQL Extensions
OutputDir=.
OutputBaseFilename=apache-age-1.1.1
Compression=lzma2/ultra64
SolidCompression=yes
LicenseFile="license.txt"
WizardSmallImageFile=logo.bmp
SetupIconFile=appicon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]

Source: "age.dll"; DestDir: "C:/PROGRA~1/POSTGR~1/12/lib"
Source: "age--1.1.1.sql"; DestDir: "C:/PROGRA~1/POSTGR~1/12/share/extension"
Source: "age.control"; DestDir: "C:/PROGRA~1/POSTGR~1/12/share/extension"
Source: "logo.bmp"; DestDir: "{app}"; Flags: dontcopy
Source: "appicon.ico"; DestDir: "{app}"; Flags: dontcopy
Source: "license.txt"; DestDir: "{app}"; Flags: isreadme

[Icons]
Name: "{group}\Uninstall AGE"; Filename: "{uninstallexe}"
