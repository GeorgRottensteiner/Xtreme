#include ".\OggDSound.h"

#include <debug/debugclient.h>

#include <Xtreme/Environment/XWindow.h>

#include  <process.h>           //for threading support

#pragma comment( lib, "dxguid.lib" )
#pragma comment( lib, "dsound.lib" )

/*
#ifdef _DEBUG
#pragma comment( lib, "ogg_static_d.lib" )
#pragma comment( lib, "vorbisfile_static_d.lib" )
#else
#pragma comment( lib, "ogg_static.lib" )
#pragma comment( lib, "vorbisfile_static.lib" )
#endif
*/

#ifdef _DEBUG
#pragma comment( lib, "libogg_static_d.lib" )
#pragma comment( lib, "libvorbis_static_d.lib" )
#pragma comment( lib, "libvorbisfile_static_d.lib" )
#else
#pragma comment( lib, "libogg_static.lib" )
#pragma comment( lib, "libvorbis_static.lib" )
#pragma comment( lib, "libvorbisfile_static.lib" )
#endif



const int BUFFER_HALF_SIZE = 25 * 1024;     //15 kb, which is roughly one second of OGG sound

int                   VolumeRange[101] =  { -10000, -6644, -5644, -5059, -4644,
                                             -4322, -4059, -3837, -3644, -3474,
                                             -3322, -3184, -3059, -2943, -2837,
                                             -2737, -2644, -2556, -2474, -2396,
                                             -2322, -2252, -2184, -2120, -2059,
                                             -2000, -1943, -1889, -1837, -1786,
                                             -1737, -1690, -1644, -1599, -1556,
                                             -1515, -1474, -1434, -1396, -1358,
                                             -1322, -1286, -1252, -1218, -1184,
                                             -1152, -1120, -1089, -1059, -1029,
                                             -1000,  -971,  -943,  -916,  -889,
                                              -862,  -837,  -811,  -786,  -761,
                                              -737,  -713,  -690,  -667,  -644,
                                              -621,  -599,  -578,  -556,  -535,
                                              -515,  -494,  -474,  -454,  -434,
                                              -415,  -396,  -377,  -358,  -340,
                                              -322,  -304,  -286,  -269,  -252,
                                              -234,  -218,  -201,  -184,  -168,
                                              -152,  -136,  -120,  -105,   -89,
                                               -74,   -59,   -44,   -29,   -14,
                                               0 };



CDirectSoundOgg::CDirectSoundOgg() : 
  m_pPrimarySoundBuffer( NULL ),
  m_pBuffer(NULL),
  m_PlayThread(0),
  m_PlayThreadActive(false),
  m_StopPlaybackEvent(0),
  m_PlaybackDone(false),
  m_pDirectSound( NULL ),
  m_Paused( false ),
  m_FileLoaded( false ),
  m_Volume( 100 )
{
  InitializeCriticalSection( &m_CriticalSection );  
}




