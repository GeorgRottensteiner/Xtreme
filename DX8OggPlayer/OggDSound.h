#pragma once

#include <Xtreme/XMusic.h>

#include <Interface/IService.h>


#include  <string>

#include  <windows.h>
#include  <mmsystem.h>

#include  <dsound.h>

#include  <vorbis/codec.h>

#include  <vorbis/vorbisfile.h>



class CDirectSoundOgg  : public XMusic
{

  private:

    GR::String          m_Filename;

    bool                m_FileLoaded;

    bool                m_Looping;

    bool                m_PlaybackDone;

    bool                m_PlayThreadActive;

    LPDIRECTSOUND       m_pDirectSound;

  protected:

    bool                GetPlayThreadActive();

    void                SetPlayThreadActive( bool Active );

    OggVorbis_File      m_VorbisFile;

    LPDIRECTSOUNDBUFFER m_pBuffer;

    LPDIRECTSOUNDBUFFER m_pPrimarySoundBuffer;

    HWND                m_hwndMain;

    bool                m_Paused;

    CRITICAL_SECTION    m_CriticalSection;

    HANDLE              m_PlayThread;
    
    HANDLE              m_StopPlaybackEvent;

    GR::u32             m_Volume;


    bool                Allocate();
    
    void                Cleanup();

    bool                Fill( const bool FirstHalf );

    static unsigned int WINAPI PlayingThread( LPVOID lpParam );

  public:

    CDirectSoundOgg();

    virtual bool            Initialize( GR::IEnvironment& Environment );

    virtual bool            Release();

    virtual bool            IsInitialized();

    virtual bool            SetVolume( int Volume );
    virtual int             Volume();

    virtual bool            LoadMusic( const GR::Char* FileName );
    virtual bool            LoadMusic( IIOStream& Stream );

    virtual ~CDirectSoundOgg();

    GR::String              GetFileName();

    bool                    IsPlaying();

    void                    Stop();

    virtual bool            Play( bool Looped = true );

    virtual bool            Resume();
    virtual bool            Pause();

};
