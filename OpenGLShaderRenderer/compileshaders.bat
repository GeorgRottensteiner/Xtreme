@echo off
del *.cso
del *.pdb
del *.h
for /r %%f in ("..\..\DX11Renderer\shaders\vs_*.hlsl") do (
  echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  rem Debug
  rem fxc.exe /Od /Zi /E"main" /nologo /T vs_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f /Fd %%~df%%~pf%%~nf.pdb
  rem Release
  HLSL2GLSLConverter.exe -i %%f -o %%f.glsl -t vs -noglsldef
  if errorlevel 1 goto error
)
for %%f in (..\..\DX11Renderer\shaders\ps_*.hlsl) do (
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  rem debug
  rem fxc.exe /Od /Zi /E"main" /nologo /T ps_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f /Fd %%~df%%~pf%%~nf.pdb
  rem Release
  HLSL2GLSLConverter.exe -i %%f -o %%f.glsl -t ps -noglsldef
  if errorlevel 1 goto error
)

echo .
echo ALL SHADERS COMPILED OK
echo .
