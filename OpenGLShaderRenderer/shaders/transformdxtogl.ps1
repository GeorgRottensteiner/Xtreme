copy ..\..\DX11Renderer\shaders\*.hlsl .
copy ..\..\DX11Renderer\shaders\*.inc .

$fileEntries = [IO.Directory]::EnumerateFiles( ".", "VS_*.hlsl" ); 

foreach( $fileName in $fileEntries ) 
{ 
  $outputFilename = [IO.Path]::GetFileNameWithoutExtension( $fileName ) + ".glsl";
  $executable = ( $curPath + "/HLSL2GLSLConverter.exe" )
  & $executable -i $fileName -o $outputFilename -noglsldef -t vs
  #& $executable -i $fileName -o $outputFilename -t vs
}      



$fileEntries = [IO.Directory]::EnumerateFiles( ".", "PS_*.hlsl" ); 

foreach( $fileName in $fileEntries ) 
{ 
  $outputFilename = [IO.Path]::GetFileNameWithoutExtension( $fileName ) + ".glsl";
  $executable = ( $curPath + "/HLSL2GLSLConverter.exe" )
  & $executable -i $fileName -o $outputFilename -noglsldef -t ps
  #& $executable -i $fileName -o $outputFilename -t ps
}      


del *.hlsl
del *.inc


$fileEntries = [IO.Directory]::EnumerateFiles( ".", "*.glsl" ); 

[IO.File]::Delete( "p:/common/Xtreme/OpenGLShader/shadercode.inl" );

$resultContent = "";
$lineBreak = "`n";  

foreach( $fileName in $fileEntries ) 
{ 
  $content = [IO.File]::ReadAllLines( $fileName );
  $trueFilename = $fileName.Substring( 2, $fileName.Length - 4 - 2 );

  # deduce vertex format from file name
  $vertexFormat = 0;
  if ( $trueFilename.IndexOf( "positionrhw" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000004;
  }
  elseif ( $trueFilename.IndexOf( "position" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000002;
  }
  if ( $trueFilename.IndexOf( "normal" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000010;
  }
  if ( $trueFilename.IndexOf( "color" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000040;
  }
  if ( $trueFilename.IndexOf( "specular" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000080;
  }
  if ( $trueFilename.IndexOf( "texture" ) -ne -1 )
  {
    $vertexFormat = $vertexFormat -bor 0x00000100;
  }

  $varName = [IO.Path]::GetFileNameWithoutExtension( $fileName );

  
  $line = "GR::String  " + $varName + " =" + $lineBreak;
  if ( $trueFilename.StartsWith( "VS" ) )
  {
    $line += '"' + "#define VERTEX_SHADER" + '"' + $lineBreak;
  }
  else
  {
    #$line = "#define VERTEX_SHADER`r`n";
  }

  foreach ( $shaderLine in $content )
  {
    $shaderLine = $shaderLine.Replace( "\", "\\" );
    $shaderLine = $shaderLine.Replace( "`"", "'" );
    $line += '"' + $shaderLine + '\n"' + $lineBreak;
  }
  $line += ";" + $lineBreak + $lineBreak;


  $resultContent += $line;

}

[IO.File]::WriteAllText( "P:\Common\Xtreme\OpenGLShader\shadercode.inl", $resultContent )