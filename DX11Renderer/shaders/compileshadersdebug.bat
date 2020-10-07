@echo off
del *.cso
del *.pdb
del *.h
for %%f in (vs_*.hlsl) do (
  echo Compiling %%f
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  rem Debug
  fxc.exe /Od /Zi /E"main" /nologo /T vs_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f /Fd %%~df%%~pf%%~nf.pdb
  rem Release
  rem fxc.exe /O1 /E"main" /nologo /T vs_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f
  if errorlevel 1 goto error
)
for %%f in (ps_*.hlsl) do (
  rem echo %%f
  rem echo %%~df%%~pf%%~nf.psc
  rem debug
  fxc.exe /Od /Zi /E"main" /nologo /T ps_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f /Fd %%~df%%~pf%%~nf.pdb
  rem Release
  rem fxc.exe /O1 /E"main" /nologo /T ps_4_0_level_9_1 /Fo %%~df%%~pf%%~nf.cso %%f
  if errorlevel 1 goto error
)

echo .
echo ALL SHADERS COMPILED OK
echo .

del shaderhex.txt
@echo off > shaderhex.txt
for %%f in (vs_*.cso) do (
  echo Hexing... %%f
  echo %%f >> shaderhex.txt
  rem bintool -mode cut -in %%f -outcarray -outwrap 32 >> shaderhex.txt
  bintool -mode cut -in %%f -outhex -outwrap 3200 >> shaderhex.txt
)
for %%f in (ps_*.cso) do (
  echo Hexing... %%f
  echo %%f >> shaderhex.txt
  rem bintool -mode cut -in %%f -outcarray -outwrap 32 >> shaderhex.txt
  bintool -mode cut -in %%f -outhex -outwrap 3200 >> shaderhex.txt
)

powershell ./shaderstocode.ps1

goto :end

:error
echo .
echo AN ERROR OCCURRED DURING SHADER COMPILATION!


:end