bool CDirectSoundOgg::Allocate() 
{
  if ( m_pDirectSound == NULL )
  {
    return false;
  }

  if ( m_Filename.empty() )
  {
    return false;
  }

  FILE* fileHandle = fopen( GetFileName().c_str(), "rb" );

  if ( !fileHandle )
  {
    return false;
  }

  //open the file as an OGG file (allocates internal OGG buffers)
  if ( ( ov_open( fileHandle, &m_VorbisFile, NULL, 0 ) ) != 0 ) 
  {
    fclose( fileHandle );
    return false;
  }

  vorbis_info* vorbisInfo = ov_info( &m_VorbisFile, -1 );

  //get a wave format structure, which we will use later to create the DirectSound buffer description

  WAVEFORMATEX      waveFormat;

  memset( &waveFormat, 0, sizeof( waveFormat ) );

  waveFormat.cbSize           = sizeof( waveFormat );                 //how big this structure is
  waveFormat.nChannels        = vorbisInfo->channels;                 //how many channelse the OGG contains
  waveFormat.wBitsPerSample   = 16;                         //always 16 in OGG
  waveFormat.nSamplesPerSec   = vorbisInfo->rate;                   //sampling rate (11 Khz, 22 Khz, 44 KHz, etc.)
  waveFormat.nAvgBytesPerSec  = waveFormat.nSamplesPerSec * waveFormat.nChannels * 2; //average bytes per second
  waveFormat.nBlockAlign      = 2 * waveFormat.nChannels;               //what block boundaries exist
  waveFormat.wFormatTag       = 1;                          //always 1 in OGG

  //----- prepare DirectSound buffer description

  DSBUFFERDESC dsBufferDescription;

  memset( &dsBufferDescription, 0, sizeof( dsBufferDescription ) );

  dsBufferDescription.dwSize        = sizeof( dsBufferDescription );    //how big this structure is
  dsBufferDescription.lpwfxFormat   = &waveFormat;            //information about the sound that the buffer will contain
  dsBufferDescription.dwBufferBytes = BUFFER_HALF_SIZE * 2;       //total buffer size = 2 * half size
  dsBufferDescription.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_STICKYFOCUS;   //buffer must support notifications

  if ( FAILED( m_pDirectSound->CreateSoundBuffer(&dsBufferDescription, &m_pBuffer, NULL ) ) ) 
  {
    return false;
  }

  // create notification event object used to signal when playback should stop
  m_StopPlaybackEvent = CreateEvent( NULL, false, false, NULL );

  //----- Fill first half of buffer with initial data
  Fill( true ); 

  return true;
}




CDirectSoundOgg::~CDirectSoundOgg() 
{
  if ( IsPlaying() )
  {
    Stop();
  }
  DeleteCriticalSection( &m_CriticalSection );
}



void CDirectSoundOgg::Cleanup() 
{
  if ( m_FileLoaded )
  {
    ov_clear( &m_VorbisFile );
    m_FileLoaded = false;
  }

  if ( m_StopPlaybackEvent )
  {
    CloseHandle( m_StopPlaybackEvent );
    m_StopPlaybackEvent = NULL;
  }

  if ( m_pBuffer )
  {
    m_pBuffer->Release();
    m_pBuffer = NULL;
  }
}




