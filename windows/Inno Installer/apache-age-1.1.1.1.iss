[Setup]
AppName=Apache Age
AppVersion=1.1.1
AppPublisher=Apache
DefaultDirName=C:/PROGRA~1/POSTGR~1/12/lib
DefaultGroupName=PostgreSQL Extensions
OutputDir=.



[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]

Source: "age.dll"; DestDir: "C:/PROGRA~1/POSTGR~1/12/lib"
Source: "age--1.1.1.sql"; DestDir: "C:/PROGRA~1/POSTGR~1/12/share/extension"
Source: "age.control"; DestDir: "C:/PROGRA~1/POSTGR~1/12/share/extension"

