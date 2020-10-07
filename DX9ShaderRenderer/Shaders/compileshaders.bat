@echo off
cd Shaders
del *.psc
for %%f in (*.psh) do (
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  fxc.exe /nologo /T ps_2_0 /Fo %%~df%%~pf%%~nf.psc %%f
  if errorlevel 1 goto error
)

echo .
echo ALL SHADERS COMPILED OK
goto :end

:error
echo .
echo AN ERROR OCCURRED DURING SHADER COMPILATION!
rem Shaders\fxc.exe /T ps_2_0 /Fo Shaders/flat.psc Shaders/flat_light_0.psh
rem Shaders\fxc.exe /T ps_2_0 /Fo Shaders/flat.psc Shaders/flat.psh


:end