bool CDirectSoundOgg::Fill( const bool FirstHalf ) 
{
  LPVOID  pFirstSegment;              //holds a pointer to the first segment that we lock
  DWORD   nFirstSegmentSize = 0;          //holds how big the first segment is
  LPVOID  pSecondSegment;             //holds a pointer to the second segment that we lock
  DWORD   nSecondSegmentSize = 0;         //holds how big the second segment is

  // lock DirectSound buffer half

  if ( FAILED( m_pBuffer->Lock ( ( FirstHalf ? 0 : BUFFER_HALF_SIZE ),     //if we are locking the first half, lock from 0, otherwise lock from BUFFER_HALF_SIZE
                                 BUFFER_HALF_SIZE,             //how big a chunk of the buffer to block
                                 &pFirstSegment,               //pointer that will receive the locked segment start address
                                 &nFirstSegmentSize,             //will return how big the first segment is (should always be BUFFER_HALF_SIZE)
                                 &pSecondSegment,              //pointer that will receive the second locked segment start address (in case of wrapping)
                                 &nSecondSegmentSize,            //how big a chunk we wrapped with (in case of wrapping)
                                 0                     //flags: no extra settings
                                ) ) ) 
  {
    return false;
  }

  //----- debug safety: we should always have locked a complete segment of the size we requested
  if ( nFirstSegmentSize != BUFFER_HALF_SIZE )
  {
    dh::Error( "OggPlayer: Lock returned wrong size!" );
    return false;
  }

  //----- decode OGG file into buffer
  unsigned int nBytesReadSoFar  = 0; //keep track of how many bytes we have read so far
  long nBytesReadThisTime       = 1; //keep track of how many bytes we read per ov_read invokation (1 to ensure that while loop is entered below)
  int nBitStream                = 0; //used to specify logical bitstream 0

  //decode vorbis file into buffer half (continue as long as the buffer hasn't been filled with something (=sound/silence)
  while ( nBytesReadSoFar < BUFFER_HALF_SIZE ) 
  {
    //decode
    nBytesReadThisTime = ov_read( &m_VorbisFile,                //what file to read from
                                  (char*)pFirstSegment + nBytesReadSoFar,   //where to put the decoded data
                                  BUFFER_HALF_SIZE - nBytesReadSoFar,     //how much data to read
                                  0,                      //0 specifies little endian decoding mode
                                  2,                      //2 specifies 16-bit samples
                                  1,                      //1 specifies signed data
                                  &nBitStream );

    //new position corresponds to the amount of data we just read
    nBytesReadSoFar += nBytesReadThisTime;

    //----- do special processing if we have reached end of the OGG file
    if ( nBytesReadThisTime == 0 ) 
    {
      //----- if looping we fill start of OGG, otherwise fill with silence
      if ( m_Looping ) 
      {
        //seek back to beginning of file
        ov_time_seek( &m_VorbisFile, 0 );
      } 
      else 
      {
        //fill with silence
        for ( unsigned int i = nBytesReadSoFar; i < BUFFER_HALF_SIZE; i++ ) 
        {
          //silence = 0 in 16 bit sampled data (which OGG always is)
          *( (char*)pFirstSegment + i ) = 0;
        }

        //signal that playback is over
        m_PlaybackDone = true;

        //and exit the reader loop
        nBytesReadSoFar = BUFFER_HALF_SIZE;
      }
    }
  } 

  //----- unlock buffer
  m_pBuffer->Unlock( pFirstSegment, nFirstSegmentSize, pSecondSegment, nSecondSegmentSize );

  return true;
}



bool CDirectSoundOgg::IsPlaying() 
{
  return GetPlayThreadActive();
}



void CDirectSoundOgg::Stop() 
{
  if ( !IsPlaying() )
  {
    return;
  }

  m_Paused = false;

  //----- Signal the play thread to stop
  SetEvent( m_StopPlaybackEvent );

  //wait for playing thread to exit
  if ( WaitForSingleObject( m_PlayThread, 500 ) == WAIT_ABANDONED ) 
  {
    //the thread hasn't terminated as expected. kill it
    TerminateThread( m_PlayThread, 1 );
    
    //not playing any more
    SetPlayThreadActive( false );

    //since playing thread has not cleaned up, this thread will have to
    Cleanup();
    
    //TODO: You should report this error here somehow
  }

  //----- store that we are not playing any longer
  SetPlayThreadActive( false );
}



bool CDirectSoundOgg::Play( const bool Looping ) 
{
  if ( IsPlaying() )
  {
    Stop();
    Cleanup();
  }

  Allocate();
  
  SetPlayThreadActive( true );

  m_Looping = Looping;

  unsigned int nThreadID = 0;

  m_PlayThread = (HANDLE)CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)CDirectSoundOgg::PlayingThread, this, 0, (LPDWORD)&nThreadID );

  return ( m_PlayThread != NULL );
}



GR::String CDirectSoundOgg::GetFileName() 
{
  EnterCriticalSection( &m_CriticalSection );
  GR::String filename = m_Filename;
  LeaveCriticalSection( &m_CriticalSection );

  return filename;
}




