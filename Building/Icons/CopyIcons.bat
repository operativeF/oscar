@echo off
setlocal
set destdir=..\..\oscar\icons

copy logo.ico %destdir%\logo.ico
copy oscar.icns %destdir%\oscar.icns

copy full-220.png %destdir%\logo-lg.png
copy full-100.png %destdir%\logo-lm.png
copy full-64.png %destdir%\logo-md.png
copy wave-32.png %destdir%\logo-sm.png
