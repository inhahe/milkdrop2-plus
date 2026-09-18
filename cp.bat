"c:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe" "D:\visual studio projects\foobar2000milkdrop\foo_vis_milk2.sln" -p:Configuration=Debug -p:Platform=x64 -m -verbosity:minimal
copy "D:\visual studio projects\foobar2000milkdrop\Bin\x64\Debug\foo_vis_milk2.dll" c:\foobar2000\profile\user-components-x64\foo_vis_milk2
c:\foobar2000\foobar2000.exe