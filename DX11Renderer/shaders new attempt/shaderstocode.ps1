$fileEntries = [IO.Directory]::EnumerateFiles( ".", "*.cso" ); 

#$line = "std::map<std::string,std::pair<ByteBuffer,GR::u32> >    m_BasicVertexShaders;" + [System.Environment]::NewLine
#$line += "std::map<std::string,std::pair<ByteBuffer,GR::u32> >    m_BasicPixelShaders;" + [System.Environment]::NewLine

[IO.File]::Delete( "p:/common/xtreme/DX11/shaderbinary.inl" );

foreach( $fileName in $fileEntries ) 
{ 
  $content = [IO.File]::ReadAllBytes( $fileName );

  $trueFilename = $fileName.Substring( 2, $fileName.Length - 4 - 2 );
  if ( $trueFilename.ToUpper().EndsWith( "FLAT" ) )
  {
    continue;
  }
  [Console]::WriteLine( $trueFilename ); 

  $hex = [BitConverter]::ToString( $content );
  $hex = $hex.Replace( "-", "" );

  if ( $trueFilename.StartsWith( "VS" ) )
  {
    $lineShader = 'hadError |= ( AddBasicVertexShader( "' + $trueFilename.Substring( 3 ) + '",' + [System.Environment]::NewLine
  }
  else
  {
    $lineShader = 'hadError |= ( AddBasicPixelShader( "' + $trueFilename.Substring( 3 ) + '",' + [System.Environment]::NewLine
  }

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


  $curPos = 0;

  while ( $curPos -lt $hex.Length )
  {
    if ( $curPos + 300 -lt $hex.Length )
    {
      $lineShader += ( "GR::String( " + '"' + $hex.SubString( $curPos, 300 ) + '" ) +' + [System.Environment]::NewLine )
      $curPos += 300;
    }
    else
    {
      $lineShader += ( "GR::String( " + '"' + $hex.SubString( $curPos ) + '" ),' + [System.Environment]::NewLine )
      $lineShader += ( '0x' + $vertexFormat.ToString( "X8" ) + ' ) == NULL );' + [System.Environment]::NewLine )
      $curPos = $hex.Length;
    }
  }

  [IO.File]::AppendAllText( "p:/common/xtreme/DX11/shaderbinary.inl", $lineShader );

  #[Console]::WriteLine( $lineShader );
}      