$projectBasePath = "d:/projekte";
if ( [IO.Directory]::Exists( "d:\privat\projekte" ) )
{
  $projectBasePath = "d:/privat/projekte";
}
echo ( "Project base path: " + $projectBasePath )
$fileEntries = [IO.Directory]::EnumerateFiles( $projectBasePath + "/Xtreme/OpenGLShaderRenderer/shaders", "*_es.glsl" ); 

[IO.File]::Delete( "p:/common/Xtreme/OpenGLShader/shadercodees.inl" );

echo ( "Processing ES shaders..." )

$resultContentES = "";
$lineBreak = "`n";  

foreach( $fileName in $fileEntries ) 
{ 
  $dir = [System.IO.Path]::GetDirectoryName($fileName);
  $nameWithoutExt = [System.IO.Path]::GetFileNameWithoutExtension($fileName);
  if ( !$nameWithoutExt.EndsWith( "_es" ) )
  {
    continue;
  }
  
  $content = [IO.File]::ReadAllLines( $fileName );
  $trueFilename = $nameWithoutExt;

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

  $varName = $nameWithoutExt.SubString( 0, $nameWithoutExt.Length - 3 );

  
  $line = "GR::String  " + $varName + " =" + $lineBreak;

  foreach ( $shaderLine in $content )
  {
    $shaderLine = $shaderLine.Replace( "\", "\\" );
    $shaderLine = $shaderLine.Replace( "`"", "'" );
    $line += '"' + $shaderLine + '\n"' + $lineBreak;
  }
  $line += ";" + $lineBreak + $lineBreak;

  $resultContentES += $line;
}

[IO.File]::WriteAllText( "P:\Common\Xtreme\OpenGLShader\shadercodees.inl", $resultContentES )

echo ( "...done" )