unsigned int CDirectSoundOgg::PlayingThread( LPVOID lpParam ) 
{
  CDirectSoundOgg* oggInstance = static_cast<CDirectSoundOgg*>( lpParam );

  //allocate all resources
  if ( oggInstance->m_pBuffer == NULL )
  {
    return 1;
  }

  //----- allocations
  bool errorOccured = false;   //assume everything will go ok

  //----- indicate that we are playing
  oggInstance->m_PlaybackDone = false;

  //----- start playing the buffer (looping because we are going to refill the buffer)
  oggInstance->m_pBuffer->SetVolume( VolumeRange[oggInstance->m_Volume] );

  HRESULT   hRes = oggInstance->m_pBuffer->Play( 0, 0, DSBPLAY_LOOPING );
  if ( FAILED( hRes ) )
  {
    return 0;
  }

  //----- go into loop waiting on notification event objects

  //create tracker of what half we have are playing
  bool playingFirstHalf = true;

  //keep going in the loop until we need to stop
  bool  continuePlaying = true;    //used to keep track of when to stop the while loop
  bool  playbackDoneProcessed = false; //used ot only process m_bPlaybackDone once
  int   nStopAtNextHalf = 0;      //0 signals "do not stop", 1 signals "stop at first half", 2 signals "stop at second half"


  //enter refill loop
  DWORD   dwPrevPos = BUFFER_HALF_SIZE * 2;

  while ( ( continuePlaying )
  &&      ( !errorOccured ) ) 
  {
    Sleep( 30 );

    int   start = 0;

    if ( WaitForSingleObject( oggInstance->m_StopPlaybackEvent, 0 ) == WAIT_OBJECT_0 ) 
    {
      break;
    }

    bool    refillNeeded = false;

    {
      if ( !continuePlaying )
      {
        break;
      }

      // Ersatz-Code
      //soundBufferCurrentPosition = pSoundClass->GetPos( dwSoundBufferHandle );

      // Orig-Code
      DWORD   soundBufferCurrentPosition = 0;
      oggInstance->m_pBuffer->GetCurrentPosition( &soundBufferCurrentPosition, NULL );

      DWORD     dwCurFragment = soundBufferCurrentPosition / BUFFER_HALF_SIZE;

      //dh::Log( "CurPos %d", soundBufferCurrentPosition );

      //dh::Log( "CurFrag %d / PrevFrag %d", dwCurFragment, dwPrevPos / BUFFER_HALF_SIZE );

      if ( dwCurFragment != dwPrevPos / BUFFER_HALF_SIZE )
      {
        dwPrevPos = soundBufferCurrentPosition;
        refillNeeded = true;
			  if ( soundBufferCurrentPosition < (DWORD)BUFFER_HALF_SIZE )
        {
				  start = BUFFER_HALF_SIZE;
        }
			  else
        {
				  start = 0;
        }
      }
    }

    //switch(WaitForMultipleObjects((DWORD)stlvEventObjects.size(), &(stlvEventObjects[0]), FALSE, INFINITE)) {
      
    //----- first half was reached

    //case WAIT_OBJECT_0:
    if ( refillNeeded )
    {
      if ( start > 0 )
      {
        //check if we should stop playing back
        if ( nStopAtNextHalf == 1 ) 
        {
          //stop playing
          continuePlaying = false;

          //leave and do not fill the next buffer half
          break;
        }

        //fill second half with sound
        if ( !( oggInstance->Fill( false ) ) )
        {
          errorOccured = true;
        }

        //if the last fill was the final fill, we should stop next time we reach this half (i.e. finish playing whatever audio we do have)
        if ( ( oggInstance->m_PlaybackDone )
        &&   ( !playbackDoneProcessed ) ) 
        {
          //make the while loop stop after playing the next half of the buffer
          nStopAtNextHalf = 1;

          //indicate that we have already processed the playback done flag
          playbackDoneProcessed = true;
        }

      }
      //----- second half was reached
      else
      {
        //check if we should stop playing back
        if ( nStopAtNextHalf == 2 ) 
        {
          //stop playing
          continuePlaying = false;
          //leave and do not fill the next buffer half
          break;
        }

        //fill first half with sound
        if ( !oggInstance->Fill( true ) )
        {
          errorOccured = true;
        }

        //if this last fill was the final fill, we should stop next time we reach this half (i.e. finish playing whatever audio we do have)
        if ( ( oggInstance->m_PlaybackDone ) 
        &&   ( !playbackDoneProcessed ) ) 
        {
          //make the while loop stop after playing the next half of the buffer
          nStopAtNextHalf = 2;

          //indicate that we have already processed the playback done flag
          playbackDoneProcessed = true;
        }
      }
    }
  }
  //----- stop the buffer from playing
  oggInstance->m_pBuffer->Stop();

  //----- perform cleanup
  oggInstance->Cleanup();

  //----- thread no longer active
  oggInstance->SetPlayThreadActive( false );

  //----- done
  return ( errorOccured ? 1 : 0 );
}



