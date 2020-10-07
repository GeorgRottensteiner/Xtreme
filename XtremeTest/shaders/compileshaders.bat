@echo off
del *.cso
for %%f in (vs_*.hlsl) do (
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  fxc.exe /E"main" /nologo /T vs_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f
  if errorlevel 1 goto error
)
for %%f in (ps_*.hlsl) do (
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  fxc.exe /E"main" /nologo /T ps_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f
  if errorlevel 1 goto error
)

echo .
echo ALL SHADERS COMPILED OK
goto :end

:error
echo .
echo AN ERROR OCCURRED DURING SHADER COMPILATION!
rem Shaders\fxc.exe /T lib_4_0_level_9_1 /Fo Shaders/flat.psc Shaders/flat_light_0.psh
rem Shaders\fxc.exe /T lib_4_0_level_9_1 /Fo Shaders/flat.psc Shaders/flat.psh


:end