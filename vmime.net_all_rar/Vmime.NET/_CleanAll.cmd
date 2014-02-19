
attrib -h *.suo
del *.suo
del *.ncb




del libvmime\*.aps
del libvmime\*.opt
del libvmime\*.plg
del libvmime\*.ilk
del libvmime\*.user

rmdir libvmime\Release32 /S /Q
rmdir libvmime\Release64 /S /Q
rmdir libvmime\Debug32 /S /Q
rmdir libvmime\Debug64 /S /Q




del DemoC++\*.aps
del DemoC++\*.opt
del DemoC++\*.plg
del DemoC++\*.ilk
del DemoC++\*.user

rmdir DemoC++\Release32 /S /Q
rmdir DemoC++\Release64 /S /Q
rmdir DemoC++\Debug32 /S /Q
rmdir DemoC++\Debug64 /S /Q





del DemoC#\*.aps
del DemoC#\*.opt
del DemoC#\*.plg
del DemoC#\*.ilk
del DemoC#\*.user

rmdir DemoC#\obj /S /Q
rmdir DemoC#\bin /S /Q





del vmime.NET\*.aps
del vmime.NET\*.opt
del vmime.NET\*.plg
del vmime.NET\*.ilk
del vmime.NET\*.user

rmdir vmime.NET\Release32 /S /Q
rmdir vmime.NET\Release64 /S /Q
rmdir vmime.NET\Debug32 /S /Q
rmdir vmime.NET\Debug64 /S /Q




attrib -h Converter\*.suo
del Converter\*.suo
del Converter\*.ncb
del Converter\*.aps
del Converter\*.opt
del Converter\*.plg
del Converter\*.ilk
del Converter\*.user

del Converter\Release\*.pdb
del Converter\Release\*.vshost.exe

rmdir Converter\Debug /S /Q
rmdir Converter\Properties /S /Q
rmdir Converter\obj /S /Q




del output32\*.pdb
del output32\*.ilk
del output32\*.exe

del output64\*.pdb
del output64\*.ilk
del output64\*.exe