bool CDirectSoundOgg::GetPlayThreadActive() 
{
  EnterCriticalSection( &m_CriticalSection );
  bool active = m_PlayThreadActive;
  LeaveCriticalSection( &m_CriticalSection );

  return active;
}



void CDirectSoundOgg::SetPlayThreadActive( bool Active ) 
{
  EnterCriticalSection( &m_CriticalSection );
  m_PlayThreadActive = Active;
  LeaveCriticalSection( &m_CriticalSection );
}



bool CDirectSoundOgg::Initialize( GR::IEnvironment& Environment )
{
  if ( m_pDirectSound )
  {
    return false;
  }
  HRESULT hResult = DirectSoundCreate( NULL, &m_pDirectSound, NULL );

  if ( ( FAILED( hResult ) )
  &&   ( hResult != DSERR_NODRIVER ) )
  {
    dh::Log( "Ogg:Init fail a" );
    return false;
  }

  Xtreme::IAppWindow* pWindowService = ( Xtreme::IAppWindow* )Environment.Service( "Window" );
  HWND      hWnd = NULL;
  if ( pWindowService != NULL )
  {
    hWnd = (HWND)pWindowService->Handle();
  }
  else
  {
    dh::Log( "No Window service found in environment" );
  }
  m_hwndMain = hWnd;
  if ( m_pDirectSound )
  {
    m_pDirectSound->SetCooperativeLevel( m_hwndMain, DSSCL_PRIORITY );
  }
  return true;
}



bool CDirectSoundOgg::Release()
{
  Stop();
  if ( m_pPrimarySoundBuffer )
  {
    m_pPrimarySoundBuffer->Stop();
    m_pPrimarySoundBuffer->Release();
    m_pPrimarySoundBuffer = NULL;
  }
  if ( m_pDirectSound )
  {
    m_pDirectSound->Release();
    m_pDirectSound = NULL;
  }
  return true;
}



bool CDirectSoundOgg::IsInitialized()
{
  return ( m_pBuffer != NULL );
}



int CDirectSoundOgg::Volume()
{
  return m_Volume;
}



bool CDirectSoundOgg::SetVolume( int Volume )
{
  if ( Volume < 0 )
  {
    Volume = 0;
  }
  if ( Volume > 100 )
  {
    Volume = 100;
  }
  m_Volume = Volume;
  if ( m_pBuffer )
  {
    return SUCCEEDED( m_pBuffer->SetVolume( VolumeRange[Volume] ) );
  }
  return true;
}




bool CDirectSoundOgg::LoadMusic( const GR::Char* FileName )
{
  Stop();
  Cleanup();

  m_Filename = FileName;

  if ( !Allocate() )
  {
    return false;
  }
  m_FileLoaded = true;
  return true;
}



bool CDirectSoundOgg::LoadMusic( IIOStream& Stream )
{
  return false;
}



bool CDirectSoundOgg::Resume()
{
  if ( !m_Paused )
  {
    return false;
  }
  m_pBuffer->Play( 0, 0, m_Looping ? DSBPLAY_LOOPING : 0 );

  m_Paused = false;
  return true;
}



bool CDirectSoundOgg::Pause()
{
  if ( m_Paused )
  {
    return true;
  }
  if ( !IsPlaying() )
  {
    return false;
  }
  m_pBuffer->Stop();
  m_Paused = true;

  return true;
}
