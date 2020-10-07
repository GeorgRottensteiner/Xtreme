#pragma once

#include <Xtreme/XMusic.h>

#include <Interface/IService.h>

#include <System/ThreadBase.h>



class XMPlayer : public System::ThreadBase,
                 public XMusic
{

  public:

    GR::u32                 m_Volume;

    bool                    m_Looping;


    XMPlayer(void);
    ~XMPlayer(void);


    bool                    Initialize( GR::IEnvironment& Environment );
    bool                    Release();
    bool                    IsInitialized();

    bool                    Play( bool Loop = true );
    void                    Stop();
    bool                    Resume();
    bool                    Pause();

    bool                    IsPlaying();

    bool                    LoadMusic( const GR::Char* FileName );
    bool                    LoadMusic( IIOStream& Stream );
    void                    Unload();

    bool                    SetVolume( int iVolume );
    int                     Volume();

    virtual void            OnServiceGotUnset( const GR::String& Service, GR::IService* pService );
    virtual void            OnServiceGotSet( const GR::String& Service, GR::IService* pService );

  protected:


    virtual int             Run();



};
