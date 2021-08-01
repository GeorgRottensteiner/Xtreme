$fileEntries = [IO.Directory]::EnumerateFiles( "d:/projekte/Xtreme/OpenGLShaderRenderer/shaders", "*.glsl" ); 

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