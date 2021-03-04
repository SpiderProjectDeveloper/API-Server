REM cl /EHsc WebServer.cpp user32.lib shell32.lib
REM cl demo.cpp ftp.cpp sftp.cpp ssh.lib libeay32.lib ssleay32.lib zlib.lib /I"..\vcpkg\packages\libssh_x86-windows\include" /I"..\vcpkg\packages\openssl-windows_x86-windows\include" /I"..\vcpkg\packages\zlib_x86-windows\" /link /L"..\vcpkg\packages\libssh_x86-windows\lib" /L"..\vcpkg\packages\openssl-windows_x86-windows\lib" 
REM cl /EHsc /I C:\lib\boost_1_62_0 WebServer.cpp /link /LIBPATH:C:\lib\boost_1_62_0\stage\lib
cl /EHsc WebServer.cpp