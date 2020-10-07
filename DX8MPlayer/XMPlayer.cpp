#include <stdio.h>
#include <malloc.h>

#include <GR/GRTypes.h>

#include <Memory/ByteBuffer.h>

#include <Xtreme/Environment/XWindow.h>

#include <Interface/IIOStream.h>

#include <debug/debugclient.h>

#include ".\xmplayer.h"

#pragma comment ( lib, "dxguid.lib" )
#pragma comment ( lib, "dsound.lib" )
#include <dsound.h>

//#pragma comment ( lib, "winmm.lib" )


#if defined(__arch64__) || defined(__alpha) || defined(__x86_64) || defined(__powerpc64__)
/* 64 bit architectures */

typedef signed char     SBYTE;      /* 1 byte, signed */
typedef unsigned char   UBYTE;      /* 1 byte, unsigned */
typedef signed short    SWORD;      /* 2 bytes, signed */
typedef unsigned short  UWORD;      /* 2 bytes, unsigned */
typedef signed int      SLONG;      /* 4 bytes, signed */
typedef unsigned int    ULONG;      /* 4 bytes, unsigned */
typedef int             BOOL;       /* 0=false, <>0 true */

#else
/* 32 bit architectures */

typedef signed char     SBYTE;      /* 1 byte, signed */
typedef unsigned char   UBYTE;      /* 1 byte, unsigned */
typedef signed short    SWORD;      /* 2 bytes, signed */
typedef unsigned short  UWORD;      /* 2 bytes, unsigned */
typedef signed long     SLONG;      /* 4 bytes, signed */
#if !defined(__OS2__)&&!defined(__EMX__)&&!defined(WIN32)
typedef unsigned long   ULONG;      /* 4 bytes, unsigned */
typedef int             BOOL;       /* 0=false, <>0 true */
#endif
#endif


namespace XMPlay
{

  DWORD     g_dwMMTimerID = 0;

  HWND      g_hwndMain = NULL;

#define BITSHIFT		9
#define REVERBERATION	110000L

#define FRACBITS 11
#define FRACMASK ((1L<<FRACBITS)-1L)

#define TICKLSIZE 8192
#define TICKWSIZE (TICKLSIZE<<1)
#define TICKBSIZE (TICKWSIZE<<1)

#define CLICK_SHIFT  6
#define CLICK_BUFFER (1L<<CLICK_SHIFT)

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif



/*
 *	========== Library version
 */

#define LIBMIKMOD_VERSION_MAJOR 3L
#define LIBMIKMOD_VERSION_MINOR 2L
#define LIBMIKMOD_REVISION      0L

#define LIBMIKMOD_VERSION \
	((LIBMIKMOD_VERSION_MAJOR<<16)| \
	 (LIBMIKMOD_VERSION_MINOR<< 8)| \
	 (LIBMIKMOD_REVISION))

extern long MikMod_GetVersion(void);


/*
 *	========== Platform independent-type definitions
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <mmsystem.h>
#endif

#if defined(__OS2__)||defined(__EMX__)
#define INCL_DOSSEMAPHORES
#include <os2.h>
#else
typedef char CHAR;
#endif



#if defined(__arch64__) || defined(__alpha) || defined(__x86_64) || defined(__powerpc64__)
/* 64 bit architectures */

typedef signed char     SBYTE;      /* 1 byte, signed */
typedef unsigned char   UBYTE;      /* 1 byte, unsigned */
typedef signed short    SWORD;      /* 2 bytes, signed */
typedef unsigned short  UWORD;      /* 2 bytes, unsigned */
typedef signed int      SLONG;      /* 4 bytes, signed */
typedef unsigned int    ULONG;      /* 4 bytes, unsigned */
typedef int             BOOL;       /* 0=false, <>0 true */

#else
/* 32 bit architectures */

typedef signed char     SBYTE;      /* 1 byte, signed */
typedef unsigned char   UBYTE;      /* 1 byte, unsigned */
typedef signed short    SWORD;      /* 2 bytes, signed */
typedef unsigned short  UWORD;      /* 2 bytes, unsigned */
typedef signed long     SLONG;      /* 4 bytes, signed */
#if !defined(__OS2__)&&!defined(__EMX__)&&!defined(WIN32)
typedef unsigned long   ULONG;      /* 4 bytes, unsigned */
typedef int             BOOL;       /* 0=false, <>0 true */
#endif
#endif


static	UWORD vc_mode;

static	SWORD **Samples;
static	long tickleft,samplesthatfit,vc_memory=0;
static	int vc_softchn;
static	GR::i64 idxsize,idxlpos,idxlend;
static	SLONG *vc_tickbuf=NULL;

typedef struct VINFO {
	UBYTE     kick;              /* =1 -> sample has to be restarted */
	UBYTE     active;            /* =1 -> sample is playing */
	UWORD     flags;             /* 16/8 bits looping/one-shot */
	SWORD     handle;            /* identifies the sample */
	ULONG     start;             /* start index */
	ULONG     size;              /* samplesize */
	ULONG     reppos;            /* loop start */
	ULONG     repend;            /* loop end */
	ULONG     frq;               /* current frequency */
	int       vol;               /* current volume */
	int       pan;               /* current panning position */

	int       rampvol;
	int       lvolsel,rvolsel;   /* Volume factor in range 0-255 */
	int       oldlvol,oldrvol;

  GR::i64 current;           /* current index in the sample */
	GR::i64 increment;         /* increment value */
} VINFO;


static	VINFO *vinf=NULL,*vnf;

/*
 *	========== Error codes
 */

enum {
	MMERR_OPENING_FILE = 1,
	MMERR_OUT_OF_MEMORY,
	MMERR_DYNAMIC_LINKING,

	MMERR_SAMPLE_TOO_BIG,
	MMERR_OUT_OF_HANDLES,
	MMERR_UNKNOWN_WAVE_TYPE,

	MMERR_LOADING_PATTERN,
	MMERR_LOADING_TRACK,
	MMERR_LOADING_HEADER,
	MMERR_LOADING_SAMPLEINFO,
	MMERR_NOT_A_MODULE,
	MMERR_NOT_A_STREAM,
	MMERR_MED_SYNTHSAMPLES,
	MMERR_ITPACK_INVALID_DATA,

	MMERR_DETECTING_DEVICE,
	MMERR_INVALID_DEVICE,
	MMERR_INITIALIZING_MIXER,
	MMERR_OPENING_AUDIO,
	MMERR_8BIT_ONLY,
	MMERR_16BIT_ONLY,
	MMERR_STEREO_ONLY,
	MMERR_ULAW,
	MMERR_NON_BLOCK,

	MMERR_AF_AUDIO_PORT,

	MMERR_AIX_CONFIG_INIT,
	MMERR_AIX_CONFIG_CONTROL,
	MMERR_AIX_CONFIG_START,

	MMERR_GUS_SETTINGS,
	MMERR_GUS_RESET,
	MMERR_GUS_TIMER,

	MMERR_HP_SETSAMPLESIZE,
	MMERR_HP_SETSPEED,
	MMERR_HP_CHANNELS,
	MMERR_HP_AUDIO_OUTPUT,
	MMERR_HP_AUDIO_DESC,
	MMERR_HP_BUFFERSIZE,

	MMERR_OSS_SETFRAGMENT,
	MMERR_OSS_SETSAMPLESIZE,
	MMERR_OSS_SETSTEREO,
	MMERR_OSS_SETSPEED,

	MMERR_SGI_SPEED,
	MMERR_SGI_16BIT,
	MMERR_SGI_8BIT,
	MMERR_SGI_STEREO,
	MMERR_SGI_MONO,

	MMERR_SUN_INIT,

	MMERR_OS2_MIXSETUP,
	MMERR_OS2_SEMAPHORE,
	MMERR_OS2_TIMER,
	MMERR_OS2_THREAD,

	MMERR_DS_PRIORITY,
	MMERR_DS_BUFFER,
	MMERR_DS_FORMAT,
	MMERR_DS_NOTIFY,
	MMERR_DS_EVENT,
	MMERR_DS_THREAD,
	MMERR_DS_UPDATE,

	MMERR_WINMM_HANDLE,
	MMERR_WINMM_ALLOCATED,
	MMERR_WINMM_DEVICEID,
	MMERR_WINMM_FORMAT,
	MMERR_WINMM_UNKNOWN,

	MMERR_MAC_SPEED,
	MMERR_MAC_START,

    MMERR_OSX_UNKNOWN_DEVICE,
    MMERR_OSX_BAD_PROPERTY,
    MMERR_OSX_UNSUPPORTED_FORMAT,
    MMERR_OSX_SET_STEREO,
    MMERR_OSX_BUFFER_ALLOC,
    MMERR_OSX_ADD_IO_PROC,
    MMERR_OSX_DEVICE_START,
	MMERR_OSX_PTHREAD,

	MMERR_DOSWSS_STARTDMA,
	MMERR_DOSSB_STARTDMA,

	MMERR_MAX
};

/*
 *	========== Error handling
 */

typedef void (MikMod_handler)(void);
typedef MikMod_handler *MikMod_handler_t;

extern int  MikMod_errno;
extern BOOL MikMod_critical;
extern char *MikMod_strerror(int);

extern MikMod_handler_t MikMod_RegisterErrorHandler(MikMod_handler_t);

/*
 *	========== Library initialization and core functions
 */

struct MDRIVER;

extern void   MikMod_RegisterAllDrivers(void);

CHAR*  MikMod_InfoDriver(void);
void   MikMod_RegisterDriver(struct MDRIVER*);
extern int    MikMod_DriverFromAlias(CHAR*);
extern struct MDRIVER *MikMod_DriverByOrdinal(int);

extern BOOL   MikMod_Init(CHAR*);
extern void   MikMod_Exit(void);
extern BOOL   MikMod_Reset(CHAR*);
extern BOOL   MikMod_SetNumVoices(int,int);
extern BOOL   MikMod_Active(void);
extern BOOL   MikMod_EnableOutput(void);
extern void   MikMod_DisableOutput(void);
extern void   MikMod_Update(void);

extern BOOL   MikMod_InitThreads(void);
extern void   MikMod_Lock(void);
extern void   MikMod_Unlock(void);

extern void*  MikMod_malloc(size_t);
extern void*  MikMod_realloc(void *, size_t);
extern void*  MikMod_calloc(size_t,size_t);
extern void   MikMod_free(void*);

/*
 *	========== Reader, Writer
 */

typedef struct MREADER {
	BOOL (*Seek)(struct MREADER*,long,int);
	long (*Tell)(struct MREADER*);
	BOOL (*Read)(struct MREADER*,void*,size_t);
	int  (*Get)(struct MREADER*);
	BOOL (*Eof)(struct MREADER*);
	long iobase;
	long prev_iobase;
} MREADER;

typedef struct MWRITER {
	BOOL (*Seek)(struct MWRITER*,long,int);
	long (*Tell)(struct MWRITER*);
	BOOL (*Write)(struct MWRITER*,void*,size_t);
	BOOL (*Put)(struct MWRITER*,int);
} MWRITER;

/*
 *	========== Samples
 */

/* Sample playback should not be interrupted */
#define SFX_CRITICAL 1

/* Sample format [loading and in-memory] flags: */
#define SF_16BITS       0x0001
#define SF_STEREO       0x0002
#define SF_SIGNED       0x0004
#define SF_BIG_ENDIAN   0x0008
#define SF_DELTA        0x0010
#define SF_ITPACKED		0x0020

#define	SF_FORMATMASK	0x003F

/* General Playback flags */

#define SF_LOOP         0x0100
#define SF_BIDI         0x0200
#define SF_REVERSE      0x0400
#define SF_SUSTAIN      0x0800

#define SF_PLAYBACKMASK	0x0C00

/* Module-only Playback Flags */

#define SF_OWNPAN		0x1000
#define SF_UST_LOOP     0x2000

#define SF_EXTRAPLAYBACKMASK	0x3000

/* Panning constants */
#define PAN_LEFT		0
#define PAN_HALFLEFT 	64
#define PAN_CENTER		128
#define PAN_HALFRIGHT	192
#define PAN_RIGHT		255
#define PAN_SURROUND	512 /* panning value for Dolby Surround */

typedef struct SAMPLE {
	SWORD  panning;     /* panning (0-255 or PAN_SURROUND) */
	ULONG  speed;       /* Base playing speed/frequency of note */
	UBYTE  volume;      /* volume 0-64 */
	UWORD  inflags;		/* sample format on disk */
	UWORD  flags;       /* sample format in memory */
	ULONG  length;      /* length of sample (in samples!) */
	ULONG  loopstart;   /* repeat position (relative to start, in samples) */
	ULONG  loopend;     /* repeat end */
	ULONG  susbegin;    /* sustain loop begin (in samples) \  Not Supported */
	ULONG  susend;      /* sustain loop end                /      Yet! */

	/* Variables used by the module player only! (ignored for sound effects) */
	UBYTE  globvol;     /* global volume */
	UBYTE  vibflags;    /* autovibrato flag stuffs */
	UBYTE  vibtype;     /* Vibratos moved from INSTRUMENT to SAMPLE */
	UBYTE  vibsweep;
	UBYTE  vibdepth;
	UBYTE  vibrate;
	CHAR*  samplename;  /* name of the sample */

	/* Values used internally only */
	UWORD  avibpos;     /* autovibrato pos [player use] */
	UBYTE  divfactor;   /* for sample scaling, maintains proper period slides */
	ULONG  seekpos;     /* seek position in file */
	SWORD  handle;      /* sample handle used by individual drivers */
	void (*onfree)(void *ctx); /* called from Sample_Free if not NULL */
	void *ctx;			/* context passed to previous function*/
} SAMPLE;

/* Sample functions */

extern SAMPLE *Sample_LoadRaw(CHAR *,ULONG rate, ULONG channel, ULONG flags);
extern SAMPLE *Sample_LoadRawFP(FILE *fp,ULONG rate,ULONG channel, ULONG flags);
extern SAMPLE *Sample_LoadRawMem(const char *buf, int len, ULONG rate, ULONG channel, ULONG flags);
extern SAMPLE *Sample_LoadRawGeneric(MREADER*reader,ULONG rate, ULONG channel, ULONG flags);

extern SAMPLE *Sample_Load(CHAR*);
extern SAMPLE *Sample_LoadFP(FILE*);
extern SAMPLE *Sample_LoadMem(const char *buf, int len);
extern SAMPLE *Sample_LoadGeneric(MREADER*);
extern void   Sample_Free(SAMPLE*);
extern SBYTE  Sample_Play(SAMPLE*,ULONG,UBYTE);

extern void   Voice_SetVolume(SBYTE,UWORD);
extern UWORD  Voice_GetVolume(SBYTE);
extern void   Voice_SetFrequency(SBYTE,ULONG);
extern ULONG  Voice_GetFrequency(SBYTE);
extern void   Voice_SetPanning(SBYTE,ULONG);
extern ULONG  Voice_GetPanning(SBYTE);
extern void   Voice_Play(SBYTE,SAMPLE*,ULONG);
extern void   Voice_Stop(SBYTE);
extern BOOL   Voice_Stopped(SBYTE);
extern SLONG  Voice_GetPosition(SBYTE);
extern ULONG  Voice_RealVolume(SBYTE);

/*
 *	========== Internal module representation (UniMod)
 */

/*
	Instrument definition - for information only, the only field which may be
	of use in user programs is the name field
*/

/* Instrument note count */
#define INSTNOTES 120

/* Envelope point */
typedef struct ENVPT {
	SWORD pos;
	SWORD val;
} ENVPT;

/* Envelope point count */
#define ENVPOINTS 32

/* Instrument structure */
typedef struct INSTRUMENT {
	CHAR* insname;

	UBYTE flags;
	UWORD samplenumber[INSTNOTES];
	UBYTE samplenote[INSTNOTES];

	UBYTE nnatype;
	UBYTE dca;              /* duplicate check action */
	UBYTE dct;              /* duplicate check type */
	UBYTE globvol;
	UWORD volfade;
	SWORD panning;          /* instrument-based panning var */

	UBYTE pitpansep;        /* pitch pan separation (0 to 255) */
	UBYTE pitpancenter;     /* pitch pan center (0 to 119) */
	UBYTE rvolvar;          /* random volume varations (0 - 100%) */
	UBYTE rpanvar;          /* random panning varations (0 - 100%) */

	/* volume envelope */
	UBYTE volflg;           /* bit 0: on 1: sustain 2: loop */
	UBYTE volpts;
	UBYTE volsusbeg;
	UBYTE volsusend;
	UBYTE volbeg;
	UBYTE volend;
	ENVPT volenv[ENVPOINTS];
	/* panning envelope */
	UBYTE panflg;           /* bit 0: on 1: sustain 2: loop */
	UBYTE panpts;
	UBYTE pansusbeg;
	UBYTE pansusend;
	UBYTE panbeg;
	UBYTE panend;
	ENVPT panenv[ENVPOINTS];
	/* pitch envelope */
	UBYTE pitflg;           /* bit 0: on 1: sustain 2: loop */
	UBYTE pitpts;
	UBYTE pitsusbeg;
	UBYTE pitsusend;
	UBYTE pitbeg;
	UBYTE pitend;
	ENVPT pitenv[ENVPOINTS];
} INSTRUMENT;

struct MP_CONTROL;
struct MP_VOICE;

/*
	Module definition
*/

/* maximum master channels supported */
#define UF_MAXCHAN	64

/* Module flags */
#define UF_XMPERIODS	0x0001 /* XM periods / finetuning */
#define UF_LINEAR		0x0002 /* LINEAR periods (UF_XMPERIODS must be set) */
#define UF_INST			0x0004 /* Instruments are used */
#define UF_NNA			0x0008 /* IT: NNA used, set numvoices rather
								  than numchn */
#define UF_S3MSLIDES	0x0010 /* uses old S3M volume slides */
#define UF_BGSLIDES		0x0020 /* continue volume slides in the background */
#define UF_HIGHBPM		0x0040 /* MED: can use >255 bpm */
#define UF_NOWRAP		0x0080 /* XM-type (i.e. illogical) pattern break
								  semantics */
#define UF_ARPMEM		0x0100 /* IT: need arpeggio memory */
#define UF_FT2QUIRKS	0x0200 /* emulate some FT2 replay quirks */
#define UF_PANNING		0x0400 /* module uses panning effects or have
								  non-tracker default initial panning */

typedef struct MODULE {
	/* general module information */
		CHAR*       songname;    /* name of the song */
		CHAR*       modtype;     /* string type of module loaded */
		CHAR*       comment;     /* module comments */

		UWORD       flags;       /* See module flags above */
		UBYTE       numchn;      /* number of module channels */
		UBYTE       numvoices;   /* max # voices used for full NNA playback */
		UWORD       numpos;      /* number of positions in this song */
		UWORD       numpat;      /* number of patterns in this song */
		UWORD       numins;      /* number of instruments */
		UWORD       numsmp;      /* number of samples */
struct  INSTRUMENT* instruments; /* all instruments */
struct  SAMPLE*     samples;     /* all samples */
		UBYTE       realchn;     /* real number of channels used */
		UBYTE       totalchn;    /* total number of channels used (incl NNAs) */

	/* playback settings */
		UWORD       reppos;      /* restart position */
		UBYTE       initspeed;   /* initial song speed */
		UWORD       inittempo;   /* initial song tempo */
		UBYTE       initvolume;  /* initial global volume (0 - 128) */
		UWORD       panning[UF_MAXCHAN]; /* panning positions */
		UBYTE       chanvol[UF_MAXCHAN]; /* channel positions */
		UWORD       bpm;         /* current beats-per-minute speed */
		UWORD       sngspd;      /* current song speed */
		SWORD       volume;      /* song volume (0-128) (or user volume) */

		BOOL        extspd;      /* extended speed flag (default enabled) */
		BOOL        panflag;     /* panning flag (default enabled) */
		BOOL        wrap;        /* wrap module ? (default disabled) */
		BOOL        loop;		 /* allow module to loop ? (default enabled) */
		BOOL        fadeout;	 /* volume fade out during last pattern */

		UWORD       patpos;      /* current row number */
		SWORD       sngpos;      /* current song position */
		ULONG       sngtime;     /* current song time in 2^-10 seconds */

		SWORD       relspd;      /* relative speed factor */

	/* internal module representation */
		UWORD       numtrk;      /* number of tracks */
		UBYTE**     tracks;      /* array of numtrk pointers to tracks */
		UWORD*      patterns;    /* array of Patterns */
		UWORD*      pattrows;    /* array of number of rows for each pattern */
		UWORD*      positions;   /* all positions */

		BOOL        forbid;      /* if true, no player update! */
		UWORD       numrow;      /* number of rows on current pattern */
		UWORD       vbtick;      /* tick counter (counts from 0 to sngspd) */
		UWORD       sngremainder;/* used for song time computation */

struct MP_CONTROL*  control;     /* Effects Channel info (size pf->numchn) */
struct MP_VOICE*    voice;       /* Audio Voice information (size md_numchn) */

		UBYTE       globalslide; /* global volume slide rate */
		UBYTE       pat_repcrazy;/* module has just looped to position -1 */
		UWORD       patbrk;      /* position where to start a new pattern */
		UBYTE       patdly;      /* patterndelay counter (command memory) */
		UBYTE       patdly2;     /* patterndelay counter (real one) */
		SWORD       posjmp;      /* flag to indicate a jump is needed... */
		UWORD		bpmlimit;	 /* threshold to detect bpm or speed values */
} MODULE;


/* This structure is used to query current playing voices status */
typedef struct VOICEINFO {
		INSTRUMENT* i;            /* Current channel instrument */
		SAMPLE*     s;            /* Current channel sample */
		SWORD       panning;      /* panning position */
		SBYTE       volume;       /* channel's "global" volume (0..64) */
		UWORD       period;       /* period to play the sample at */
		UBYTE       kick;         /* if true = sample has been restarted */
} VOICEINFO;

/*
 *	========== Module loaders
 */

struct MLOADER;

extern CHAR*   MikMod_InfoLoader(void);
extern void    MikMod_RegisterAllLoaders(void);
extern void    MikMod_RegisterLoader(struct MLOADER*);

extern struct MLOADER load_669; /* 669 and Extended-669 (by Tran/Renaissance) */
extern struct MLOADER load_amf; /* DMP Advanced Module Format (by Otto Chrons) */
extern struct MLOADER load_asy; /* ASYLUM Music Format 1.0 */
extern struct MLOADER load_dsm; /* DSIK internal module format */
extern struct MLOADER load_far; /* Farandole Composer (by Daniel Potter) */
extern struct MLOADER load_gdm; /* General DigiMusic (by Edward Schlunder) */
extern struct MLOADER load_gt2; /* Graoumf tracker */
extern struct MLOADER load_it;  /* Impulse Tracker (by Jeffrey Lim) */
extern struct MLOADER load_imf; /* Imago Orpheus (by Lutz Roeder) */
extern struct MLOADER load_med; /* Amiga MED modules (by Teijo Kinnunen) */
extern struct MLOADER load_m15; /* Soundtracker 15-instrument */
extern struct MLOADER load_mod; /* Standard 31-instrument Module loader */
extern struct MLOADER load_mtm; /* Multi-Tracker Module (by Renaissance) */
extern struct MLOADER load_okt; /* Amiga Oktalyzer */
extern struct MLOADER load_stm; /* ScreamTracker 2 (by Future Crew) */
extern struct MLOADER load_stx; /* STMIK 0.2 (by Future Crew) */
extern struct MLOADER load_s3m; /* ScreamTracker 3 (by Future Crew) */
extern struct MLOADER load_ult; /* UltraTracker (by MAS) */
extern struct MLOADER load_uni; /* MikMod and APlayer internal module format */
extern struct MLOADER load_xm;  /* FastTracker 2 (by Triton) */

/*
 *	========== Module player
 */

extern MODULE* Player_Load( const CHAR*,int,BOOL);
extern MODULE* Player_LoadFP(FILE*,int,BOOL);
extern MODULE* Player_LoadMem(const char *buffer,int len,int maxchan,BOOL curious);
extern MODULE* Player_LoadGeneric(MREADER*,int,BOOL);
extern CHAR*   Player_LoadTitle(CHAR*);
extern CHAR*   Player_LoadTitleFP(FILE*);
extern CHAR*   Player_LoadTitleMem(const char *buffer,int len);
extern CHAR*   Player_LoadTitleGeneric(MREADER*);

extern void    Player_Free(MODULE*);
extern void    Player_Start(MODULE*);
extern BOOL    Player_Active(void);
extern void    Player_Stop(void);
extern void    Player_TogglePause(void);
extern BOOL    Player_Paused(void);
extern void    Player_NextPosition(void);
extern void    Player_PrevPosition(void);
extern void    Player_SetPosition(UWORD);
extern BOOL    Player_Muted(UBYTE);
extern void    Player_SetVolume(SWORD);
extern MODULE* Player_GetModule(void);
extern void    Player_SetSpeed(UWORD);
extern void    Player_SetTempo(UWORD);
extern void    Player_Unmute(SLONG,...);
extern void    Player_Mute(SLONG,...);
extern void    Player_ToggleMute(SLONG,...);
extern int     Player_GetChannelVoice(UBYTE);
extern UWORD   Player_GetChannelPeriod(UBYTE);
extern int     Player_QueryVoices(UWORD numvoices, VOICEINFO *vinfo);

typedef void (*MikMod_player_t)(void);
typedef void (*MikMod_callback_t)(unsigned char *data, size_t len);

extern MikMod_player_t MikMod_RegisterPlayer(MikMod_player_t);

#define MUTE_EXCLUSIVE  32000
#define MUTE_INCLUSIVE  32001

/*
 *	========== Drivers
 */

enum {
	MD_MUSIC = 0,
	MD_SNDFX
};

enum {
	MD_HARDWARE = 0,
	MD_SOFTWARE
};

/* Mixing flags */

/* These ones take effect only after MikMod_Init or MikMod_Reset */
#define DMODE_16BITS     0x0001 /* enable 16 bit output */
#define DMODE_STEREO     0x0002 /* enable stereo output */
#define DMODE_SOFT_SNDFX 0x0004 /* Process sound effects via software mixer */
#define DMODE_SOFT_MUSIC 0x0008 /* Process music via software mixer */
#define DMODE_HQMIXER    0x0010 /* Use high-quality (slower) software mixer */
#define DMODE_FLOAT      0x0020 /* enable float output */
/* These take effect immediately. */
#define DMODE_SURROUND   0x0100 /* enable surround sound */
#define DMODE_INTERP     0x0200 /* enable interpolation */
#define DMODE_REVERSE    0x0400 /* reverse stereo */
#define DMODE_SIMDMIXER	 0x0800 /* enable SIMD mixing */
#define DMODE_NOISEREDUCTION 0x1000 /* Low pass filtering */

struct SAMPLOAD;
typedef struct MDRIVER {
struct MDRIVER* next;
	CHAR*       Name;
	CHAR*       Version;

	UBYTE       HardVoiceLimit; /* Limit of hardware mixer voices */
	UBYTE       SoftVoiceLimit; /* Limit of software mixer voices */

	CHAR        *Alias;
	CHAR        *CmdLineHelp;

	void        (*CommandLine)      (CHAR*);
	BOOL        (*IsPresent)        (void);
	SWORD       (*SampleLoad)       (struct SAMPLOAD*,int);
	void        (*SampleUnload)     (SWORD);
	ULONG       (*FreeSampleSpace)  (int);
	ULONG       (*RealSampleLength) (int,struct SAMPLE*);
	BOOL        (*Init)             (void);
	void        (*Exit)             (void);
	BOOL        (*Reset)            (void);
	BOOL        (*SetNumVoices)     (void);
	BOOL        (*PlayStart)        (void);
	void        (*PlayStop)         (void);
	void        (*Update)           (void);
	void		(*Pause)			(void);
	void        (*VoiceSetVolume)   (UBYTE,UWORD);
	UWORD       (*VoiceGetVolume)   (UBYTE);
	void        (*VoiceSetFrequency)(UBYTE,ULONG);
	ULONG       (*VoiceGetFrequency)(UBYTE);
	void        (*VoiceSetPanning)  (UBYTE,ULONG);
	ULONG       (*VoiceGetPanning)  (UBYTE);
	void        (*VoicePlay)        (UBYTE,SWORD,ULONG,ULONG,ULONG,ULONG,UWORD);
	void        (*VoiceStop)        (UBYTE);
	BOOL        (*VoiceStopped)     (UBYTE);
	SLONG       (*VoiceGetPosition) (UBYTE);
	ULONG       (*VoiceRealVolume)  (UBYTE);
} MDRIVER;

/* These variables can be changed at ANY time and results will be immediate */
extern UBYTE md_volume;      /* global sound volume (0-128) */
extern UBYTE md_musicvolume; /* volume of song */
extern UBYTE md_sndfxvolume; /* volume of sound effects */
extern UBYTE md_reverb;      /* 0 = none;  15 = chaos */
extern UBYTE md_pansep;      /* 0 = mono;  128 == 100% (full left/right) */

/* The variables below can be changed at any time, but changes will not be
   implemented until MikMod_Reset is called. A call to MikMod_Reset may result
   in a skip or pop in audio (depending on the soundcard driver and the settings
   changed). */
extern UWORD md_device;      /* device */
extern UWORD md_mixfreq;     /* mixing frequency */
extern UWORD md_mode;        /* mode. See DMODE_? flags above */

/* The following variable should not be changed! */
extern MDRIVER* md_driver;   /* Current driver in use. */

/* Known drivers list */

extern struct MDRIVER drv_nos;    /* no sound */
extern struct MDRIVER drv_pipe;   /* piped output */
//extern struct MDRIVER drv_raw;    /* raw file disk writer [music.raw] */
//extern struct MDRIVER drv_stdout; /* output to stdout */
//extern struct MDRIVER drv_wav;    /* RIFF WAVE file disk writer [music.wav] */
extern struct MDRIVER drv_aiff;   /* AIFF file disk writer [music.aiff] */

extern struct MDRIVER drv_ultra;  /* Linux Ultrasound driver */
extern struct MDRIVER drv_sam9407;	/* Linux sam9407 driver */

extern struct MDRIVER drv_AF;     /* Dec Alpha AudioFile */
extern struct MDRIVER drv_aix;    /* AIX audio device */
extern struct MDRIVER drv_alsa;   /* Advanced Linux Sound Architecture (ALSA) */
extern struct MDRIVER drv_esd;    /* Enlightened sound daemon (EsounD) */
extern struct MDRIVER drv_hp;     /* HP-UX audio device */
extern struct MDRIVER drv_nas;    /* Network Audio System (NAS) */
extern struct MDRIVER drv_oss;    /* OpenSound System (Linux,FreeBSD...) */
extern struct MDRIVER drv_sgi;    /* SGI audio library */
extern struct MDRIVER drv_sun;    /* Sun/NetBSD/OpenBSD audio device */

extern struct MDRIVER drv_dart;   /* OS/2 Direct Audio RealTime */
extern struct MDRIVER drv_os2;    /* OS/2 MMPM/2 */

extern struct MDRIVER drv_ds;     /* Win32 DirectSound driver */
extern struct MDRIVER drv_win;    /* Win32 multimedia API driver */

extern struct MDRIVER drv_mac;    /* Macintosh Sound Manager driver */
extern struct MDRIVER drv_osx;	/* MacOS X CoreAudio Driver */

extern struct MDRIVER drv_gp32;   /* GP32 Sound driver */

extern struct MDRIVER drv_wss;    /* DOS WSS driver */
extern struct MDRIVER drv_sb;     /* DOS SB driver */

/* == Virtual channel mixer interface (for user-supplied drivers only) */

extern BOOL  VC_Init(void);
extern void  VC_Exit(void);
extern void  VC_SetCallback(MikMod_callback_t callback);
extern BOOL  VC_SetNumVoices(void);
extern ULONG VC_SampleSpace(int);
extern ULONG VC_SampleLength(int,SAMPLE*);

extern BOOL  VC_PlayStart(void);
extern void  VC_PlayStop(void);

extern SWORD VC_SampleLoad(struct SAMPLOAD*,int);
extern void  VC_SampleUnload(SWORD);

extern ULONG VC_WriteBytes(SBYTE*,ULONG);
extern ULONG VC_SilenceBytes(SBYTE*,ULONG);

extern void  VC_VoiceSetVolume(UBYTE,UWORD);
extern UWORD VC_VoiceGetVolume(UBYTE);
extern void  VC_VoiceSetFrequency(UBYTE,ULONG);
extern ULONG VC_VoiceGetFrequency(UBYTE);
extern void  VC_VoiceSetPanning(UBYTE,ULONG);
extern ULONG VC_VoiceGetPanning(UBYTE);
extern void  VC_VoicePlay(UBYTE,SWORD,ULONG,ULONG,ULONG,ULONG,UWORD);

extern void  VC_VoiceStop(UBYTE);
extern BOOL  VC_VoiceStopped(UBYTE);
extern SLONG VC_VoiceGetPosition(UBYTE);
extern ULONG VC_VoiceRealVolume(UBYTE);


#include <stdarg.h>
#if defined(__OS2__)||defined(__EMX__)||defined(WIN32)
#define strcasecmp(s,t) _stricmp(s,t)
#endif


/*========== More type definitions */

/* SLONGLONG: 64bit, signed */
#if defined (__arch64__) || defined(__alpha)
typedef long		SLONGLONG;
#define NATIVE_64BIT_INT
#elif defined(__powerpc64__)
typedef long long	SLONGLONG;
#define NATIVE_64BIT_INT
#elif defined(__WATCOMC__)
typedef __int64		SLONGLONG;
#elif defined(WIN32) && !defined(__MWERKS__)
typedef __int64	SLONGLONG;
#elif macintosh && !TYPE_LONGLONG
#include <Types.h>
typedef SInt64	    SLONGLONG;
#else
typedef long long	SLONGLONG;
#endif

/*========== Error handling */

#define _mm_errno MikMod_errno
#define _mm_critical MikMod_critical
extern MikMod_handler_t _mm_errorhandler;

/*========== MT stuff */

#ifdef HAVE_PTHREAD
#include <pthread.h>
#define DECLARE_MUTEX(name) \
	extern pthread_mutex_t _mm_mutex_##name
#define MUTEX_LOCK(name)	\
 	pthread_mutex_lock(&_mm_mutex_##name)
#define MUTEX_UNLOCK(name)	\
	pthread_mutex_unlock(&_mm_mutex_##name)
#elif defined(__OS2__)||defined(__EMX__)
#define DECLARE_MUTEX(name)	\
	extern HMTX _mm_mutex_##name
#define MUTEX_LOCK(name)	\
	if(_mm_mutex_##name)	\
		DosRequestMutexSem(_mm_mutex_##name,SEM_INDEFINITE_WAIT)
#define MUTEX_UNLOCK(name)	\
	if(_mm_mutex_##name)	\
		DosReleaseMutexSem(_mm_mutex_##name)
#elif defined(WIN32)
#include <windows.h>
#define DECLARE_MUTEX(name)	\
	extern HANDLE _mm_mutex_##name
#define MUTEX_LOCK(name)	\
	if(_mm_mutex_##name)	\
		WaitForSingleObject(_mm_mutex_##name,INFINITE)
#define MUTEX_UNLOCK(name)	\
	if(_mm_mutex_##name)	\
		ReleaseMutex(_mm_mutex_##name)
#else
#define DECLARE_MUTEX(name)	\
	extern void *_mm_mutex_##name
#define MUTEX_LOCK(name)
#define MUTEX_UNLOCK(name)
#endif

DECLARE_MUTEX(lists);
DECLARE_MUTEX(vars);

/*========== Portable file I/O */

extern MREADER* _mm_new_mem_reader(const void *buffer, int len);
extern void _mm_delete_mem_reader(MREADER *reader);

extern MREADER* _mm_new_file_reader(FILE* fp);
extern void _mm_delete_file_reader(MREADER*);

extern MWRITER* _mm_new_file_writer(FILE *fp);
extern void _mm_delete_file_writer(MWRITER*);

extern BOOL _mm_FileExists(CHAR *fname);

#define _mm_write_SBYTE(x,y) y->Put(y,(int)x)
#define _mm_write_UBYTE(x,y) y->Put(y,(int)x)

#define _mm_read_SBYTE(x) (SBYTE)x->Get(x)
#define _mm_read_UBYTE(x) (UBYTE)x->Get(x)

#define _mm_write_SBYTES(x,y,z) z->Write(z,(void *)x,y)
#define _mm_write_UBYTES(x,y,z) z->Write(z,(void *)x,y)
#define _mm_read_SBYTES(x,y,z)  z->Read(z,(void *)x,y)
#define _mm_read_UBYTES(x,y,z)  z->Read(z,(void *)x,y)

#define _mm_fseek(x,y,z) x->Seek(x,y,z)
#define _mm_ftell(x) x->Tell(x)
#define _mm_rewind(x) _mm_fseek(x,0,SEEK_SET)

#define _mm_eof(x) x->Eof(x)

extern void _mm_iobase_setcur(MREADER*);
extern void _mm_iobase_revert(MREADER*);
extern FILE *_mm_fopen( const CHAR*,CHAR*);
extern int	_mm_fclose(FILE *);
extern void _mm_write_string(CHAR*,MWRITER*);
extern int  _mm_read_string (CHAR*,int,MREADER*);

extern SWORD _mm_read_M_SWORD(MREADER*);
extern SWORD _mm_read_I_SWORD(MREADER*);
extern UWORD _mm_read_M_UWORD(MREADER*);
extern UWORD _mm_read_I_UWORD(MREADER*);

extern SLONG _mm_read_M_SLONG(MREADER*);
extern SLONG _mm_read_I_SLONG(MREADER*);
extern ULONG _mm_read_M_ULONG(MREADER*);
extern ULONG _mm_read_I_ULONG(MREADER*);

extern int _mm_read_M_SWORDS(SWORD*,int,MREADER*);
extern int _mm_read_I_SWORDS(SWORD*,int,MREADER*);
extern int _mm_read_M_UWORDS(UWORD*,int,MREADER*);
extern int _mm_read_I_UWORDS(UWORD*,int,MREADER*);

extern int _mm_read_M_SLONGS(SLONG*,int,MREADER*);
extern int _mm_read_I_SLONGS(SLONG*,int,MREADER*);
extern int _mm_read_M_ULONGS(ULONG*,int,MREADER*);
extern int _mm_read_I_ULONGS(ULONG*,int,MREADER*);

extern void _mm_write_M_SWORD(SWORD,MWRITER*);
extern void _mm_write_I_SWORD(SWORD,MWRITER*);
extern void _mm_write_M_UWORD(UWORD,MWRITER*);
extern void _mm_write_I_UWORD(UWORD,MWRITER*);

extern void _mm_write_M_SLONG(SLONG,MWRITER*);
extern void _mm_write_I_SLONG(SLONG,MWRITER*);
extern void _mm_write_M_ULONG(ULONG,MWRITER*);
extern void _mm_write_I_ULONG(ULONG,MWRITER*);

extern void _mm_write_M_SWORDS(SWORD*,int,MWRITER*);
extern void _mm_write_I_SWORDS(SWORD*,int,MWRITER*);
extern void _mm_write_M_UWORDS(UWORD*,int,MWRITER*);
extern void _mm_write_I_UWORDS(UWORD*,int,MWRITER*);

extern void _mm_write_M_SLONGS(SLONG*,int,MWRITER*);
extern void _mm_write_I_SLONGS(SLONG*,int,MWRITER*);
extern void _mm_write_M_ULONGS(ULONG*,int,MWRITER*);
extern void _mm_write_I_ULONGS(ULONG*,int,MWRITER*);

/*========== Samples */

/* This is a handle of sorts attached to any sample registered with
   SL_RegisterSample.  Generally, this only need be used or changed by the
   loaders and drivers of mikmod. */
typedef struct SAMPLOAD {
	struct SAMPLOAD *next;

	ULONG    length;       /* length of sample (in samples!) */
	ULONG    loopstart;    /* repeat position (relative to start, in samples) */
	ULONG    loopend;      /* repeat end */
	UWORD    infmt,outfmt;
	int      scalefactor;
	SAMPLE*  sample;
	MREADER* reader;
} SAMPLOAD;

/*========== Sample and waves loading interface */

extern void      SL_HalveSample(SAMPLOAD*,int);
extern void      SL_Sample8to16(SAMPLOAD*);
extern void      SL_Sample16to8(SAMPLOAD*);
extern void      SL_SampleSigned(SAMPLOAD*);
extern void      SL_SampleUnsigned(SAMPLOAD*);
extern BOOL      SL_LoadSamples(void);
extern SAMPLOAD* SL_RegisterSample(SAMPLE*,int,MREADER*);
extern BOOL      SL_Load(void*,SAMPLOAD*,ULONG);
extern BOOL      SL_Init(SAMPLOAD*);
extern void      SL_Exit(SAMPLOAD*);

/*========== Internal module representation (UniMod) interface */

/* number of notes in an octave */
#define OCTAVE 12

extern void   UniSetRow(UBYTE*);
extern UBYTE  UniGetByte(void);
extern UWORD  UniGetWord(void);
extern UBYTE* UniFindRow(UBYTE*,UWORD);
extern void   UniSkipOpcode(void);
extern void   UniReset(void);
extern void   UniWriteByte(UBYTE);
extern void   UniWriteWord(UWORD);
extern void   UniNewline(void);
extern UBYTE* UniDup(void);
extern BOOL   UniInit(void);
extern void   UniCleanup(void);
extern void   UniEffect(UWORD,UWORD);
#define UniInstrument(x) UniEffect(UNI_INSTRUMENT,x)
#define UniNote(x)       UniEffect(UNI_NOTE,x)
extern void   UniPTEffect(UBYTE,UBYTE);
extern void   UniVolEffect(UWORD,UBYTE);

/*========== Module Commands */

enum {
	/* Simple note */
	UNI_NOTE = 1,
	/* Instrument change */
	UNI_INSTRUMENT,
	/* Protracker effects */
	UNI_PTEFFECT0,     /* arpeggio */
	UNI_PTEFFECT1,     /* porta up */
	UNI_PTEFFECT2,     /* porta down */
	UNI_PTEFFECT3,     /* porta to note */
	UNI_PTEFFECT4,     /* vibrato */
	UNI_PTEFFECT5,     /* dual effect 3+A */
	UNI_PTEFFECT6,     /* dual effect 4+A */
	UNI_PTEFFECT7,     /* tremolo */
	UNI_PTEFFECT8,     /* pan */
	UNI_PTEFFECT9,     /* sample offset */
	UNI_PTEFFECTA,     /* volume slide */
	UNI_PTEFFECTB,     /* pattern jump */
	UNI_PTEFFECTC,     /* set volume */
	UNI_PTEFFECTD,     /* pattern break */
	UNI_PTEFFECTE,     /* extended effects */
	UNI_PTEFFECTF,     /* set speed */
	/* Scream Tracker effects */
	UNI_S3MEFFECTA,    /* set speed */
	UNI_S3MEFFECTD,    /* volume slide */
	UNI_S3MEFFECTE,    /* porta down */
	UNI_S3MEFFECTF,    /* porta up */
	UNI_S3MEFFECTI,    /* tremor */
	UNI_S3MEFFECTQ,    /* retrig */
	UNI_S3MEFFECTR,    /* tremolo */
	UNI_S3MEFFECTT,    /* set tempo */
	UNI_S3MEFFECTU,    /* fine vibrato */
	UNI_KEYOFF,        /* note off */
	/* Fast Tracker effects */
	UNI_KEYFADE,       /* note fade */
	UNI_VOLEFFECTS,    /* volume column effects */
	UNI_XMEFFECT4,     /* vibrato */
	UNI_XMEFFECT6,     /* dual effect 4+A */
	UNI_XMEFFECTA,     /* volume slide */
	UNI_XMEFFECTE1,    /* fine porta up */
	UNI_XMEFFECTE2,    /* fine porta down */
	UNI_XMEFFECTEA,    /* fine volume slide up */
	UNI_XMEFFECTEB,    /* fine volume slide down */
	UNI_XMEFFECTG,     /* set global volume */
	UNI_XMEFFECTH,     /* global volume slide */
	UNI_XMEFFECTL,     /* set envelope position */
	UNI_XMEFFECTP,     /* pan slide */
	UNI_XMEFFECTX1,    /* extra fine porta up */
	UNI_XMEFFECTX2,    /* extra fine porta down */
	/* Impulse Tracker effects */
	UNI_ITEFFECTG,     /* porta to note */
	UNI_ITEFFECTH,     /* vibrato */
	UNI_ITEFFECTI,     /* tremor (xy not incremented) */
	UNI_ITEFFECTM,     /* set channel volume */
	UNI_ITEFFECTN,     /* slide / fineslide channel volume */
	UNI_ITEFFECTP,     /* slide / fineslide channel panning */
	UNI_ITEFFECTT,     /* slide tempo */
	UNI_ITEFFECTU,     /* fine vibrato */
	UNI_ITEFFECTW,     /* slide / fineslide global volume */
	UNI_ITEFFECTY,     /* panbrello */
	UNI_ITEFFECTZ,     /* resonant filters */
	UNI_ITEFFECTS0,
	/* UltraTracker effects */
	UNI_ULTEFFECT9,    /* Sample fine offset */
	/* OctaMED effects */
	UNI_MEDSPEED,
	UNI_MEDEFFECTF1,   /* play note twice */
	UNI_MEDEFFECTF2,   /* delay note */
	UNI_MEDEFFECTF3,   /* play note three times */
	/* Oktalyzer effects */
	UNI_OKTARP,		   /* arpeggio */

	UNI_LAST
};

extern UWORD unioperands[UNI_LAST];

/* IT / S3M Extended SS effects: */
enum {
	SS_GLISSANDO = 1,
	SS_FINETUNE,
	SS_VIBWAVE,
	SS_TREMWAVE,
	SS_PANWAVE,
	SS_FRAMEDELAY,
	SS_S7EFFECTS,
	SS_PANNING,
	SS_SURROUND,
	SS_HIOFFSET,
	SS_PATLOOP,
	SS_NOTECUT,
	SS_NOTEDELAY,
	SS_PATDELAY
};

/* IT Volume column effects */
enum {
	VOL_VOLUME = 1,
	VOL_PANNING,
	VOL_VOLSLIDE,
	VOL_PITCHSLIDEDN,
	VOL_PITCHSLIDEUP,
	VOL_PORTAMENTO,
	VOL_VIBRATO
};

/* IT resonant filter information */

#define	UF_MAXMACRO		0x10
#define	UF_MAXFILTER	0x100

#define FILT_CUT		0x80
#define FILT_RESONANT	0x81

typedef struct FILTER {
    UBYTE filter,inf;
} FILTER;

/*========== Instruments */

/* Instrument format flags */
#define IF_OWNPAN       1
#define IF_PITCHPAN     2

/* Envelope flags: */
#define EF_ON           1
#define EF_SUSTAIN      2
#define EF_LOOP         4
#define EF_VOLENV       8

/* New Note Action Flags */
#define NNA_CUT         0
#define NNA_CONTINUE    1
#define NNA_OFF         2
#define NNA_FADE        3

#define NNA_MASK        3

#define DCT_OFF         0
#define DCT_NOTE        1
#define DCT_SAMPLE      2
#define DCT_INST        3

#define DCA_CUT         0
#define DCA_OFF         1
#define DCA_FADE        2

#define KEY_KICK        0
#define KEY_OFF         1
#define KEY_FADE        2
#define KEY_KILL        (KEY_OFF|KEY_FADE)

#define KICK_ABSENT     0
#define KICK_NOTE       1
#define KICK_KEYOFF     2
#define KICK_ENV        4

#define AV_IT           1   /* IT vs. XM vibrato info */

/*========== Playing */

#define POS_NONE        (-2)	/* no loop position defined */

#define	LAST_PATTERN	(UWORD)(-1)	/* special ``end of song'' pattern */

typedef struct ENVPR {
	UBYTE  flg;          /* envelope flag */
	UBYTE  pts;          /* number of envelope points */
	UBYTE  susbeg;       /* envelope sustain index begin */
	UBYTE  susend;       /* envelope sustain index end */
	UBYTE  beg;          /* envelope loop begin */
	UBYTE  end;          /* envelope loop end */
	SWORD  p;            /* current envelope counter */
	UWORD  a;            /* envelope index a */
	UWORD  b;            /* envelope index b */
	ENVPT* env;          /* envelope points */
} ENVPR;

typedef struct MP_CHANNEL {
	INSTRUMENT* i;
	SAMPLE*     s;
	UBYTE       sample;       /* which sample number */
	UBYTE       note;         /* the audible note as heard, direct rep of period */
	SWORD       outvolume;    /* output volume (vol + sampcol + instvol) */
	SBYTE       chanvol;      /* channel's "global" volume */
	UWORD       fadevol;      /* fading volume rate */
	SWORD       panning;      /* panning position */
	UBYTE       kick;         /* if true = sample has to be restarted */
	UBYTE       kick_flag;    /* kick has been true */
	UWORD       period;       /* period to play the sample at */
	UBYTE       nna;          /* New note action type + master/slave flags */

	UBYTE       volflg;       /* volume envelope settings */
	UBYTE       panflg;       /* panning envelope settings */
	UBYTE       pitflg;       /* pitch envelope settings */

	UBYTE       keyoff;       /* if true = fade out and stuff */
	SWORD       handle;       /* which sample-handle */
	UBYTE       notedelay;    /* (used for note delay) */
	SLONG       start;        /* The starting byte index in the sample */
} MP_CHANNEL;

typedef struct MP_CONTROL {
	struct MP_CHANNEL	main;

	struct MP_VOICE	*slave;	  /* Audio Slave of current effects control channel */

	UBYTE       slavechn;     /* Audio Slave of current effects control channel */
	UBYTE       muted;        /* if set, channel not played */
	UWORD		ultoffset;    /* fine sample offset memory */
	UBYTE       anote;        /* the note that indexes the audible */
	UBYTE		oldnote;
	SWORD       ownper;
	SWORD       ownvol;
	UBYTE       dca;          /* duplicate check action */
	UBYTE       dct;          /* duplicate check type */
	UBYTE*      row;          /* row currently playing on this channel */
	SBYTE       retrig;       /* retrig value (0 means don't retrig) */
	ULONG       speed;        /* what finetune to use */
	SWORD       volume;       /* amiga volume (0 t/m 64) to play the sample at */

	SWORD       tmpvolume;    /* tmp volume */
	UWORD       tmpperiod;    /* tmp period */
	UWORD       wantedperiod; /* period to slide to (with effect 3 or 5) */

	UBYTE       arpmem;       /* arpeggio command memory */
	UBYTE       pansspd;      /* panslide speed */
	UWORD       slidespeed;
	UWORD       portspeed;    /* noteslide speed (toneportamento) */

	UBYTE       s3mtremor;    /* s3m tremor (effect I) counter */
	UBYTE       s3mtronof;    /* s3m tremor ontime/offtime */
	UBYTE       s3mvolslide;  /* last used volslide */
	SBYTE       sliding;
	UBYTE       s3mrtgspeed;  /* last used retrig speed */
	UBYTE       s3mrtgslide;  /* last used retrig slide */

	UBYTE       glissando;    /* glissando (0 means off) */
	UBYTE       wavecontrol;

	SBYTE       vibpos;       /* current vibrato position */
	UBYTE       vibspd;       /* "" speed */
	UBYTE       vibdepth;     /* "" depth */

	SBYTE       trmpos;       /* current tremolo position */
	UBYTE       trmspd;       /* "" speed */
	UBYTE       trmdepth;     /* "" depth */

	UBYTE       fslideupspd;
	UBYTE       fslidednspd;
	UBYTE       fportupspd;   /* fx E1 (extra fine portamento up) data */
	UBYTE       fportdnspd;   /* fx E2 (extra fine portamento dn) data */
	UBYTE       ffportupspd;  /* fx X1 (extra fine portamento up) data */
	UBYTE       ffportdnspd;  /* fx X2 (extra fine portamento dn) data */

	ULONG       hioffset;     /* last used high order of sample offset */
	UWORD       soffset;      /* last used low order of sample-offset (effect 9) */

	UBYTE       sseffect;     /* last used Sxx effect */
	UBYTE       ssdata;       /* last used Sxx data info */
	UBYTE       chanvolslide; /* last used channel volume slide */

	UBYTE       panbwave;     /* current panbrello waveform */
	UBYTE       panbpos;      /* current panbrello position */
	SBYTE       panbspd;      /* "" speed */
	UBYTE       panbdepth;    /* "" depth */

	UWORD       newsamp;      /* set to 1 upon a sample / inst change */
	UBYTE       voleffect;    /* Volume Column Effect Memory as used by IT */
	UBYTE       voldata;      /* Volume Column Data Memory */

	SWORD       pat_reppos;   /* patternloop position */
	UWORD       pat_repcnt;   /* times to loop */
} MP_CONTROL;

/* Used by NNA only player (audio control.  AUDTMP is used for full effects
   control). */
typedef struct MP_VOICE {
	struct MP_CHANNEL	main;

	ENVPR       venv;
	ENVPR       penv;
	ENVPR       cenv;

	UWORD       avibpos;      /* autovibrato pos */
	UWORD       aswppos;      /* autovibrato sweep pos */

	ULONG       totalvol;     /* total volume of channel (before global mixings) */

	BOOL        mflag;
	SWORD       masterchn;
	UWORD       masterperiod;

	MP_CONTROL* master;       /* index of "master" effects channel */
} MP_VOICE;

/*========== Loaders */

typedef struct MLOADER {
struct MLOADER* next;
	CHAR*       type;
	CHAR*       version;
	BOOL        (*Init)(void);
	BOOL        (*Test)(void);
	BOOL        (*Load)(BOOL);
	void        (*Cleanup)(void);
	CHAR*       (*LoadTitle)(void);
} MLOADER;

/* internal loader variables */
extern MREADER* modreader;
extern UWORD   finetune[16];
extern MODULE  of;                  /* static unimod loading space */
extern UWORD   npertab[7*OCTAVE];   /* used by the original MOD loaders */

extern SBYTE   remap[UF_MAXCHAN];   /* for removing empty channels */
extern UBYTE*  poslookup;           /* lookup table for pattern jumps after
                                      blank pattern removal */
extern UBYTE   poslookupcnt;
extern UWORD*  origpositions;

extern BOOL    filters;             /* resonant filters in use */
extern UBYTE   activemacro;         /* active midi macro number for Sxx */
extern UBYTE   filtermacros[UF_MAXMACRO];    /* midi macro settings */
extern FILTER  filtersettings[UF_MAXFILTER]; /* computed filter settings */

extern int*    noteindex;

/*========== Internal loader interface */

extern BOOL   ReadComment(UWORD);
extern BOOL   ReadLinedComment(UWORD,UWORD);
extern BOOL   AllocPositions(int);
extern BOOL   AllocPatterns(void);
extern BOOL   AllocTracks(void);
extern BOOL   AllocInstruments(void);
extern BOOL   AllocSamples(void);
extern CHAR*  DupStr(CHAR*,UWORD,BOOL);

/* loader utility functions */
extern int*   AllocLinear(void);
extern void   FreeLinear(void);
extern int    speed_to_finetune(ULONG,int);
extern void   S3MIT_ProcessCmd(UBYTE,UBYTE,unsigned int);
extern void   S3MIT_CreateOrders(BOOL);

/* flags for S3MIT_ProcessCmd */
#define	S3MIT_OLDSTYLE	1	/* behave as old scream tracker */
#define	S3MIT_IT		2	/* behave as impulse tracker */
#define	S3MIT_SCREAM	4	/* enforce scream tracker specific limits */

/* used to convert c4spd to linear XM periods (IT and IMF loaders). */
extern UWORD  getlinearperiod(UWORD,ULONG);
extern ULONG  getfrequency(UWORD,ULONG);

/* loader shared data */
#define STM_NTRACKERS 3
extern CHAR *STM_Signatures[STM_NTRACKERS];

/*========== Player interface */

extern BOOL   Player_Init(MODULE*);
extern void   Player_Exit(MODULE*);
extern void   Player_HandleTick(void);

/*========== Drivers */

/* max. number of handles a driver has to provide. (not strict) */
#define MAXSAMPLEHANDLES 384

/* These variables can be changed at ANY time and results will be immediate */
extern UWORD md_bpm;         /* current song / hardware BPM rate */

/* Variables below can be changed via MD_SetNumVoices at any time. However, a
   call to MD_SetNumVoicess while the driver is active will cause the sound to
   skip slightly. */
extern UBYTE md_numchn;      /* number of song + sound effects voices */
extern UBYTE md_sngchn;      /* number of song voices */
extern UBYTE md_sfxchn;      /* number of sound effects voices */
extern UBYTE md_hardchn;     /* number of hardware mixed voices */
extern UBYTE md_softchn;     /* number of software mixed voices */

/* This is for use by the hardware drivers only.  It points to the registered
   tickhandler function. */
extern void (*md_player)(void);

extern SWORD  MD_SampleLoad(SAMPLOAD*,int);
extern void   MD_SampleUnload(SWORD);
extern ULONG  MD_SampleSpace(int);
extern ULONG  MD_SampleLength(int,SAMPLE*);

/* uLaw conversion */
extern void unsignedtoulaw(char *,int);

/* Parameter extraction helper */
extern CHAR  *MD_GetAtom(CHAR*,CHAR*,BOOL);

/* Internal software mixer stuff */
extern void VC_SetupPointers(void);
extern BOOL VC1_Init(void);

#if defined(unix) || defined(__APPLE__) && defined(__MACH__)
/* POSIX helper functions */
extern BOOL MD_Access(CHAR *);
extern BOOL MD_DropPrivileges(void);
#endif

/* Macro to define a missing driver, yet allowing binaries to dynamically link
   with the library without missing symbol errors */
#define MISSING(a) MDRIVER a = { NULL, NULL, NULL, 0, 0 }

/*========== Prototypes for non-MT safe versions of some public functions */

extern void _mm_registerdriver(struct MDRIVER*);
extern void _mm_registerloader(struct MLOADER*);
extern BOOL MikMod_Active_internal(void);
extern void MikMod_DisableOutput_internal(void);
extern BOOL MikMod_EnableOutput_internal(void);
extern void MikMod_Exit_internal(void);
extern BOOL MikMod_SetNumVoices_internal(int,int);
extern void Player_Exit_internal(MODULE*);
extern void Player_Stop_internal(void);
extern BOOL Player_Paused_internal(void);
extern void Sample_Free_internal(SAMPLE*);
extern void Voice_Play_internal(SBYTE,SAMPLE*,ULONG);
extern void Voice_SetFrequency_internal(SBYTE,ULONG);
extern void Voice_SetPanning_internal(SBYTE,ULONG);
extern void Voice_SetVolume_internal(SBYTE,UWORD);
extern void Voice_Stop_internal(SBYTE);
extern BOOL Voice_Stopped_internal(SBYTE);

/*========== SIMD mixing routines */

#if defined(__APPLE__) && defined(__MACH__)
#ifndef HAVE_ALTIVEC
#define HAVE_ALTIVEC
#elif defined WIN32 || defined __WIN64

// FIXME: emmintrin.h requires VC6 processor pack or VC2003+
#ifndef HAVE_SSE2
#define HAVE_SSE2
#endif // HAVE_SSE2
/* Fixes couples warnings */
#pragma warning(disable:4761)
#pragma warning(disable:4391)
#pragma warning(disable:4244)
#endif
#endif
// TODO: Test for GCC Linux

/*========== SIMD mixing helper functions =============*/

#define IS_ALIGNED_16(ptr) (!(((int)(ptr)) & 15))

/* Altivec helper function */
#if defined HAVE_ALTIVEC

#define simd_m128i vector signed int
#define simd_m128 vector float

#include <ppc_intrinsics.h>

// Helper functions

// Set single float across the four values
static inline vector float vec_mul( const vector float a, const vector float b)
{
    return vec_madd(a, b, (const vector float)(0.f));
}

// Set single float across the four values
static inline vector float vec_load_ps1(const float *pF )
{
    vector float data = vec_lde(0, pF);
    return vec_splat(vec_perm(data, data, vec_lvsl(0, pF)), 0);
}

// Set vector to 0
static inline const vector float vec_setzero()
{
    return (const vector float) (0.);
}

static inline vector signed char vec_set1_8(unsigned char splatchar)
{
	vector unsigned char splatmap = vec_lvsl(0, &splatchar);
	vector unsigned char result = vec_lde(0, &splatchar);
	splatmap = vec_splat(splatmap, 0);
	return (vector signed char)vec_perm(result, result, splatmap);
}

#define PERM_A0 0x00,0x01,0x02,0x03
#define PERM_A1 0x04,0x05,0x06,0x07
#define PERM_A2 0x08,0x09,0x0A,0x0B
#define PERM_A3 0x0C,0x0D,0x0E,0x0F
#define PERM_B0 0x10,0x11,0x12,0x13
#define PERM_B1 0x14,0x15,0x16,0x17
#define PERM_B2 0x18,0x19,0x1A,0x1B
#define PERM_B3 0x1C,0x1D,0x1E,0x1F

// Equivalent to _mm_unpacklo_epi32
static inline vector signed int vec_unpacklo(vector signed int a, vector signed int b)
{
   return vec_perm(a, b, (vector unsigned char)(PERM_A0,PERM_A1,PERM_B0,PERM_B1));
}

// Equivalent to _mm_srli_si128(a, 8) (without the zeroing in high part).
static inline vector signed int vec_hiqq(vector signed int a)
{
   vector signed int b = vec_splat_s32(0);
   return vec_perm(a, b, (vector unsigned char)(PERM_A2,PERM_A3,PERM_B2,PERM_B3));
}

// vec_sra is max +15. We have to do in two times ...
#define EXTRACT_SAMPLE_SIMD_0(srce, var) var = vec_sra(vec_sra(vec_ld(0, (vector signed int*)(srce)), vec_splat_u32(15)), vec_splat_u32(BITSHIFT+16-15-0));
#define EXTRACT_SAMPLE_SIMD_8(srce, var) var = vec_sra(vec_sra(vec_ld(0, (vector signed int*)(srce)), vec_splat_u32(15)), vec_splat_u32(BITSHIFT+16-15-8));
#define EXTRACT_SAMPLE_SIMD_16(srce, var) var = vec_sra(vec_ld(0, (vector signed int*)(srce)), vec_splat_u32(BITSHIFT+16-16));
#define PUT_SAMPLE_SIMD_W(dste, v1, v2)  vec_st(vec_packs(v1, v2), 0, dste);
#define PUT_SAMPLE_SIMD_B(dste, v1, v2, v3, v4)  vec_st(vec_add(vec_packs((vector signed short)vec_packs(v1, v2), (vector signed short)vec_packs(v3, v4)), vec_set1_8(128)), 0, dste);
#define PUT_SAMPLE_SIMD_F(dste, v1, v2)  vec_st(vec_mul(vec_ctf(v1, 0), v2), 0, dste);
#define LOAD_PS1_SIMD(ptr) vec_load_ps1(ptr)

#elif defined HAVE_SSE2

/* SSE2 helper function */

#include <emmintrin.h>

static __inline __m128i mm_hiqq(const __m128i a)
{
   return _mm_srli_si128(a, 8); // get the 64bit upper part. new 64bit upper is undefined (zeroed is fine).
}

/* 128-bit mixing macros */
#define EXTRACT_SAMPLE_SIMD(srce, var, size) var = _mm_srai_epi32(_mm_load_si128((__m128i*)(srce)), BITSHIFT+16-size);
#define EXTRACT_SAMPLE_SIMD_0(srce, var) EXTRACT_SAMPLE_SIMD(srce, var, 0)
#define EXTRACT_SAMPLE_SIMD_8(srce, var) EXTRACT_SAMPLE_SIMD(srce, var, 8)
#define EXTRACT_SAMPLE_SIMD_16(srce, var) EXTRACT_SAMPLE_SIMD(srce, var, 16)
#define PUT_SAMPLE_SIMD_W(dste, v1, v2)  _mm_store_si128((__m128i*)(dste), _mm_packs_epi32(v1, v2));
#define PUT_SAMPLE_SIMD_B(dste, v1, v2, v3, v4)  _mm_store_si128((__m128i*)(dste), _mm_add_epi8(_mm_packs_epi16(_mm_packs_epi32(v1, v2), _mm_packs_epi32(v3, v4)), _mm_set1_epi8(128)));
#define PUT_SAMPLE_SIMD_F(dste, v1, v2)  _mm_store_ps((float*)(dste), _mm_mul_ps(_mm_cvtepi32_ps(v1), v2));
#define LOAD_PS1_SIMD(ptr) _mm_load_ps1(ptr)
#define simd_m128i __m128i
#define simd_m128 __m128

#endif

//#ifndef _IN_VIRTCH_

extern BOOL  VC1_Init(void);
static BOOL (*VC_Init_ptr)(void)=VC1_Init;
extern void  VC1_Exit(void);
static void (*VC_Exit_ptr)(void)=VC1_Exit;
extern BOOL  VC1_SetNumVoices(void);
static BOOL (*VC_SetNumVoices_ptr)(void);
extern ULONG VC1_SampleSpace(int);
static ULONG (*VC_SampleSpace_ptr)(int);
extern ULONG VC1_SampleLength(int,SAMPLE*);
static ULONG (*VC_SampleLength_ptr)(int,SAMPLE*);

extern BOOL  VC1_PlayStart(void);
static BOOL (*VC_PlayStart_ptr)(void);
extern void  VC1_PlayStop(void);
static void (*VC_PlayStop_ptr)(void);

extern SWORD VC1_SampleLoad(struct SAMPLOAD*,int);
static SWORD (*VC_SampleLoad_ptr)(struct SAMPLOAD*,int);
extern void  VC1_SampleUnload(SWORD);
static void (*VC_SampleUnload_ptr)(SWORD);

extern ULONG VC1_WriteBytes(SBYTE*,ULONG);
static ULONG (*VC_WriteBytes_ptr)(SBYTE*,ULONG);
extern ULONG VC1_SilenceBytes(SBYTE*,ULONG);
static ULONG (*VC_SilenceBytes_ptr)(SBYTE*,ULONG);

extern void  VC1_VoiceSetVolume(UBYTE,UWORD);
static void (*VC_VoiceSetVolume_ptr)(UBYTE,UWORD);
extern UWORD VC1_VoiceGetVolume(UBYTE);
static UWORD (*VC_VoiceGetVolume_ptr)(UBYTE);
extern void  VC1_VoiceSetFrequency(UBYTE,ULONG);
static void (*VC_VoiceSetFrequency_ptr)(UBYTE,ULONG);
extern ULONG VC1_VoiceGetFrequency(UBYTE);
static ULONG (*VC_VoiceGetFrequency_ptr)(UBYTE);
extern void  VC1_VoiceSetPanning(UBYTE,ULONG);
static void (*VC_VoiceSetPanning_ptr)(UBYTE,ULONG);
extern ULONG VC1_VoiceGetPanning(UBYTE);
static ULONG (*VC_VoiceGetPanning_ptr)(UBYTE);
extern void  VC1_VoicePlay(UBYTE,SWORD,ULONG,ULONG,ULONG,ULONG,UWORD);
static void (*VC_VoicePlay_ptr)(UBYTE,SWORD,ULONG,ULONG,ULONG,ULONG,UWORD);

extern void  VC1_VoiceStop(UBYTE);
static void (*VC_VoiceStop_ptr)(UBYTE);
extern BOOL  VC1_VoiceStopped(UBYTE);
static BOOL (*VC_VoiceStopped_ptr)(UBYTE);
extern SLONG VC1_VoiceGetPosition(UBYTE);
static SLONG (*VC_VoiceGetPosition_ptr)(UBYTE);
extern ULONG VC1_VoiceRealVolume(UBYTE);
static ULONG (*VC_VoiceRealVolume_ptr)(UBYTE);

#if defined __STDC__ || defined _MSC_VER || defined MPW_C
#define VC_PROC0(suffix) \
void VC_##suffix (void) { VC_##suffix##_ptr(); }

#define VC_FUNC0(suffix,ret) \
ret VC_##suffix (void) { return VC_##suffix##_ptr(); }

#define VC_PROC1(suffix,typ1) \
void VC_##suffix (typ1 a) { VC_##suffix##_ptr(a); }

#define VC_FUNC1(suffix,ret,typ1) \
ret VC_##suffix (typ1 a) { return VC_##suffix##_ptr(a); }

#define VC_PROC2(suffix,typ1,typ2) \
void VC_##suffix (typ1 a,typ2 b) { VC_##suffix##_ptr(a,b); }

#define VC_FUNC2(suffix,ret,typ1,typ2) \
ret VC_##suffix (typ1 a,typ2 b) { return VC_##suffix##_ptr(a,b); }
#else
#define VC_PROC0(suffix) \
void VC_/**/suffix (void) { VC_/**/suffix/**/_ptr(); }

#define VC_FUNC0(suffix,ret) \
ret VC_/**/suffix (void) { return VC_/**/suffix/**/_ptr(); }

#define VC_PROC1(suffix,typ1) \
void VC_/**/suffix (typ1 a) { VC_/**/suffix/**/_ptr(a); }

#define VC_FUNC1(suffix,ret,typ1) \
ret VC_/**/suffix (typ1 a) { return VC_/**/suffix/**/_ptr(a); }

#define VC_PROC2(suffix,typ1,typ2) \
void VC_/**/suffix (typ1 a,typ2 b) { VC_/**/suffix/**/_ptr(a,b); }

#define VC_FUNC2(suffix,ret,typ1,typ2) \
ret VC_/**/suffix (typ1 a,typ2 b) { return VC_/**/suffix/**/_ptr(a,b); }
#endif

VC_FUNC0(Init,BOOL)
VC_PROC0(Exit)
VC_FUNC0(SetNumVoices,BOOL)
VC_FUNC1(SampleSpace,ULONG,int)
VC_FUNC2(SampleLength,ULONG,int,SAMPLE*)
VC_FUNC0(PlayStart,BOOL)
VC_PROC0(PlayStop)
VC_FUNC2(SampleLoad,SWORD,struct SAMPLOAD*,int)
VC_PROC1(SampleUnload,SWORD)
VC_FUNC2(WriteBytes,ULONG,SBYTE*,ULONG)
VC_FUNC2(SilenceBytes,ULONG,SBYTE*,ULONG)
VC_PROC2(VoiceSetVolume,UBYTE,UWORD)
VC_FUNC1(VoiceGetVolume,UWORD,UBYTE)
VC_PROC2(VoiceSetFrequency,UBYTE,ULONG)
VC_FUNC1(VoiceGetFrequency,ULONG,UBYTE)
VC_PROC2(VoiceSetPanning,UBYTE,ULONG)
VC_FUNC1(VoiceGetPanning,ULONG,UBYTE)

void  VC_VoicePlay(UBYTE a,SWORD b,ULONG c,ULONG d,ULONG e,ULONG f,UWORD g)
{ VC_VoicePlay_ptr(a,b,c,d,e,f,g); }

VC_PROC1(VoiceStop,UBYTE)
VC_FUNC1(VoiceStopped,BOOL,UBYTE)
VC_FUNC1(VoiceGetPosition,SLONG,UBYTE)
VC_FUNC1(VoiceRealVolume,ULONG,UBYTE)

//#else

static ULONG samples2bytes(ULONG samples)
{
	if(vc_mode & DMODE_FLOAT) samples <<= 2;
	else if(vc_mode & DMODE_16BITS) samples <<= 1;
	if(vc_mode & DMODE_STEREO) samples <<= 1;
	return samples;
}

static ULONG bytes2samples(ULONG bytes)
{
	if(vc_mode & DMODE_FLOAT) bytes >>= 2;
	else if(vc_mode & DMODE_16BITS) bytes >>= 1;
	if(vc_mode & DMODE_STEREO) bytes >>= 1;
	return bytes;
}

/* Fill the buffer with 'todo' bytes of silence (it depends on the mixing mode
   how the buffer is filled) */
ULONG VC1_SilenceBytes(SBYTE* buf,ULONG todo)
{
	todo=samples2bytes(bytes2samples(todo));

	/* clear the buffer to zero (16 bits signed) or 0x80 (8 bits unsigned) */
	if(vc_mode & DMODE_FLOAT)
		memset(buf,0,todo);
	else if(vc_mode & DMODE_16BITS)
		memset(buf,0,todo);
	else
		memset(buf,0x80,todo);

	return todo;
}

void VC1_WriteSamples(SBYTE*,ULONG);

/* Writes 'todo' mixed SBYTES (!!) to 'buf'. It returns the number of SBYTES
   actually written to 'buf' (which is rounded to number of samples that fit
   into 'todo' bytes). */
ULONG VC1_WriteBytes(SBYTE* buf,ULONG todo)
{
	if(!vc_softchn)
		return VC1_SilenceBytes(buf,todo);

	todo = bytes2samples(todo);
	VC1_WriteSamples(buf,todo);

	return samples2bytes(todo);
}

void VC1_Exit(void)
{
	if(vc_tickbuf) MikMod_free(vc_tickbuf);
	if(vinf) MikMod_free(vinf);
	if(Samples) MikMod_free(Samples);

	vc_tickbuf = NULL;
	vinf = NULL;
	Samples = NULL;

	VC_SetupPointers();
}

UWORD VC1_VoiceGetVolume(UBYTE voice)
{
	return (UWORD)vinf[voice].vol;
}

ULONG VC1_VoiceGetPanning(UBYTE voice)
{
	return vinf[voice].pan;
}

void VC1_VoiceSetFrequency(UBYTE voice,ULONG frq)
{
	vinf[voice].frq=frq;
}

ULONG VC1_VoiceGetFrequency(UBYTE voice)
{
	return vinf[voice].frq;
}

void VC1_VoicePlay(UBYTE voice,SWORD handle,ULONG start,ULONG size,ULONG reppos,ULONG repend,UWORD flags)
{
	vinf[voice].flags    = flags;
	vinf[voice].handle   = handle;
	vinf[voice].start    = start;
	vinf[voice].size     = size;
	vinf[voice].reppos   = reppos;
	vinf[voice].repend   = repend;
	vinf[voice].kick     = 1;
}

void VC1_VoiceStop(UBYTE voice)
{
	vinf[voice].active = 0;
}

BOOL VC1_VoiceStopped(UBYTE voice)
{
	return(vinf[voice].active==0);
}

SLONG VC1_VoiceGetPosition(UBYTE voice)
{
	return (SLONG)(vinf[voice].current>>FRACBITS);
}

void VC1_VoiceSetVolume(UBYTE voice,UWORD vol)
{
	/* protect against clicks if volume variation is too high */
	if(abs((int)vinf[voice].vol-(int)vol)>32)
		vinf[voice].rampvol=CLICK_BUFFER;
	vinf[voice].vol=vol;
}

void VC1_VoiceSetPanning(UBYTE voice,ULONG pan)
{
	/* protect against clicks if panning variation is too high */
	if(abs((int)vinf[voice].pan-(int)pan)>48)
		vinf[voice].rampvol=CLICK_BUFFER;
	vinf[voice].pan=pan;
}

/*========== External mixer interface */

void VC1_SampleUnload(SWORD handle)
{
	if (handle<MAXSAMPLEHANDLES) {
		if (Samples[handle])
			MikMod_free(Samples[handle]);
		Samples[handle]=NULL;
	}
}

SWORD VC1_SampleLoad(struct SAMPLOAD* sload,int type)
{
	SAMPLE *s = sload->sample;
	int handle;
	ULONG t, length,loopstart,loopend;

	if(type==MD_HARDWARE) return -1;

	/* Find empty slot to put sample address in */
	for(handle=0;handle<MAXSAMPLEHANDLES;handle++)
		if(!Samples[handle]) break;

	if(handle==MAXSAMPLEHANDLES) {
		_mm_errno = MMERR_OUT_OF_HANDLES;
		return -1;
	}

	/* Reality check for loop settings */
	if (s->loopend > s->length)
		s->loopend = s->length;
	if (s->loopstart >= s->loopend)
		s->flags &= ~SF_LOOP;

	length    = s->length;
	loopstart = s->loopstart;
	loopend   = s->loopend;

	SL_SampleSigned(sload);
	SL_Sample8to16(sload);

	if(!(Samples[handle]=(SWORD*)MikMod_malloc((length+20)<<1))) {
		_mm_errno = MMERR_SAMPLE_TOO_BIG;
		return -1;
	}

  memset( Samples[handle], 0, ( length + 20 ) << 1 );

	/* read sample into buffer */
	if (SL_Load(Samples[handle],sload,length))
		return -1;

	/* Unclick sample */
	if(s->flags & SF_LOOP) {
		if(s->flags & SF_BIDI)
			for(t=0;t<16;t++)
				Samples[handle][loopend+t]=Samples[handle][(loopend-t)-1];
		else
			for(t=0;t<16;t++)
				Samples[handle][loopend+t]=Samples[handle][t+loopstart];
	} else
		for(t=0;t<16;t++)
			Samples[handle][t+length]=0;

	return (SWORD)handle;
}

ULONG VC1_SampleSpace(int )
{
	return vc_memory;
}

ULONG VC1_SampleLength(int ,SAMPLE* s)
{
	if (!s) return 0;

	return (s->length*((s->flags&SF_16BITS)?2:1))+16;
}

ULONG VC1_VoiceRealVolume(UBYTE voice)
{
	ULONG i,s,size;
	int k,j;
	SWORD *smp;
	SLONG t;

	t = (SLONG)(vinf[voice].current>>FRACBITS);
	if(!vinf[voice].active) return 0;

	s = vinf[voice].handle;
	size = vinf[voice].size;

	i=64; t-=64; k=0; j=0;
	if(i>size) i = size;
	if(t<0) t = 0;
	if(t+i > size) t = size-i;

	i &= ~1;  /* make sure it's EVEN. */

	smp = &Samples[s][t];
	for(;i;i--,smp++) {
		if(k<*smp) k = *smp;
		if(j>*smp) j = *smp;
	}
	return abs(k-j);
}


MikMod_callback_t vc_callback;

//#endif




/* Reverb control variables */

static	int RVc1, RVc2, RVc3, RVc4, RVc5, RVc6, RVc7, RVc8;
static	ULONG RVRindex;

/* For Mono or Left Channel */
static	SLONG *RVbufL1=NULL,*RVbufL2=NULL,*RVbufL3=NULL,*RVbufL4=NULL,
		      *RVbufL5=NULL,*RVbufL6=NULL,*RVbufL7=NULL,*RVbufL8=NULL;

/* For Stereo only (Right Channel) */
static	SLONG *RVbufR1=NULL,*RVbufR2=NULL,*RVbufR3=NULL,*RVbufR4=NULL,
		      *RVbufR5=NULL,*RVbufR6=NULL,*RVbufR7=NULL,*RVbufR8=NULL;

#ifdef NATIVE_64BIT_INT
#define NATIVE SLONGLONG
#else
#define NATIVE SLONG
#endif
#if defined HAVE_SSE2 || defined HAVE_ALTIVEC

static size_t MixSIMDMonoNormal(const SWORD* srce,SLONG* dest,size_t index, size_t increment,size_t todo)
{
	// TODO:
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;

	while(todo--) {
		sample = srce[index >> FRACBITS];
		index += increment;

		*dest++ += lvolsel * sample;
	}
	return index;
}

static size_t MixSIMDStereoNormal(const SWORD* srce, SLONG* dest, size_t index, size_t increment,size_t todo)
{
	SWORD vol[8] = {vnf->lvolsel, vnf->rvolsel};
	SWORD sample;
	SLONG remain = todo;

	// Dest can be misaligned ...
	while(!IS_ALIGNED_16(dest)) {
		sample=srce[(index += increment) >> FRACBITS];
		*dest++ += vol[0] * sample;
		*dest++ += vol[1] * sample;
		todo--;
	}

	// Srce is always aligned ...

#if defined HAVE_SSE2
	remain = todo&3;
	{
		__m128i v0 = _mm_set_epi16(0, vol[1],
								   0, vol[0],
								   0, vol[1],
								   0, vol[0]);
		for(todo>>=2;todo; todo--)
		{
			SWORD s0 = srce[(index += increment) >> FRACBITS];
			SWORD s1 = srce[(index += increment) >> FRACBITS];
			SWORD s2 = srce[(index += increment) >> FRACBITS];
			SWORD s3 = srce[(index += increment) >> FRACBITS];
			__m128i v1 = _mm_set_epi16(0, s1, 0, s1, 0, s0, 0, s0);
			__m128i v2 = _mm_set_epi16(0, s3, 0, s3, 0, s2, 0, s2);
			__m128i v3 = _mm_load_si128((__m128i*)(dest+0));
			__m128i v4 = _mm_load_si128((__m128i*)(dest+4));
			_mm_store_si128((__m128i*)(dest+0), _mm_add_epi32(v3, _mm_madd_epi16(v0, v1)));
			_mm_store_si128((__m128i*)(dest+4), _mm_add_epi32(v4, _mm_madd_epi16(v0, v2)));
			dest+=8;
		}
	}

#elif defined HAVE_ALTIVEC
	remain = todo&3;
	{
		vector signed short r0 = vec_ld(0, vol);
		vector signed short v0 = vec_perm(r0, r0, (vector unsigned char)(0, 1, // l
																		 0, 1, // l
																		 2, 3, // r
																		 2, 1, // r
																		 0, 1, // l
																		 0, 1, // l
																		 2, 3, // r
																		 2, 3 // r
																		 ));
		SWORD s[8];

		for(todo>>=2;todo; todo--)
		{
			// Load constants
			s[0] = srce[(index += increment) >> FRACBITS];
			s[1] = srce[(index += increment) >> FRACBITS];
			s[2] = srce[(index += increment) >> FRACBITS];
			s[3] = srce[(index += increment) >> FRACBITS];
			s[4] = 0;

			vector short int r1 = vec_ld(0, s);
			vector signed short v1 = vec_perm(r1, r1, (vector unsigned char)(0*2, 0*2+1, // s0
																			 4*2, 4*2+1, // 0
																			 0*2, 0*2+1, // s0
																			 4*2, 4*2+1, // 0
																			 1*2, 1*2+1, // s1
																			 4*2, 4*2+1, // 0
																			 1*2, 1*2+1, // s1
																			 4*2, 4*2+1  // 0
																			 ));

			vector signed short v2 = vec_perm(r1, r1, (vector unsigned char)(2*2, 2*2+1, // s2
																			 4*2, 4*2+1, // 0
																			 2*2, 2*2+1, // s2
																			 4*2, 4*2+1, // 0
																			 3*2, 3*2+1, // s3
																			 4*2, 4*2+1, // 0
																			 3*2, 3*2+1, // s3
																			 4*2, 4*2+1  // 0
																			 ));
			vector signed int v3 = vec_ld(0, dest);
			vector signed int v4 = vec_ld(0, dest + 4);
			vector signed int v5 = vec_mule(v0, v1);
			vector signed int v6 = vec_mule(v0, v2);

			vec_st(vec_add(v3, v5), 0, dest);
			vec_st(vec_add(v4, v6), 0, dest + 4);

			dest+=8;
		}
	}
#endif // HAVE_ALTIVEC

	// Remaining bits ...
	while(remain--) {
		sample=srce[(index += increment) >> FRACBITS];

		*dest++ += vol[0] * sample;
		*dest++ += vol[1] * sample;
	}
	return index;
}
#endif

/*========== 32 bit sample mixers - only for 32 bit platforms */
#ifndef NATIVE_64BIT_INT

static SLONG Mix32MonoNormal(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;

	while(todo--) {
		sample = srce[index >> FRACBITS];
		index += increment;

		*dest++ += lvolsel * sample;
	}
	return index;
}

// FIXME: This mixer should works also on 64-bit platform
// Hint : changes SLONG / SLONGLONG mess with size_t
static SLONG Mix32StereoNormal(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
#if defined HAVE_ALTIVEC || defined HAVE_SSE2
    if (md_mode & DMODE_SIMDMIXER)
    {
		return MixSIMDStereoNormal(srce, dest, index, increment, todo);
	}
	else
#endif
	{
		SWORD sample;
		SLONG lvolsel = vnf->lvolsel;
		SLONG rvolsel = vnf->rvolsel;
		while(todo--) {
			sample=srce[(index += increment) >> FRACBITS];

			*dest++ += lvolsel * sample;
			*dest++ += rvolsel * sample;
		}
	}
	return index;
}


static SLONG Mix32SurroundNormal(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;

	if (lvolsel>=rvolsel) {
		while(todo--) {
			sample = srce[index >> FRACBITS];
			index += increment;

			*dest++ += lvolsel*sample;
			*dest++ -= lvolsel*sample;
		}
	} else {
		while(todo--) {
			sample = srce[index >> FRACBITS];
			index += increment;

			*dest++ -= rvolsel*sample;
			*dest++ += rvolsel*sample;
		}
	}
	return index;
}

static SLONG Mix32MonoInterp(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rampvol = vnf->rampvol;

	if (rampvol) {
		SLONG oldlvol = vnf->oldlvol - lvolsel;
		while(todo--) {
			sample=(SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS);
			index += increment;

			*dest++ += ((lvolsel << CLICK_SHIFT) + oldlvol * rampvol)
			           * sample >> CLICK_SHIFT;
			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS);
		index += increment;

		*dest++ += lvolsel * sample;
	}
	return index;
}

static SLONG Mix32StereoInterp(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;
	SLONG rampvol = vnf->rampvol;

	if (rampvol) {
		SLONG oldlvol = vnf->oldlvol - lvolsel;
		SLONG oldrvol = vnf->oldrvol - rvolsel;
		while(todo--) {
			sample=(SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS);
			index += increment;

			*dest++ += ((lvolsel << CLICK_SHIFT) + oldlvol * rampvol)
			           * sample >> CLICK_SHIFT;
			*dest++ += ((rvolsel << CLICK_SHIFT) + oldrvol * rampvol)
					   * sample >> CLICK_SHIFT;
			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS);
		index += increment;

		*dest++ += lvolsel * sample;
		*dest++ += rvolsel * sample;
	}
	return index;
}

static SLONG Mix32SurroundInterp(const SWORD* srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;
	SLONG rampvol = vnf->rampvol;
	SLONG oldvol, vol;

	if (lvolsel >= rvolsel) {
		vol = lvolsel;
		oldvol = vnf->oldlvol;
	} else {
		vol = rvolsel;
		oldvol = vnf->oldrvol;
	}

	if (rampvol) {
		oldvol -= vol;
		while(todo--) {
			sample=(SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS);
			index += increment;

			sample=((vol << CLICK_SHIFT) + oldvol * rampvol)
				   * sample >> CLICK_SHIFT;
			*dest++ += sample;
			*dest++ -= sample;

			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS);
		index += increment;

		*dest++ += vol*sample;
		*dest++ -= vol*sample;
	}
	return index;
}
#endif

/*========== 64 bit sample mixers - all platforms */

static SLONGLONG MixMonoNormal(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;

	while(todo--) {
		sample = srce[index >> FRACBITS];
		index += increment;

		*dest++ += lvolsel * sample;
	}
	return index;
}

static SLONGLONG MixStereoNormal(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;

	while(todo--) {
		sample=srce[index >> FRACBITS];
		index += increment;

		*dest++ += lvolsel * sample;
		*dest++ += rvolsel * sample;
	}
	return index;
}

static SLONGLONG MixSurroundNormal(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SWORD sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;

	if(vnf->lvolsel>=vnf->rvolsel) {
		while(todo--) {
			sample = srce[index >> FRACBITS];
			index += increment;

			*dest++ += lvolsel*sample;
			*dest++ -= lvolsel*sample;
		}
	} else {
		while(todo--) {
			sample = srce[index >> FRACBITS];
			index += increment;

			*dest++ -= rvolsel*sample;
			*dest++ += rvolsel*sample;
		}
	}
	return index;
}

static SLONGLONG MixMonoInterp(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rampvol = vnf->rampvol;

	if (rampvol) {
		SLONG oldlvol = vnf->oldlvol - lvolsel;
		while(todo--) {
			sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS) );
			index += increment;

			*dest++ += ((lvolsel << CLICK_SHIFT) + oldlvol * rampvol)
					   * sample >> CLICK_SHIFT;
			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS) );
		index += increment;

		*dest++ += lvolsel * sample;
	}
	return index;
}

static SLONGLONG MixStereoInterp(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;
	SLONG rampvol = vnf->rampvol;

	if (rampvol) {
		SLONG oldlvol = vnf->oldlvol - lvolsel;
		SLONG oldrvol = vnf->oldrvol - rvolsel;
		while(todo--) {
			sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS) );
			index += increment;

			*dest++ +=((lvolsel << CLICK_SHIFT) + oldlvol * rampvol)
					   * sample >> CLICK_SHIFT;
			*dest++ +=((rvolsel << CLICK_SHIFT) + oldrvol * rampvol)
					   * sample >> CLICK_SHIFT;
			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS) );
		index += increment;

		*dest++ += lvolsel * sample;
		*dest++ += rvolsel * sample;
	}
	return index;
}

static SLONGLONG MixSurroundInterp(const SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
	SLONG sample;
	SLONG lvolsel = vnf->lvolsel;
	SLONG rvolsel = vnf->rvolsel;
	SLONG rampvol = vnf->rampvol;
	SLONG oldvol, vol;

	if (lvolsel >= rvolsel) {
		vol = lvolsel;
		oldvol = vnf->oldlvol;
	} else {
		vol = rvolsel;
		oldvol = vnf->oldrvol;
	}

	if (rampvol) {
		oldvol -= vol;
		while(todo--) {
			sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
			       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
			        *(index&FRACMASK)>>FRACBITS) );
			index += increment;

			sample=((vol << CLICK_SHIFT) + oldvol * rampvol)
				   * sample >> CLICK_SHIFT;
			*dest++ += sample;
			*dest++ -= sample;
			if (!--rampvol)
				break;
		}
		vnf->rampvol = rampvol;
		if (todo < 0)
			return index;
	}

	while(todo--) {
		sample=(SLONG)( (SLONG)srce[index>>FRACBITS]+
		       ((SLONG)(srce[(index>>FRACBITS)+1]-srce[index>>FRACBITS])
		        *(index&FRACMASK)>>FRACBITS) );
		index += increment;

		*dest++ += vol*sample;
		*dest++ -= vol*sample;
	}
	return index;
}

static void (*MixReverb)(SLONG* srce,NATIVE count);

/* Reverb macros */
#define COMPUTE_LOC(n) loc##n = RVRindex % RVc##n
#define COMPUTE_LECHO(n) RVbufL##n [loc##n ]=speedup+((ReverbPct*RVbufL##n [loc##n ])>>7)
#define COMPUTE_RECHO(n) RVbufR##n [loc##n ]=speedup+((ReverbPct*RVbufR##n [loc##n ])>>7)

static void MixReverb_Normal(SLONG* srce,NATIVE count)
{
	unsigned int speedup;
	int ReverbPct;
	unsigned int loc1,loc2,loc3,loc4;
	unsigned int loc5,loc6,loc7,loc8;

	ReverbPct=58+(md_reverb<<2);

	COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
	COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

	while(count--) {
		/* Compute the left channel echo buffers */
		speedup = *srce >> 3;

		COMPUTE_LECHO(1); COMPUTE_LECHO(2); COMPUTE_LECHO(3); COMPUTE_LECHO(4);
		COMPUTE_LECHO(5); COMPUTE_LECHO(6); COMPUTE_LECHO(7); COMPUTE_LECHO(8);

		/* Prepare to compute actual finalized data */
		RVRindex++;

		COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
		COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

		/* left channel */
		*srce++ +=RVbufL1[loc1]-RVbufL2[loc2]+RVbufL3[loc3]-RVbufL4[loc4]+
		          RVbufL5[loc5]-RVbufL6[loc6]+RVbufL7[loc7]-RVbufL8[loc8];
	}
}

static void MixReverb_Stereo(SLONG* srce,NATIVE count)
{
	unsigned int speedup;
	int          ReverbPct;
	unsigned int loc1, loc2, loc3, loc4;
	unsigned int loc5, loc6, loc7, loc8;

	ReverbPct = 92+(md_reverb<<1);

	COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
	COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

	while(count--) {
		/* Compute the left channel echo buffers */
		speedup = *srce >> 3;

		COMPUTE_LECHO(1); COMPUTE_LECHO(2); COMPUTE_LECHO(3); COMPUTE_LECHO(4);
		COMPUTE_LECHO(5); COMPUTE_LECHO(6); COMPUTE_LECHO(7); COMPUTE_LECHO(8);

		/* Compute the right channel echo buffers */
		speedup = srce[1] >> 3;

		COMPUTE_RECHO(1); COMPUTE_RECHO(2); COMPUTE_RECHO(3); COMPUTE_RECHO(4);
		COMPUTE_RECHO(5); COMPUTE_RECHO(6); COMPUTE_RECHO(7); COMPUTE_RECHO(8);

		/* Prepare to compute actual finalized data */
		RVRindex++;

		COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
		COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

		/* left channel then right channel */
		*srce++ +=RVbufL1[loc1]-RVbufL2[loc2]+RVbufL3[loc3]-RVbufL4[loc4]+
		          RVbufL5[loc5]-RVbufL6[loc6]+RVbufL7[loc7]-RVbufL8[loc8];

		*srce++ +=RVbufR1[loc1]-RVbufR2[loc2]+RVbufR3[loc3]-RVbufR4[loc4]+
		          RVbufR5[loc5]-RVbufR6[loc6]+RVbufR7[loc7]-RVbufR8[loc8];
	}
}

static void (*MixLowPass)(SLONG* srce,NATIVE count);

static int nLeftNR, nRightNR;

static void MixLowPass_Stereo(SLONG* srce,NATIVE count)
{
	int n1 = nLeftNR, n2 = nRightNR;
	SLONG *pnr = srce;
	int nr=count;
	for (; nr; nr--)
	{
		int vnr = pnr[0] >> 1;
		pnr[0] = vnr + n1;
		n1 = vnr;
		vnr = pnr[1] >> 1;
		pnr[1] = vnr + n2;
		n2 = vnr;
		pnr += 2;
	}
	nLeftNR = n1;
	nRightNR = n2;
}

static void MixLowPass_Normal(SLONG* srce,NATIVE count)
{
	int n1 = nLeftNR;
	SLONG *pnr = srce;
	int nr=count;
	for (; nr; nr--)
	{
		int vnr = pnr[0] >> 1;
		pnr[0] = vnr + n1;
		n1 = vnr;
		pnr ++;
	}
	nLeftNR = n1;
}

/* Mixing macros */
#define EXTRACT_SAMPLE_FP(var,size) var=(*srce++>>(BITSHIFT-(int)size)) * ((1.0f / 32768.0f) / (1 << (int)size))
#define CHECK_SAMPLE_FP(var,bound) var=(var>bound)?bound:(var<-bound)?-bound:var
#define PUT_SAMPLE_FP(var) *dste++=var

static void Mix32ToFP(float* dste,const SLONG *srce,NATIVE count)
{
	float x1,x2,x3,x4;
	int	remain;

	#define FP_SHIFT	4

	remain=count&3;
	for(count>>=2;count;count--) {
		EXTRACT_SAMPLE_FP(x1,FP_SHIFT); EXTRACT_SAMPLE_FP(x2,FP_SHIFT);
		EXTRACT_SAMPLE_FP(x3,FP_SHIFT); EXTRACT_SAMPLE_FP(x4,FP_SHIFT);

		CHECK_SAMPLE_FP(x1,1.0f); CHECK_SAMPLE_FP(x2,1.0f);
		CHECK_SAMPLE_FP(x3,1.0f); CHECK_SAMPLE_FP(x4,1.0f);

		PUT_SAMPLE_FP(x1); PUT_SAMPLE_FP(x2);
		PUT_SAMPLE_FP(x3); PUT_SAMPLE_FP(x4);
	}
	while(remain--) {
		EXTRACT_SAMPLE_FP(x1,FP_SHIFT);
		CHECK_SAMPLE_FP(x1,1.0f);
		PUT_SAMPLE_FP(x1);
	}
}


/* Mixing macros */
#define EXTRACT_SAMPLE(var,size) var=*srce++>>(BITSHIFT+16-size)
#define CHECK_SAMPLE(var,bound) var=(var>=bound)?bound-1:(var<-bound)?-bound:var
#define PUT_SAMPLE(var) *dste++=var

static void Mix32To16(SWORD* dste,const SLONG *srce,NATIVE count)
{
	SLONG x1,x2,x3,x4;
	int	remain;

	remain=count&3;
	for(count>>=2;count;count--) {
		EXTRACT_SAMPLE(x1,16); EXTRACT_SAMPLE(x2,16);
		EXTRACT_SAMPLE(x3,16); EXTRACT_SAMPLE(x4,16);

		CHECK_SAMPLE(x1,32768); CHECK_SAMPLE(x2,32768);
		CHECK_SAMPLE(x3,32768); CHECK_SAMPLE(x4,32768);

		PUT_SAMPLE((SWORD)x1); PUT_SAMPLE((SWORD)x2); PUT_SAMPLE((SWORD)x3); PUT_SAMPLE((SWORD)x4);
	}
	while(remain--) {
		EXTRACT_SAMPLE(x1,16);
		CHECK_SAMPLE(x1,32768);
		PUT_SAMPLE((SWORD)x1);
	}
}

static void Mix32To8(SBYTE* dste,const SLONG *srce,NATIVE count)
{
	SWORD x1,x2,x3,x4;
	int	remain;

	remain=count&3;
	for(count>>=2;count;count--)
  {
    x1 = (SWORD)( *srce++ >> ( BITSHIFT + 16 - 8 ) );
    x2 = (SWORD)( *srce++ >> ( BITSHIFT + 16 - 8 ) );
    x3 = (SWORD)( *srce++ >> ( BITSHIFT + 16 - 8 ) );
    x4 = (SWORD)( *srce++ >> ( BITSHIFT + 16 - 8 ) );

		CHECK_SAMPLE(x1,128); CHECK_SAMPLE(x2,128);
		CHECK_SAMPLE(x3,128); CHECK_SAMPLE(x4,128);

		PUT_SAMPLE( (SBYTE)( x1+128 ) ); PUT_SAMPLE( (SBYTE)( x2+128 ) );
		PUT_SAMPLE( (SBYTE)( x3+128 ) ); PUT_SAMPLE( (SBYTE)( x4+128 ) );
	}
	while(remain--)
  {
    x1 = (SWORD)( *srce++ >> ( BITSHIFT + 16 - 8 ) );
		CHECK_SAMPLE(x1,128);
		PUT_SAMPLE( (SBYTE)( x1 + 128 ) );
	}
}

#if defined HAVE_ALTIVEC || defined HAVE_SSE2

// Mix 32bit input to floating point. 32 samples per iteration
// PC: ?
static void Mix32ToFP_SIMD(float* dste,SLONG* srce,NATIVE count)
{
	int	remain=count&3;

	const float k = (1.0f / (float)(1<<24));
	simd_m128i x1;
	simd_m128 x2 = LOAD_PS1_SIMD(&k); // Scale factor
	remain = count&3;
	for(count>>=2;count;count--) {
	   EXTRACT_SAMPLE_SIMD_0(srce, x1);  // Load 32 samples
	PUT_SAMPLE_SIMD_F(dste, x1, x2); // Store 32 samples
	srce+=4;
	dste+=4;
	}

	if (remain)
	   Mix32ToFP(dste, srce, remain);
}
// PC: Ok, Mac Ok
static void Mix32To16_SIMD(SWORD* dste,SLONG* srce,NATIVE count)
{
	int	remain = count;

	while(!IS_ALIGNED_16(dste) || !IS_ALIGNED_16(srce))
	{
		SLONG x1;
		EXTRACT_SAMPLE(x1,16);
		CHECK_SAMPLE(x1,32768);
		PUT_SAMPLE(x1);
		count--;
		if (!count)
		{
			return;
		}
	}

	remain = count&7;

	for(count>>=3;count;count--)
	{
		simd_m128i x1,x2;
		EXTRACT_SAMPLE_SIMD_16(srce, x1);  // Load 4 samples
		EXTRACT_SAMPLE_SIMD_16(srce+4, x2);  // Load 4 samples
		PUT_SAMPLE_SIMD_W(dste, x1, x2);  // Store 8 samples
		srce+=8;
		dste+=8;
	}

	if (remain)
	   Mix32To16(dste, srce, remain);
}

// Mix 32bit input to 8bit. 128 samples per iteration
// PC:OK, Mac: Ok
static void Mix32To8_SIMD(SBYTE* dste,SLONG* srce,NATIVE count)
{
	int	remain=count;

	while(!IS_ALIGNED_16(dste) || !IS_ALIGNED_16(srce))
	{
		SWORD x1;
		EXTRACT_SAMPLE(x1,8);
		CHECK_SAMPLE(x1,128);
		PUT_SAMPLE(x1+128);
		count--;
		if (!count)
		{
			return;
		}
	}

	remain = count&15;

	for(count>>=4;count;count--) {
	simd_m128i x1,x2,x3,x4;
	EXTRACT_SAMPLE_SIMD_8(srce, x1);  // Load 4 samples
	EXTRACT_SAMPLE_SIMD_8(srce+4, x2);  // Load 4 samples
	EXTRACT_SAMPLE_SIMD_8(srce+8, x3);  // Load 4 samples
	EXTRACT_SAMPLE_SIMD_8(srce+12, x4);  // Load 4 samples
	PUT_SAMPLE_SIMD_B(dste, x1, x2, x3, x4); // Store 16 samples
	srce+=16;
	dste+=16;
	}
	if (remain)
	   Mix32To8(dste, srce, remain);
}

#endif



static void AddChannel(SLONG* ptr,NATIVE todo)
{
	SLONGLONG end,done;
	SWORD *s;

	if(!(s=Samples[vnf->handle])) {
		vnf->current = vnf->active  = 0;
		return;
	}

	/* update the 'current' index so the sample loops, or stops playing if it
	   reached the end of the sample */
	while(todo>0) {
		SLONGLONG endpos;

		if(vnf->flags & SF_REVERSE) {
			/* The sample is playing in reverse */
			if((vnf->flags&SF_LOOP)&&(vnf->current<idxlpos)) {
				/* the sample is looping and has reached the loopstart index */
				if(vnf->flags & SF_BIDI) {
					/* sample is doing bidirectional loops, so 'bounce' the
					   current index against the idxlpos */
					vnf->current = idxlpos+(idxlpos-vnf->current);
					vnf->flags &= ~SF_REVERSE;
					vnf->increment = -vnf->increment;
				} else
					/* normal backwards looping, so set the current position to
					   loopend index */
					vnf->current=idxlend-(idxlpos-vnf->current);
			} else {
				/* the sample is not looping, so check if it reached index 0 */
				if(vnf->current < 0) {
					/* playing index reached 0, so stop playing this sample */
					vnf->current = vnf->active  = 0;
					break;
				}
			}
		} else {
			/* The sample is playing forward */
			if((vnf->flags & SF_LOOP) &&
			   (vnf->current >= idxlend)) {
				/* the sample is looping, check the loopend index */
				if(vnf->flags & SF_BIDI) {
					/* sample is doing bidirectional loops, so 'bounce' the
					   current index against the idxlend */
					vnf->flags |= SF_REVERSE;
					vnf->increment = -vnf->increment;
					vnf->current = idxlend-(vnf->current-idxlend);
				} else
					/* normal backwards looping, so set the current position
					   to loopend index */
					vnf->current=idxlpos+(vnf->current-idxlend);
			} else {
				/* sample is not looping, so check if it reached the last
				   position */
				if(vnf->current >= idxsize) {
					/* yes, so stop playing this sample */
					vnf->current = vnf->active  = 0;
					break;
				}
			}
		}

		end=(vnf->flags&SF_REVERSE)?(vnf->flags&SF_LOOP)?idxlpos:0:
		     (vnf->flags&SF_LOOP)?idxlend:idxsize;

		/* if the sample is not blocked... */
		if((end==vnf->current)||(!vnf->increment))
			done=0;
		else {
			done=MIN((end-vnf->current)/vnf->increment+1,todo);
			if(done<0) done=0;
		}

		if(!done) {
			vnf->active = 0;
			break;
		}

		endpos=vnf->current+done*vnf->increment;

		if(vnf->vol) {
#ifndef NATIVE_64BIT_INT
			/* use the 32 bit mixers as often as we can (they're much faster) */
			if((vnf->current<0x7fffffff)&&(endpos<0x7fffffff)) {
				if((md_mode & DMODE_INTERP)) {
					if(vc_mode & DMODE_STEREO) {
						if((vnf->pan==PAN_SURROUND)&&(md_mode&DMODE_SURROUND))
							vnf->current=Mix32SurroundInterp(s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
						else
							vnf->current=Mix32StereoInterp(s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					} else
						vnf->current=Mix32MonoInterp
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
				} else if(vc_mode & DMODE_STEREO) {
					if((vnf->pan==PAN_SURROUND)&&(md_mode&DMODE_SURROUND))
						vnf->current=Mix32SurroundNormal
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					else
					{
#if defined HAVE_ALTIVEC || defined HAVE_SSE2
					if (md_mode & DMODE_SIMDMIXER)
						vnf->current=MixSIMDStereoNormal
						               (s,ptr,vnf->current,vnf->increment,done);

					else
#endif
						vnf->current=Mix32StereoNormal
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					}
				} else
					vnf->current=Mix32MonoNormal
					                   (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
			} else
#endif
			       {
				if((md_mode & DMODE_INTERP)) {
					if(vc_mode & DMODE_STEREO) {
						if((vnf->pan==PAN_SURROUND)&&(md_mode&DMODE_SURROUND))
							vnf->current=MixSurroundInterp
							           (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
						else
							vnf->current=MixStereoInterp
							           (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					} else
						vnf->current=MixMonoInterp
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
				} else if(vc_mode & DMODE_STEREO) {
					if((vnf->pan==PAN_SURROUND)&&(md_mode&DMODE_SURROUND))
						vnf->current=MixSurroundNormal
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					else
					{
#if defined HAVE_ALTIVEC || defined HAVE_SSE2
					if (md_mode & DMODE_SIMDMIXER)
						vnf->current=MixSIMDStereoNormal
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);

					else
#endif
						vnf->current=MixStereoNormal
						               (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
					}
				} else
					vnf->current=MixMonoNormal
					                   (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
			}
		} else
			/* update sample position */
			vnf->current=endpos;

		todo-=(SLONG)done;
		ptr +=(vc_mode & DMODE_STEREO)?(done<<1):done;
	}
}

void VC1_WriteSamples(SBYTE* buf,ULONG todo)
{
	int left,portion=0,count;
	SBYTE  *buffer;
	int t, pan, vol;

	while(todo) {
		if(!tickleft) {
			if(vc_mode & DMODE_SOFT_MUSIC) md_player();
			tickleft=(md_mixfreq*125L)/(md_bpm*50L);
		}
		left = MIN((ULONG)tickleft, todo);
		buffer    = buf;
		tickleft -= left;
		todo     -= left;
		buf += samples2bytes(left);

		while(left) {
			portion = MIN(left, samplesthatfit);
			count   = (vc_mode & DMODE_STEREO)?(portion<<1):portion;
			memset(vc_tickbuf, 0, count<<2);
			for(t=0;t<vc_softchn;t++) {
				vnf = &vinf[t];

				if(vnf->kick) {
					vnf->current=((SLONGLONG)vnf->start)<<FRACBITS;
					vnf->kick   =0;
					vnf->active =1;
				}

				if(!vnf->frq) vnf->active = 0;

				if(vnf->active) {
					vnf->increment=((SLONGLONG)(vnf->frq<<FRACBITS))/md_mixfreq;
					if(vnf->flags&SF_REVERSE) vnf->increment=-vnf->increment;
					vol = vnf->vol;  pan = vnf->pan;

					vnf->oldlvol=vnf->lvolsel;vnf->oldrvol=vnf->rvolsel;
					if(vc_mode & DMODE_STEREO) {
						if(pan != PAN_SURROUND) {
							vnf->lvolsel=(vol*(PAN_RIGHT-pan))>>8;
							vnf->rvolsel=(vol*pan)>>8;
						} else
							vnf->lvolsel=vnf->rvolsel=vol/2;
					} else
						vnf->lvolsel=vol;

					idxsize = (vnf->size)? ((SLONGLONG)vnf->size << FRACBITS)-1 : 0;
					idxlend = (vnf->repend)? ((SLONGLONG)vnf->repend << FRACBITS)-1 : 0;
					idxlpos = (SLONGLONG)vnf->reppos << FRACBITS;
					AddChannel(vc_tickbuf, portion);
				}
			}

			if(md_mode & DMODE_NOISEREDUCTION) {
				MixLowPass(vc_tickbuf, portion);
			}

			if(md_reverb) {
				if(md_reverb>15) md_reverb=15;
				MixReverb(vc_tickbuf, portion);
			}

			if (vc_callback) {
				vc_callback((unsigned char*)vc_tickbuf, portion);
			}


#if defined HAVE_ALTIVEC || defined HAVE_SSE2
			if (md_mode & DMODE_SIMDMIXER)
			{
				if(vc_mode & DMODE_FLOAT)
					Mix32ToFP_SIMD((float*) buffer, vc_tickbuf, count);
				else if(vc_mode & DMODE_16BITS)
					Mix32To16_SIMD((SWORD*) buffer, vc_tickbuf, count);
				else
					Mix32To8_SIMD((SBYTE*) buffer, vc_tickbuf, count);
			}
			else
#endif
			{
				if(vc_mode & DMODE_FLOAT)
					Mix32ToFP((float*) buffer, vc_tickbuf, count);
				else if(vc_mode & DMODE_16BITS)
					Mix32To16((SWORD*) buffer, vc_tickbuf, count);
				else
					Mix32To8((SBYTE*) buffer, vc_tickbuf, count);
			}
			buffer += samples2bytes(portion);
			left   -= portion;
		}
	}
}

namespace VC2
{
  BOOL VC2_Init();
};


BOOL VC1_Init(void)
{
	VC_SetupPointers();

	if (md_mode&DMODE_HQMIXER)
    return VC2::VC2_Init();

	if(!(Samples=(SWORD**)MikMod_calloc(MAXSAMPLEHANDLES,sizeof(SWORD*)))) {
		_mm_errno = MMERR_INITIALIZING_MIXER;
		return 1;
	}
	if(!vc_tickbuf)
		if(!(vc_tickbuf=(SLONG*)MikMod_malloc((TICKLSIZE+32)*sizeof(SLONG)))) {
			_mm_errno = MMERR_INITIALIZING_MIXER;
			return 1;
		}

	MixReverb=(md_mode&DMODE_STEREO)?MixReverb_Stereo:MixReverb_Normal;
	MixLowPass=(md_mode&DMODE_STEREO)?MixLowPass_Stereo:MixLowPass_Normal;
	vc_mode = md_mode;
	return 0;
}

BOOL VC1_PlayStart(void)
{
	samplesthatfit=TICKLSIZE;
	if(vc_mode & DMODE_STEREO) samplesthatfit >>= 1;
	tickleft = 0;

	RVc1 = (5000L * md_mixfreq) / REVERBERATION;
	RVc2 = (5078L * md_mixfreq) / REVERBERATION;
	RVc3 = (5313L * md_mixfreq) / REVERBERATION;
	RVc4 = (5703L * md_mixfreq) / REVERBERATION;
	RVc5 = (6250L * md_mixfreq) / REVERBERATION;
	RVc6 = (6953L * md_mixfreq) / REVERBERATION;
	RVc7 = (7813L * md_mixfreq) / REVERBERATION;
	RVc8 = (8828L * md_mixfreq) / REVERBERATION;

	if(!(RVbufL1=(SLONG*)MikMod_calloc((RVc1+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL2=(SLONG*)MikMod_calloc((RVc2+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL3=(SLONG*)MikMod_calloc((RVc3+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL4=(SLONG*)MikMod_calloc((RVc4+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL5=(SLONG*)MikMod_calloc((RVc5+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL6=(SLONG*)MikMod_calloc((RVc6+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL7=(SLONG*)MikMod_calloc((RVc7+1),sizeof(SLONG)))) return 1;
	if(!(RVbufL8=(SLONG*)MikMod_calloc((RVc8+1),sizeof(SLONG)))) return 1;

	if(!(RVbufR1=(SLONG*)MikMod_calloc((RVc1+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR2=(SLONG*)MikMod_calloc((RVc2+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR3=(SLONG*)MikMod_calloc((RVc3+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR4=(SLONG*)MikMod_calloc((RVc4+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR5=(SLONG*)MikMod_calloc((RVc5+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR6=(SLONG*)MikMod_calloc((RVc6+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR7=(SLONG*)MikMod_calloc((RVc7+1),sizeof(SLONG)))) return 1;
	if(!(RVbufR8=(SLONG*)MikMod_calloc((RVc8+1),sizeof(SLONG)))) return 1;

	RVRindex = 0;
	return 0;
}

void VC1_PlayStop(void)
{
	if(RVbufL1) MikMod_free(RVbufL1);
	if(RVbufL2) MikMod_free(RVbufL2);
	if(RVbufL3) MikMod_free(RVbufL3);
	if(RVbufL4) MikMod_free(RVbufL4);
	if(RVbufL5) MikMod_free(RVbufL5);
	if(RVbufL6) MikMod_free(RVbufL6);
	if(RVbufL7) MikMod_free(RVbufL7);
	if(RVbufL8) MikMod_free(RVbufL8);
	RVbufL1=RVbufL2=RVbufL3=RVbufL4=RVbufL5=RVbufL6=RVbufL7=RVbufL8=NULL;
	if(RVbufR1) MikMod_free(RVbufR1);
	if(RVbufR2) MikMod_free(RVbufR2);
	if(RVbufR3) MikMod_free(RVbufR3);
	if(RVbufR4) MikMod_free(RVbufR4);
	if(RVbufR5) MikMod_free(RVbufR5);
	if(RVbufR6) MikMod_free(RVbufR6);
	if(RVbufR7) MikMod_free(RVbufR7);
	if(RVbufR8) MikMod_free(RVbufR8);
	RVbufR1=RVbufR2=RVbufR3=RVbufR4=RVbufR5=RVbufR6=RVbufR7=RVbufR8=NULL;
}

BOOL VC1_SetNumVoices(void)
{
	int t;

	if(!(vc_softchn=md_softchn)) return 0;

	if(vinf) MikMod_free(vinf);
	if(!(vinf= (VINFO*)MikMod_calloc(sizeof(VINFO),vc_softchn))) return 1;

	for(t=0;t<vc_softchn;t++) {
		vinf[t].frq=10000;
		vinf[t].pan=(t&1)?PAN_LEFT:PAN_RIGHT;
	}

	return 0;
}


static	int sl_rlength;
static	SWORD sl_old;
static	SWORD *sl_buffer=NULL;
static	SAMPLOAD *musiclist=NULL,*sndfxlist=NULL;

/* size of the loader buffer in words */
#define SLBUFSIZE 2048

/* IT-Compressed status structure */
typedef struct ITPACK {
	UWORD bits;    /* current number of bits */
	UWORD bufbits; /* bits in buffer */
	SWORD last;    /* last output */
	UBYTE buf;     /* bit buffer */
} ITPACK;

BOOL SL_Init(SAMPLOAD* s)
{
	if(!sl_buffer)
  {
		if(!(sl_buffer=(SWORD*)MikMod_malloc(SLBUFSIZE*sizeof(SWORD)))) return 0;

    memset( sl_buffer, 0, SLBUFSIZE * sizeof( SWORD ) );
  }

	sl_rlength = s->length;
	if(s->infmt & SF_16BITS) sl_rlength>>=1;
	sl_old = 0;

	return 1;
}

void SL_Exit(SAMPLOAD *s)
{
	if(sl_rlength>0) _mm_fseek(s->reader,sl_rlength,SEEK_CUR);
	if(sl_buffer) {
		MikMod_free(sl_buffer);
		sl_buffer=NULL;
	}
}

/* unpack a 8bit IT packed sample */
static BOOL read_itcompr8(ITPACK* status,MREADER *reader,SWORD *sl_buffer,UWORD count,UWORD* incnt)
{
	SWORD *dest=sl_buffer,*end=sl_buffer+count;
	UWORD x,y,needbits,havebits,new_count=0;
	UWORD bits = status->bits;
	UWORD bufbits = status->bufbits;
	SBYTE last = (SBYTE)status->last;
	UBYTE buf = status->buf;

	while (dest<end) {
		needbits=new_count?3:bits;
		x=havebits=0;
		while (needbits) {
			/* feed buffer */
			if (!bufbits) {
				if((*incnt)--)
					buf=_mm_read_UBYTE(reader);
				else
					buf=0;
				bufbits=8;
			}
			/* get as many bits as necessary */
			y = needbits<bufbits?needbits:bufbits;
			x|= (buf & ((1<<y)- 1))<<havebits;
			buf>>=y;
      bufbits = bufbits - y;
      needbits = needbits - y;
      havebits = havebits + y;
		}
		if (new_count) {
			new_count = 0;
			if (++x >= bits)
				x++;
			bits = x;
			continue;
		}
		if (bits<7) {
			if (x==(1<<(bits-1))) {
				new_count = 1;
				continue;
			}
		}
		else if (bits<9) {
			y = (0xff >> (9-bits)) - 4;
			if ((x>y)&&(x<=y+8)) {
				if ( (x = x - y)>=bits)
					x++;
				bits = x;
				continue;
			}
		}
		else if (bits<10) {
			if (x>=0x100) {
				bits=x-0x100+1;
				continue;
			}
		} else {
			/* error in compressed data... */
			_mm_errno=MMERR_ITPACK_INVALID_DATA;
			return 0;
		}

		if (bits<8) /* extend sign */
			x = ((SBYTE)(x <<(8-bits))) >> (8-bits);

    last = (SBYTE)( last + x );

		*(dest++)= (SWORD)( last << 8 ); /* convert to 16 bit */
	}
	status->bits = bits;
	status->bufbits = bufbits;
	status->last = last;
	status->buf = buf;
	return !!(dest-sl_buffer);
}

/* unpack a 16bit IT packed sample */
static BOOL read_itcompr16(ITPACK *status,MREADER *reader,SWORD *sl_buffer,UWORD count,UWORD* incnt)
{
	SWORD *dest=sl_buffer,*end=sl_buffer+count;
	SLONG x,y,needbits,havebits,new_count=0;
	UWORD bits = status->bits;
	UWORD bufbits = status->bufbits;
	SWORD last = status->last;
	UBYTE buf = status->buf;

	while (dest<end) {
		needbits=new_count?4:bits;
		x=havebits=0;
		while (needbits) {
			/* feed buffer */
			if (!bufbits) {
				if((*incnt)--)
					buf=_mm_read_UBYTE(reader);
				else
					buf=0;
				bufbits=8;
			}
			/* get as many bits as necessary */
			y=needbits<bufbits?needbits:bufbits;
			x|=(buf &((1<<y)-1))<<havebits;
			buf>>=y;
      bufbits = (UWORD)( bufbits - y );
      needbits = needbits - y;
      havebits = havebits + y;
		}
		if (new_count) {
			new_count = 0;
			if (++x >= bits)
				x++;
			bits = (UWORD)x;
			continue;
		}
		if (bits<7) {
			if (x==(1<<(bits-1))) {
				new_count=1;
				continue;
			}
		}
		else if (bits<17) {
			y=(0xffff>>(17-bits))-8;
			if ((x>y)&&(x<=y+16)) {
				if ((x-=y)>=bits)
					x++;
				bits = (UWORD)x;
				continue;
			}
		}
		else if (bits<18) {
			if (x>=0x10000) {
				bits=(UWORD)(x-0x10000+1);
				continue;
			}
		} else {
			 /* error in compressed data... */
			_mm_errno=MMERR_ITPACK_INVALID_DATA;
			return 0;
		}

		if (bits<16) /* extend sign */
			x = ((SWORD)(x<<(16-bits)))>>(16-bits);

    last = last + (SWORD)x;

		*(dest++)= last;
	}
	status->bits = bits;
	status->bufbits = bufbits;
	status->last = last;
	status->buf = buf;
	return !!(dest-sl_buffer);
}

static BOOL SL_LoadInternal(void* buffer,UWORD infmt,UWORD outfmt,int scalefactor,ULONG length,MREADER* reader,BOOL dither)
{
	SBYTE *bptr = (SBYTE*)buffer;
	SWORD *wptr = (SWORD*)buffer;
	int stodo,t,u;

	int result,c_block=0;	/* compression bytes until next block */
	ITPACK status;
	UWORD incnt;

	while(length) {
		stodo=(length<SLBUFSIZE)?length:SLBUFSIZE;

		if(infmt&SF_ITPACKED) {
			sl_rlength=0;
			if (!c_block) {
				status.bits = (infmt & SF_16BITS) ? 17 : 9;
				status.last = status.bufbits = 0;
				incnt=_mm_read_I_UWORD(reader);
				c_block = (infmt & SF_16BITS) ? 0x4000 : 0x8000;
				if(infmt&SF_DELTA) sl_old=0;
			}
			if (infmt & SF_16BITS) {
				if(!(result=read_itcompr16(&status,reader,sl_buffer, (UWORD)stodo,&incnt)))
					return 1;
			} else {
				if(!(result=read_itcompr8(&status,reader,sl_buffer,(UWORD)stodo,&incnt)))
					return 1;
			}
			if(result!=stodo) {
				_mm_errno=MMERR_ITPACK_INVALID_DATA;
				return 1;
			}
			c_block -= stodo;
		} else {
			if(infmt&SF_16BITS) {
				if(infmt&SF_BIG_ENDIAN)
					_mm_read_M_SWORDS(sl_buffer,stodo,reader);
				else
					_mm_read_I_SWORDS(sl_buffer,stodo,reader);
			} else {
				SBYTE *src;
				SWORD *dest;

				reader->Read(reader,sl_buffer,sizeof(SBYTE)*stodo);
				src = (SBYTE*)sl_buffer;
				dest  = sl_buffer;
				src += stodo;dest += stodo;

				for(t=0;t<stodo;t++) {
					src--;dest--;
					*dest = (*src)<<8;
				}
			}
			sl_rlength-=stodo;
		}

		if(infmt & SF_DELTA)
			for(t=0;t<stodo;t++)
      {
        sl_buffer[t] = sl_buffer[t] + sl_old;
				sl_old = sl_buffer[t];
			}

		if((infmt^outfmt) & SF_SIGNED)
			for(t=0;t<stodo;t++)
				sl_buffer[t]^= 0x8000;

		if(scalefactor) {
			int idx = 0;
			SLONG scaleval;

			/* Sample Scaling... average values for better results. */
			t= 0;
			while(t<stodo && length) {
				scaleval = 0;
				for(u=scalefactor;u && t<stodo;u--,t++)
					scaleval+=sl_buffer[t];
				sl_buffer[idx++]=(UWORD)(scaleval/(scalefactor-u));
				length--;
			}
			stodo = idx;
		} else
			length -= stodo;

		if (dither) {
			if((infmt & SF_STEREO) && !(outfmt & SF_STEREO)) {
				/* dither stereo to mono, average together every two samples */
				SLONG avgval;
				int idx = 0;

				t=0;
				while(t<stodo && length) {
					avgval=sl_buffer[t++];
					avgval+=sl_buffer[t++];
					sl_buffer[idx++]=(SWORD)(avgval>>1);
					length-=2;
				}
				stodo = idx;
			}
		}

		if(outfmt & SF_16BITS) {
			for(t=0;t<stodo;t++)
				*(wptr++)=sl_buffer[t];
		} else {
			for(t=0;t<stodo;t++)
				*(bptr++)= (SBYTE)( sl_buffer[t]>>8 );
		}
	}
	return 0;
}

BOOL SL_Load(void* buffer,SAMPLOAD *smp,ULONG length)
{
	return SL_LoadInternal(buffer,smp->infmt,smp->outfmt,smp->scalefactor,
	                       length,smp->reader,0);
}

/* Registers a sample for loading when SL_LoadSamples() is called. */
SAMPLOAD* SL_RegisterSample(SAMPLE* s,int type,MREADER* reader)
{
	SAMPLOAD *news,**samplist,*cruise;

	if(type==MD_MUSIC) {
		samplist = &musiclist;
		cruise = musiclist;
	} else
	  if (type==MD_SNDFX) {
		samplist = &sndfxlist;
		cruise = sndfxlist;
	} else
		return NULL;

	/* Allocate and add structure to the END of the list */
	if(!(news=(SAMPLOAD*)MikMod_malloc(sizeof(SAMPLOAD)))) return NULL;

	if(cruise) {
		while(cruise->next) cruise=cruise->next;
		cruise->next = news;
	} else
		*samplist = news;

	news->infmt     = s->flags & SF_FORMATMASK;
	news->outfmt    = news->infmt;
	news->reader    = reader;
	news->sample    = s;
	news->length    = s->length;
	news->loopstart = s->loopstart;
	news->loopend   = s->loopend;

	return news;
}

static void FreeSampleList(SAMPLOAD* s)
{
	SAMPLOAD *old;

	while(s) {
		old = s;
		s = s->next;
		MikMod_free(old);
	}
}

/* Returns the total amount of memory required by the samplelist queue. */
static ULONG SampleTotal(SAMPLOAD* samplist,int type)
{
	int total = 0;

	while(samplist) {
		samplist->sample->flags=
		  (samplist->sample->flags&~SF_FORMATMASK)|samplist->outfmt;
		total += MD_SampleLength(type,samplist->sample);
		samplist=samplist->next;
	}

	return total;
}

static ULONG RealSpeed(SAMPLOAD *s)
{
	return(s->sample->speed/(s->scalefactor?s->scalefactor:1));
}

static BOOL DitherSamples(SAMPLOAD* samplist,int type)
{
	SAMPLOAD *c2smp=NULL;
	ULONG maxsize, speed;
	SAMPLOAD *s;

	if(!samplist) return 0;

	if((maxsize=MD_SampleSpace(type)*1024))
		while(SampleTotal(samplist,type)>maxsize) {
			/* First Pass - check for any 16 bit samples */
			s = samplist;
			while(s) {
				if(s->outfmt & SF_16BITS) {
					SL_Sample16to8(s);
					break;
				}
				s=s->next;
			}
			/* Second pass (if no 16bits found above) is to take the sample with
			   the highest speed and dither it by half. */
			if(!s) {
				s = samplist;
				speed = 0;
				while(s) {
					if((s->sample->length) && (RealSpeed(s)>speed)) {
						speed=RealSpeed(s);
						c2smp=s;
					}
					s=s->next;
				}
				if (c2smp)
					SL_HalveSample(c2smp,2);
			}
		}

	/* Samples dithered, now load them ! */
	s = samplist;
	while(s) {
		/* sample has to be loaded ? -> increase number of samples, allocate
		   memory and load sample. */
		if(s->sample->length) {
			if(s->sample->seekpos)
				_mm_fseek(s->reader, s->sample->seekpos, SEEK_SET);

			/* Call the sample load routine of the driver module. It has to
			   return a 'handle' (>=0) that identifies the sample. */
			s->sample->handle = MD_SampleLoad(s, type);
			s->sample->flags  = (s->sample->flags & ~SF_FORMATMASK) | s->outfmt;
			if(s->sample->handle<0) {
				FreeSampleList(samplist);
				if(_mm_errorhandler) _mm_errorhandler();
				return 1;
			}
		}
		s = s->next;
	}

	FreeSampleList(samplist);
	return 0;
}

BOOL SL_LoadSamples(void)
{
	BOOL ok;

	_mm_critical = 0;

	if((!musiclist)&&(!sndfxlist)) return 0;
	ok=DitherSamples(musiclist,MD_MUSIC)||DitherSamples(sndfxlist,MD_SNDFX);
	musiclist=sndfxlist=NULL;

	return ok;
}

void SL_Sample16to8(SAMPLOAD* s)
{
	s->outfmt &= ~SF_16BITS;
	s->sample->flags = (s->sample->flags&~SF_FORMATMASK) | s->outfmt;
}

void SL_Sample8to16(SAMPLOAD* s)
{
	s->outfmt |= SF_16BITS;
	s->sample->flags = (s->sample->flags&~SF_FORMATMASK) | s->outfmt;
}

void SL_SampleSigned(SAMPLOAD* s)
{
	s->outfmt |= SF_SIGNED;
	s->sample->flags = (s->sample->flags&~SF_FORMATMASK) | s->outfmt;
}

void SL_SampleUnsigned(SAMPLOAD* s)
{
	s->outfmt &= ~SF_SIGNED;
	s->sample->flags = (s->sample->flags&~SF_FORMATMASK) | s->outfmt;
}

void SL_HalveSample(SAMPLOAD* s,int factor)
{
	s->scalefactor=factor>0?factor:2;

	s->sample->divfactor = (UBYTE)s->scalefactor;
	s->sample->length    = s->length / s->scalefactor;
	s->sample->loopstart = s->loopstart / s->scalefactor;
	s->sample->loopend   = s->loopend / s->scalefactor;
}


UWORD npertab[7 * OCTAVE] = {
	/* Octaves 6 -> 0 */
	/* C    C#     D    D#     E     F    F#     G    G#     A    A#     B */
	0x6b0,0x650,0x5f4,0x5a0,0x54c,0x500,0x4b8,0x474,0x434,0x3f8,0x3c0,0x38a,
	0x358,0x328,0x2fa,0x2d0,0x2a6,0x280,0x25c,0x23a,0x21a,0x1fc,0x1e0,0x1c5,
	0x1ac,0x194,0x17d,0x168,0x153,0x140,0x12e,0x11d,0x10d,0x0fe,0x0f0,0x0e2,
	0x0d6,0x0ca,0x0be,0x0b4,0x0aa,0x0a0,0x097,0x08f,0x087,0x07f,0x078,0x071,
	0x06b,0x065,0x05f,0x05a,0x055,0x050,0x04b,0x047,0x043,0x03f,0x03c,0x038,
	0x035,0x032,0x02f,0x02d,0x02a,0x028,0x025,0x023,0x021,0x01f,0x01e,0x01c,
	0x01b,0x019,0x018,0x016,0x015,0x014,0x013,0x012,0x011,0x010,0x00f,0x00e
};


static void extract_channel(const char *src, char *dst, int num_chan, int num_samples, int samp_size, int channel);;


typedef struct WAV {
	CHAR  rID[4];
	ULONG rLen;
	CHAR  wID[4];
	CHAR  fID[4];
	ULONG fLen;
	UWORD wFormatTag;
	UWORD nChannels;
	ULONG nSamplesPerSec;
	ULONG nAvgBytesPerSec;
	UWORD nBlockAlign;
	UWORD nFormatSpecific;
} WAV;

BOOL isWaveFile(MREADER* reader)
{
	WAV wh;

	_mm_fseek(reader, SEEK_SET, 0);

	/* read wav header */
	_mm_read_string(wh.rID,4,reader);
	wh.rLen = _mm_read_I_ULONG(reader);
	_mm_read_string(wh.wID,4,reader);

	/* check for correct header */
	if(_mm_eof(reader)|| memcmp(wh.rID,"RIFF",4) || memcmp(wh.wID,"WAVE",4)) {
		return 0;
	}
	return 1;
}

SAMPLE* Sample_LoadGeneric_internal_wav(MREADER* reader)
{
	SAMPLE *si=NULL;
	WAV wh;
	BOOL have_fmt=0;

	_mm_fseek(reader, SEEK_SET, 0);

	/* read wav header */
	_mm_read_string(wh.rID,4,reader);
	wh.rLen = _mm_read_I_ULONG(reader);
	_mm_read_string(wh.wID,4,reader);

	/* check for correct header */
	if(_mm_eof(reader)|| memcmp(wh.rID,"RIFF",4) || memcmp(wh.wID,"WAVE",4)) {
		_mm_errno = MMERR_UNKNOWN_WAVE_TYPE;
		return NULL;
	}

	/* scan all RIFF blocks until we find the sample data */
	for(;;) {
		CHAR dID[4];
		ULONG len,start;

		_mm_read_string(dID,4,reader);
		len = _mm_read_I_ULONG(reader);
		/* truncated file ? */
		if (_mm_eof(reader)) {
			_mm_errno=MMERR_UNKNOWN_WAVE_TYPE;
			return NULL;
		}
		start = _mm_ftell(reader);

		/* sample format block
		   should be present only once and before a data block */
		if(!memcmp(dID,"fmt ",4)) {
			wh.wFormatTag      = _mm_read_I_UWORD(reader);
			wh.nChannels       = _mm_read_I_UWORD(reader);
			wh.nSamplesPerSec  = _mm_read_I_ULONG(reader);
			wh.nAvgBytesPerSec = _mm_read_I_ULONG(reader);
			wh.nBlockAlign     = _mm_read_I_UWORD(reader);
			wh.nFormatSpecific = _mm_read_I_UWORD(reader);

#ifdef MIKMOD_DEBUG
			fprintf(stderr,"\rwavloader : wFormatTag=%04x blockalign=%04x nFormatSpc=%04x\n",
			        wh.wFormatTag,wh.nBlockAlign,wh.nFormatSpecific);
#endif

			if((have_fmt)||(wh.nChannels>1)) {
				_mm_errno=MMERR_UNKNOWN_WAVE_TYPE;
				return NULL;
			}
			have_fmt=1;
		} else
		/* sample data block
		   should be present only once and after a format block */
		  if(!memcmp(dID,"data",4)) {
			if(!have_fmt) {
				_mm_errno=MMERR_UNKNOWN_WAVE_TYPE;
				return NULL;
			}
			if(!(si=(SAMPLE*)MikMod_malloc(sizeof(SAMPLE)))) return NULL;
			si->speed  = wh.nSamplesPerSec/wh.nChannels;
			si->volume = 64;
			si->length = len;
			if(wh.nBlockAlign == 2) {
				si->flags    = SF_16BITS | SF_SIGNED;
				si->length >>= 1;
			}
			si->inflags = si->flags;
			SL_RegisterSample(si,MD_SNDFX,reader);
			SL_LoadSamples();

			/* skip any other remaining blocks - so in case of repeated sample
			   fragments, we'll return the first anyway instead of an error */
			break;
		}
		/* onto next block */
		_mm_fseek(reader,start+len,SEEK_SET);
		if (_mm_eof(reader))
			break;
	}

	return si;
}

SAMPLE* Sample_LoadRawGeneric_internal(MREADER* reader, ULONG rate, ULONG channel, ULONG flags)
{
	SAMPLE *si;
	long len;
	int samp_size=1;

	if(!(si=(SAMPLE*)MikMod_malloc(sizeof(SAMPLE)))) return NULL;

	/* length */
	_mm_fseek(reader, 0, SEEK_END);
	len = _mm_ftell(reader);

	si->panning = PAN_CENTER;
	si->speed = rate/1;
	si->volume = 64;
	si->length = len;
	si->loopstart=0;
	si->loopend = len;
	si->susbegin = 0;
	si->susend = 0;
	si->inflags = si->flags = (UWORD)flags;
	if (si->flags & SF_16BITS) {
		si->length >>= 1;
		si->loopstart >>= 1;
		si->loopend >>= 1;
		samp_size = 2;
	}

	if (si->flags & SF_STEREO)
	{
		char *data, *channel_data;
		int num_samp = si->length/samp_size/2;
		MREADER *chn_reader;

		data = (char*)MikMod_malloc(si->length);
		if (!data) { MikMod_free(si); return NULL; }

		channel_data = (char*)MikMod_malloc(si->length/2);
		if (!channel_data) { MikMod_free(data); MikMod_free(si); return NULL; }

		/* load the raw samples completely, and fully extract the
		 * requested channel. Create a memory reader pointing to
		 * the channel data. */
		_mm_fseek(reader, 0, SEEK_SET);
		reader->Read(reader, data, si->length);

		extract_channel(data, channel_data, 2, num_samp, samp_size, channel);
		chn_reader = _mm_new_mem_reader(channel_data, num_samp * samp_size);
		if (!chn_reader) {
			MikMod_free(channel_data);
			MikMod_free(data);
			MikMod_free(si);
			return NULL;
		}

		/* half of the samples were in the other channel */
		si->loopstart=0;
		si->length=num_samp;
		si->loopend=num_samp;

		SL_RegisterSample(si, MD_SNDFX, chn_reader);
		SL_LoadSamples();

		_mm_delete_mem_reader(chn_reader);
		MikMod_free(channel_data);
		MikMod_free(data);

		return si;
	}

	_mm_fseek(reader, 0, SEEK_SET);
	SL_RegisterSample(si, MD_SNDFX, reader);
	SL_LoadSamples();



	return si;
}

SAMPLE* Sample_LoadGeneric_internal(MREADER* reader, const char * )
{
	if (isWaveFile(reader)) {
		return Sample_LoadGeneric_internal_wav(reader);
	}

	return NULL;
}

SAMPLE* Sample_LoadRawGeneric(MREADER* reader, ULONG rate, ULONG channel, ULONG flags)
{
	SAMPLE* result;

	MUTEX_LOCK(vars);
//	result=Sample_LoadRawGeneric_internal(reader, NULL);
	result = Sample_LoadRawGeneric_internal(reader, rate, channel, flags);
	MUTEX_UNLOCK(vars);

	return result;
}

extern SAMPLE *Sample_LoadRawMem(const char *buf, int len, ULONG rate, ULONG channel, ULONG flags)
{
	SAMPLE *result=NULL;
	MREADER *reader;

	if ((reader=_mm_new_mem_reader(buf, len))) {
		result=Sample_LoadRawGeneric(reader, rate, channel, flags);
		_mm_delete_mem_reader(reader);
	}
	return result;
}

SAMPLE* Sample_LoadRawFP(FILE *fp, ULONG rate, ULONG channel, ULONG flags)
{
	SAMPLE* result=NULL;
	MREADER* reader;

	if((reader=_mm_new_file_reader(fp))) {
		result=Sample_LoadRawGeneric(reader, rate, channel, flags);
		_mm_delete_file_reader(reader);
	}
	return result;
}

SAMPLE* Sample_LoadRaw(CHAR* filename, ULONG rate, ULONG channel, ULONG flags)
{
	FILE *fp;
	SAMPLE *si=NULL;

	printf("filename: %s\n", filename);

	if(!(md_mode & DMODE_SOFT_SNDFX)) return NULL;
	if((fp=_mm_fopen(filename,"rb"))) {
		si = Sample_LoadRawFP(fp, rate, channel, flags);
		_mm_fclose(fp);
	}
	return si;
}



SAMPLE* Sample_LoadGeneric(MREADER* reader)
{
	SAMPLE* result;

	MUTEX_LOCK(vars);
	result=Sample_LoadGeneric_internal(reader, NULL);
	MUTEX_UNLOCK(vars);

	return result;
}

extern SAMPLE *Sample_LoadMem(const char *buf, int len)
{
	SAMPLE* result=NULL;
	MREADER* reader;

	if ((reader=_mm_new_mem_reader(buf, len))) {
		result=Sample_LoadGeneric(reader);
		_mm_delete_mem_reader(reader);
	}
	return result;
}

SAMPLE* Sample_LoadFP(FILE *fp)
{
	SAMPLE* result=NULL;
	MREADER* reader;

	if((reader=_mm_new_file_reader(fp))) {
		result=Sample_LoadGeneric(reader);
		_mm_delete_file_reader(reader);
	}
	return result;
}

SAMPLE* Sample_Load(CHAR* filename)
{
	FILE *fp;
	SAMPLE *si=NULL;

	if(!(md_mode & DMODE_SOFT_SNDFX)) return NULL;
	if((fp=_mm_fopen(filename,"rb"))) {
		si = Sample_LoadFP(fp);
		_mm_fclose(fp);
	}
	return si;
}

void Sample_Free(SAMPLE* si)
{
	if(si) {
		if (si->onfree != NULL) { si->onfree(si->ctx); }
		MD_SampleUnload(si->handle);
		MikMod_free(si);
	}
}

void Sample_Free_internal(SAMPLE *si)
{
	MUTEX_LOCK(vars);
	Sample_Free(si);
	MUTEX_UNLOCK(vars);
}

static void extract_channel(const char *src, char *dst, int num_chan, int num_samples, int samp_size, int channel)
{
	int i;
	printf("Extract channel: %p %p, num_chan=%d, num_samples=%d, samp_size=%d, channel=%d\n",
			src,dst,num_chan,num_samples,samp_size,channel);
	src += channel * samp_size;

	while (num_samples--)
	{
		for (i=0; i<samp_size; i++) {
			dst[i] = src[i];
		}
		src += samp_size * num_chan;
		dst += samp_size;
	}
}


#define BUFPAGE  128

UWORD unioperands[UNI_LAST]={
	0, /* not used */
	1, /* UNI_NOTE */
	1, /* UNI_INSTRUMENT */
	1, /* UNI_PTEFFECT0 */
	1, /* UNI_PTEFFECT1 */
	1, /* UNI_PTEFFECT2 */
	1, /* UNI_PTEFFECT3 */
	1, /* UNI_PTEFFECT4 */
	1, /* UNI_PTEFFECT5 */
	1, /* UNI_PTEFFECT6 */
	1, /* UNI_PTEFFECT7 */
	1, /* UNI_PTEFFECT8 */
	1, /* UNI_PTEFFECT9 */
	1, /* UNI_PTEFFECTA */
	1, /* UNI_PTEFFECTB */
	1, /* UNI_PTEFFECTC */
	1, /* UNI_PTEFFECTD */
	1, /* UNI_PTEFFECTE */
	1, /* UNI_PTEFFECTF */
	1, /* UNI_S3MEFFECTA */
	1, /* UNI_S3MEFFECTD */
	1, /* UNI_S3MEFFECTE */
	1, /* UNI_S3MEFFECTF */
	1, /* UNI_S3MEFFECTI */
	1, /* UNI_S3MEFFECTQ */
	1, /* UNI_S3MEFFECTR */
	1, /* UNI_S3MEFFECTT */
	1, /* UNI_S3MEFFECTU */
	0, /* UNI_KEYOFF */
	1, /* UNI_KEYFADE */
	2, /* UNI_VOLEFFECTS */
	1, /* UNI_XMEFFECT4 */
	1, /* UNI_XMEFFECT6 */
	1, /* UNI_XMEFFECTA */
	1, /* UNI_XMEFFECTE1 */
	1, /* UNI_XMEFFECTE2 */
	1, /* UNI_XMEFFECTEA */
	1, /* UNI_XMEFFECTEB */
	1, /* UNI_XMEFFECTG */
	1, /* UNI_XMEFFECTH */
	1, /* UNI_XMEFFECTL */
	1, /* UNI_XMEFFECTP */
	1, /* UNI_XMEFFECTX1 */
	1, /* UNI_XMEFFECTX2 */
	1, /* UNI_ITEFFECTG */
	1, /* UNI_ITEFFECTH */
	1, /* UNI_ITEFFECTI */
	1, /* UNI_ITEFFECTM */
	1, /* UNI_ITEFFECTN */
	1, /* UNI_ITEFFECTP */
	1, /* UNI_ITEFFECTT */
	1, /* UNI_ITEFFECTU */
	1, /* UNI_ITEFFECTW */
	1, /* UNI_ITEFFECTY */
	2, /* UNI_ITEFFECTZ */
	1, /* UNI_ITEFFECTS0 */
	2, /* UNI_ULTEFFECT9 */
	2, /* UNI_MEDSPEED */
	0, /* UNI_MEDEFFECTF1 */
	0, /* UNI_MEDEFFECTF2 */
	0, /* UNI_MEDEFFECTF3 */
	2, /* UNI_OKTARP */
};

/* Sparse description of the internal module format
   ------------------------------------------------

  A UNITRK stream is an array of bytes representing a single track of a pattern.
It's made up of 'repeat/length' bytes, opcodes and operands (sort of a assembly
language):

rrrlllll
[REP/LEN][OPCODE][OPERAND][OPCODE][OPERAND] [REP/LEN][OPCODE][OPERAND]..
^                                         ^ ^
|-------ROWS 0 - 0+REP of a track---------| |-------ROWS xx - xx+REP of a track...

  The rep/len byte contains the number of bytes in the current row, _including_
the length byte itself (So the LENGTH byte of row 0 in the previous example
would have a value of 5). This makes it easy to search through a stream for a
particular row. A track is concluded by a 0-value length byte.

  The upper 3 bits of the rep/len byte contain the number of times -1 this row
is repeated for this track. (so a value of 7 means this row is repeated 8 times)

  Opcodes can range from 1 to 255 but currently only opcodes 1 to 62 are being
used. Each opcode can have a different number of operands. You can find the
number of operands to a particular opcode by using the opcode as an index into
the 'unioperands' table.

*/

/*========== Reading routines */

static	UBYTE *rowstart; /* startadress of a row */
static	UBYTE *rowend;   /* endaddress of a row (exclusive) */
static	UBYTE *rowpc;    /* current unimod(tm) programcounter */

static	UBYTE lastbyte;  /* for UniSkipOpcode() */

void UniSetRow(UBYTE* t)
{
	rowstart = t;
	rowpc    = rowstart;
	rowend   = t?rowstart+(*(rowpc++)&0x1f):t;
}

UBYTE UniGetByte(void)
{
	return lastbyte = (rowpc<rowend)?*(rowpc++):0;
}

UWORD UniGetWord(void)
{
	return ((UWORD)UniGetByte()<<8)|UniGetByte();
}

void UniSkipOpcode(void)
{
	if (lastbyte < UNI_LAST) {
		UWORD t = unioperands[lastbyte];

		while (t--)
			UniGetByte();
	}
}

/* Finds the address of row number 'row' in the UniMod(tm) stream 't' returns
   NULL if the row can't be found. */
UBYTE *UniFindRow(UBYTE* t,UWORD row)
{
	UBYTE c,l;

	if(t)
		for ( ;; )
    {
			c = *t;             /* get rep/len byte */
			if(!c) return NULL; /* zero ? -> end of track.. */
			l = (c>>5)+1;       /* extract repeat value */
			if(l>row) break;    /* reached wanted row? -> return pointer */
      row = row - l;      /* haven't reached row yet.. update row */
			t += c&0x1f;        /* point t to the next row */
		}
	return t;
}

/*========== Writing routines */

static	UBYTE *unibuf; /* pointer to the temporary unitrk buffer */
static	UWORD unimax;  /* buffer size */

static	UWORD unipc;   /* buffer cursor */
static	UWORD unitt;   /* current row index */
static	UWORD lastp;   /* previous row index */

/* Resets index-pointers to create a new track. */
void UniReset(void)
{
	unitt     = 0;   /* reset index to rep/len byte */
	unipc     = 1;   /* first opcode will be written to index 1 */
	lastp     = 0;   /* no previous row yet */
	unibuf[0] = 0;   /* clear rep/len byte */
}

/* Expands the buffer */
static BOOL UniExpand(int wanted)
{
	if ((unipc+wanted)>=unimax) {
		UBYTE *newbuf;

		/* Expand the buffer by BUFPAGE bytes */
		newbuf=(UBYTE*)MikMod_realloc(unibuf,(unimax+BUFPAGE)*sizeof(UBYTE));

		/* Check if MikMod_realloc succeeded */
		if(newbuf) {
			unibuf = newbuf;
			unimax+=BUFPAGE;
			return 1;
		} else
			return 0;
	}
	return 1;
}

/* Appends one byte of data to the current row of a track. */
void UniWriteByte(UBYTE data)
{
	if (UniExpand(1))
		/* write byte to current position and update */
		unibuf[unipc++]=data;
}

void UniWriteWord(UWORD data)
{
	if (UniExpand(2))
  {
		unibuf[unipc++]= (UBYTE)( data >> 8 );
		unibuf[unipc++] = (UBYTE)( data & 0xff );
	}
}

static BOOL MyCmp(UBYTE* a,UBYTE* b,UWORD l)
{
	UWORD t;

	for(t=0;t<l;t++)
		if(*(a++)!=*(b++)) return 0;
	return 1;
}

/* Closes the current row of a unitrk stream (updates the rep/len byte) and sets
   pointers to start a new row. */
void UniNewline(void)
{
	UWORD n,l,len;

	n = (unibuf[lastp]>>5)+1;     /* repeat of previous row */
	l = (unibuf[lastp]&0x1f);     /* length of previous row */

	len = unipc-unitt;            /* length of current row */

	/* Now, check if the previous and the current row are identical.. when they
	   are, just increase the repeat field of the previous row */
	if(n<8 && len==l && MyCmp(&unibuf[lastp+1],&unibuf[unitt+1],len-1)) {
		unibuf[lastp]+=0x20;
		unipc = unitt+1;
	} else {
		if (UniExpand(unitt-unipc)) {
			/* current and previous row aren't equal... update the pointers */
			unibuf[unitt] = (UBYTE)len;
			lastp = unitt;
			unitt = unipc++;
		}
	}
}

/* Terminates the current unitrk stream and returns a pointer to a copy of the
   stream. */
UBYTE* UniDup(void)
{
	UBYTE *d;

	if (!UniExpand(unitt-unipc)) return NULL;
	unibuf[unitt] = 0;

	if(!(d=(UBYTE *)MikMod_malloc(unipc))) return NULL;
	memcpy(d,unibuf,unipc);

	return d;
}

BOOL UniInit(void)
{
	unimax = BUFPAGE;

	if(!(unibuf=(UBYTE*)MikMod_malloc(unimax*sizeof(UBYTE)))) return 0;
	return 1;
}

void UniCleanup(void)
{
	if(unibuf) MikMod_free(unibuf);
	unibuf = NULL;
}


MODULE *pf = NULL;

#define	HIGH_OCTAVE		2	/* number of above-range octaves */

static	UWORD oldperiods[OCTAVE*2]={
	0x6b00, 0x6800, 0x6500, 0x6220, 0x5f50, 0x5c80,
	0x5a00, 0x5740, 0x54d0, 0x5260, 0x5010, 0x4dc0,
	0x4b90, 0x4960, 0x4750, 0x4540, 0x4350, 0x4160,
	0x3f90, 0x3dc0, 0x3c10, 0x3a40, 0x38b0, 0x3700
};

static	UBYTE VibratoTable[32]={
	  0, 24, 49, 74, 97,120,141,161,180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,180,161,141,120, 97, 74, 49, 24
};

static	UBYTE avibtab[128]={
	 0, 1, 3, 4, 6, 7, 9,10,12,14,15,17,18,20,21,23,
	24,25,27,28,30,31,32,34,35,36,38,39,40,41,42,44,
	45,46,47,48,49,50,51,52,53,54,54,55,56,57,57,58,
	59,59,60,60,61,61,62,62,62,63,63,63,63,63,63,63,
	64,63,63,63,63,63,63,63,62,62,62,61,61,60,60,59,
	59,58,57,57,56,55,54,54,53,52,51,50,49,48,47,46,
	45,44,42,41,40,39,38,36,35,34,32,31,30,28,27,25,
	24,23,21,20,18,17,15,14,12,10, 9, 7, 6, 4, 3, 1
};

/* Triton's linear periods to frequency translation table (for XM modules) */
static	ULONG lintab[768]={
	535232,534749,534266,533784,533303,532822,532341,531861,
	531381,530902,530423,529944,529466,528988,528511,528034,
	527558,527082,526607,526131,525657,525183,524709,524236,
	523763,523290,522818,522346,521875,521404,520934,520464,
	519994,519525,519057,518588,518121,517653,517186,516720,
	516253,515788,515322,514858,514393,513929,513465,513002,
	512539,512077,511615,511154,510692,510232,509771,509312,
	508852,508393,507934,507476,507018,506561,506104,505647,
	505191,504735,504280,503825,503371,502917,502463,502010,
	501557,501104,500652,500201,499749,499298,498848,498398,
	497948,497499,497050,496602,496154,495706,495259,494812,
	494366,493920,493474,493029,492585,492140,491696,491253,
	490809,490367,489924,489482,489041,488600,488159,487718,
	487278,486839,486400,485961,485522,485084,484647,484210,
	483773,483336,482900,482465,482029,481595,481160,480726,
	480292,479859,479426,478994,478562,478130,477699,477268,
	476837,476407,475977,475548,475119,474690,474262,473834,
	473407,472979,472553,472126,471701,471275,470850,470425,
	470001,469577,469153,468730,468307,467884,467462,467041,
	466619,466198,465778,465358,464938,464518,464099,463681,
	463262,462844,462427,462010,461593,461177,460760,460345,
	459930,459515,459100,458686,458272,457859,457446,457033,
	456621,456209,455797,455386,454975,454565,454155,453745,
	453336,452927,452518,452110,451702,451294,450887,450481,
	450074,449668,449262,448857,448452,448048,447644,447240,
	446836,446433,446030,445628,445226,444824,444423,444022,
	443622,443221,442821,442422,442023,441624,441226,440828,
	440430,440033,439636,439239,438843,438447,438051,437656,
	437261,436867,436473,436079,435686,435293,434900,434508,
	434116,433724,433333,432942,432551,432161,431771,431382,
	430992,430604,430215,429827,429439,429052,428665,428278,
	427892,427506,427120,426735,426350,425965,425581,425197,
	424813,424430,424047,423665,423283,422901,422519,422138,
	421757,421377,420997,420617,420237,419858,419479,419101,
	418723,418345,417968,417591,417214,416838,416462,416086,
	415711,415336,414961,414586,414212,413839,413465,413092,
	412720,412347,411975,411604,411232,410862,410491,410121,
	409751,409381,409012,408643,408274,407906,407538,407170,
	406803,406436,406069,405703,405337,404971,404606,404241,
	403876,403512,403148,402784,402421,402058,401695,401333,
	400970,400609,400247,399886,399525,399165,398805,398445,
	398086,397727,397368,397009,396651,396293,395936,395579,
	395222,394865,394509,394153,393798,393442,393087,392733,
	392378,392024,391671,391317,390964,390612,390259,389907,
	389556,389204,388853,388502,388152,387802,387452,387102,
	386753,386404,386056,385707,385359,385012,384664,384317,
	383971,383624,383278,382932,382587,382242,381897,381552,
	381208,380864,380521,380177,379834,379492,379149,378807,
	378466,378124,377783,377442,377102,376762,376422,376082,
	375743,375404,375065,374727,374389,374051,373714,373377,
	373040,372703,372367,372031,371695,371360,371025,370690,
	370356,370022,369688,369355,369021,368688,368356,368023,
	367691,367360,367028,366697,366366,366036,365706,365376,
	365046,364717,364388,364059,363731,363403,363075,362747,
	362420,362093,361766,361440,361114,360788,360463,360137,
	359813,359488,359164,358840,358516,358193,357869,357547,
	357224,356902,356580,356258,355937,355616,355295,354974,
	354654,354334,354014,353695,353376,353057,352739,352420,
	352103,351785,351468,351150,350834,350517,350201,349885,
	349569,349254,348939,348624,348310,347995,347682,347368,
	347055,346741,346429,346116,345804,345492,345180,344869,
	344558,344247,343936,343626,343316,343006,342697,342388,
	342079,341770,341462,341154,340846,340539,340231,339924,
	339618,339311,339005,338700,338394,338089,337784,337479,
	337175,336870,336566,336263,335959,335656,335354,335051,
	334749,334447,334145,333844,333542,333242,332941,332641,
	332341,332041,331741,331442,331143,330844,330546,330247,
	329950,329652,329355,329057,328761,328464,328168,327872,
	327576,327280,326985,326690,326395,326101,325807,325513,
	325219,324926,324633,324340,324047,323755,323463,323171,
	322879,322588,322297,322006,321716,321426,321136,320846,
	320557,320267,319978,319690,319401,319113,318825,318538,
	318250,317963,317676,317390,317103,316817,316532,316246,
	315961,315676,315391,315106,314822,314538,314254,313971,
	313688,313405,313122,312839,312557,312275,311994,311712,
	311431,311150,310869,310589,310309,310029,309749,309470,
	309190,308911,308633,308354,308076,307798,307521,307243,
	306966,306689,306412,306136,305860,305584,305308,305033,
	304758,304483,304208,303934,303659,303385,303112,302838,
	302565,302292,302019,301747,301475,301203,300931,300660,
	300388,300117,299847,299576,299306,299036,298766,298497,
	298227,297958,297689,297421,297153,296884,296617,296349,
	296082,295815,295548,295281,295015,294749,294483,294217,
	293952,293686,293421,293157,292892,292628,292364,292100,
	291837,291574,291311,291048,290785,290523,290261,289999,
	289737,289476,289215,288954,288693,288433,288173,287913,
	287653,287393,287134,286875,286616,286358,286099,285841,
	285583,285326,285068,284811,284554,284298,284041,283785,
	283529,283273,283017,282762,282507,282252,281998,281743,
	281489,281235,280981,280728,280475,280222,279969,279716,
	279464,279212,278960,278708,278457,278206,277955,277704,
	277453,277203,276953,276703,276453,276204,275955,275706,
	275457,275209,274960,274712,274465,274217,273970,273722,
	273476,273229,272982,272736,272490,272244,271999,271753,
	271508,271263,271018,270774,270530,270286,270042,269798,
	269555,269312,269069,268826,268583,268341,268099,267857
};

#define LOGFAC 2*16
static	UWORD logtab[104]={
	LOGFAC*907,LOGFAC*900,LOGFAC*894,LOGFAC*887,
	LOGFAC*881,LOGFAC*875,LOGFAC*868,LOGFAC*862,
	LOGFAC*856,LOGFAC*850,LOGFAC*844,LOGFAC*838,
	LOGFAC*832,LOGFAC*826,LOGFAC*820,LOGFAC*814,
	LOGFAC*808,LOGFAC*802,LOGFAC*796,LOGFAC*791,
	LOGFAC*785,LOGFAC*779,LOGFAC*774,LOGFAC*768,
	LOGFAC*762,LOGFAC*757,LOGFAC*752,LOGFAC*746,
	LOGFAC*741,LOGFAC*736,LOGFAC*730,LOGFAC*725,
	LOGFAC*720,LOGFAC*715,LOGFAC*709,LOGFAC*704,
	LOGFAC*699,LOGFAC*694,LOGFAC*689,LOGFAC*684,
	LOGFAC*678,LOGFAC*675,LOGFAC*670,LOGFAC*665,
	LOGFAC*660,LOGFAC*655,LOGFAC*651,LOGFAC*646,
	LOGFAC*640,LOGFAC*636,LOGFAC*632,LOGFAC*628,
	LOGFAC*623,LOGFAC*619,LOGFAC*614,LOGFAC*610,
	LOGFAC*604,LOGFAC*601,LOGFAC*597,LOGFAC*592,
	LOGFAC*588,LOGFAC*584,LOGFAC*580,LOGFAC*575,
	LOGFAC*570,LOGFAC*567,LOGFAC*563,LOGFAC*559,
	LOGFAC*555,LOGFAC*551,LOGFAC*547,LOGFAC*543,
	LOGFAC*538,LOGFAC*535,LOGFAC*532,LOGFAC*528,
	LOGFAC*524,LOGFAC*520,LOGFAC*516,LOGFAC*513,
	LOGFAC*508,LOGFAC*505,LOGFAC*502,LOGFAC*498,
	LOGFAC*494,LOGFAC*491,LOGFAC*487,LOGFAC*484,
	LOGFAC*480,LOGFAC*477,LOGFAC*474,LOGFAC*470,
	LOGFAC*467,LOGFAC*463,LOGFAC*460,LOGFAC*457,
	LOGFAC*453,LOGFAC*450,LOGFAC*447,LOGFAC*443,
	LOGFAC*440,LOGFAC*437,LOGFAC*434,LOGFAC*431
};

static	SBYTE PanbrelloTable[256]={
	  0,  2,  3,  5,  6,  8,  9, 11, 12, 14, 16, 17, 19, 20, 22, 23,
	 24, 26, 27, 29, 30, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44,
	 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59,
	 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64,
	 64, 64, 64, 64, 64, 64, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60,
	 59, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46,
	 45, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 30, 29, 27, 26,
	 24, 23, 22, 20, 19, 17, 16, 14, 12, 11,  9,  8,  6,  5,  3,  2,
	  0,- 2,- 3,- 5,- 6,- 8,- 9,-11,-12,-14,-16,-17,-19,-20,-22,-23,
	-24,-26,-27,-29,-30,-32,-33,-34,-36,-37,-38,-39,-41,-42,-43,-44,
	-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-56,-57,-58,-59,
	-59,-60,-60,-61,-61,-62,-62,-62,-63,-63,-63,-64,-64,-64,-64,-64,
	-64,-64,-64,-64,-64,-64,-63,-63,-63,-62,-62,-62,-61,-61,-60,-60,
	-59,-59,-58,-57,-56,-56,-55,-54,-53,-52,-51,-50,-49,-48,-47,-46,
	-45,-44,-43,-42,-41,-39,-38,-37,-36,-34,-33,-32,-30,-29,-27,-26,
	-24,-23,-22,-20,-19,-17,-16,-14,-12,-11,- 9,- 8,- 6,- 5,- 3,- 2
};

/* returns a random value between 0 and ceil-1, ceil must be a power of two */
static int getrandom(int ceil)
{
#ifdef HAVE_SRANDOM
	return random()&(ceil-1);
#else
	return (int)( (rand()*ceil)/(RAND_MAX+1.0) );
#endif
}

/*	New Note Action Scoring System :
	--------------------------------
	1)	total-volume (fadevol, chanvol, volume) is the main scorer.
	2)	a looping sample is a bonus x2
	3)	a foreground channel is a bonus x4
	4)	an active envelope with keyoff is a handicap -x2
*/
static int MP_FindEmptyChannel(MODULE *mod)
{
	MP_VOICE *a;
	ULONG t,k,tvol,pp;

	for (t=0;t<md_sngchn;t++)
		if (((mod->voice[t].main.kick==KICK_ABSENT)||
			 (mod->voice[t].main.kick==KICK_ENV))&&
		   Voice_Stopped_internal((SBYTE)t))
			return t;

	tvol = 0xffffffUL;
  t = (ULONG)-1;
  a = mod->voice;

	for (k=0;k<md_sngchn;k++,a++) {
		/* allow us to take over a nonexisting sample */
		if (!a->main.s)
			return k;

		if ((a->main.kick==KICK_ABSENT)||(a->main.kick==KICK_ENV)) {
			pp=a->totalvol<<((a->main.s->flags&SF_LOOP)?1:0);
			if ((a->master)&&(a==a->master->slave))
				pp<<=2;

			if (pp<tvol) {
				tvol=pp;
				t=k;
			}
		}
	}

	if (tvol>8000*7) return -1;
	return t;
}

static SWORD Interpolate(SWORD p,SWORD p1,SWORD p2,SWORD v1,SWORD v2)
{
	if ((p1==p2)||(p==p1)) return v1;
	return (SWORD)( v1+((SLONG)((p-p1)*(v2-v1))/(p2-p1)) );
}

UWORD getlinearperiod(UWORD note,ULONG fine)
{
	UWORD t;

	t= (UWORD)( ((20L+2*HIGH_OCTAVE)*OCTAVE+2-note)*32L-(fine>>1) );
	return t;
}

static UWORD getlogperiod(UWORD note,ULONG fine)
{
	UWORD n,o;
	UWORD p1,p2;
	ULONG i;

	n=note%(2*OCTAVE);
	o=note/(2*OCTAVE);
	i=(n<<2)+(fine>>4); /* n*8 + fine/16 */

	p1=logtab[i];
	p2=logtab[i+1];

	return (Interpolate((SWORD)( fine>>4 ),0,15,p1,p2)>>o);
}

static UWORD getoldperiod(UWORD note,ULONG speed)
{
	UWORD n,o;

	/* This happens sometimes on badly converted AMF, and old MOD */
	if (!speed) {
#ifdef MIKMOD_DEBUG
		fprintf(stderr,"\rmplayer: getoldperiod() called with note=%d, speed=0 !\n",note);
#endif
		return 4242; /* <- prevent divide overflow.. (42 hehe) */
	}

	n=note%(2*OCTAVE);
	o=note/(2*OCTAVE);
	return (UWORD)( ((8363L*(ULONG)oldperiods[n])>>o)/speed );
}

static UWORD GetPeriod(UWORD flags, UWORD note, ULONG speed)
{
	if (flags & UF_XMPERIODS) {
		if (flags & UF_LINEAR)
				return getlinearperiod(note, speed);
		else
				return getlogperiod(note, speed);
	} else
		return getoldperiod(note, speed);
}

static SWORD InterpolateEnv(SWORD p,ENVPT *a,ENVPT *b)
{
	return (Interpolate(p,a->pos,b->pos,a->val,b->val));
}

static SWORD DoPan(SWORD envpan,SWORD pan)
{
	int newpan;

	newpan=pan+(((envpan-PAN_CENTER)*(128-abs(pan-PAN_CENTER)))/128);

	return (SWORD)( (newpan<PAN_LEFT)?PAN_LEFT:(newpan>PAN_RIGHT?PAN_RIGHT:newpan) );
}

static SWORD StartEnvelope(ENVPR *t,UBYTE flg,UBYTE pts,UBYTE susbeg,UBYTE susend,UBYTE beg,UBYTE end,ENVPT *p,UBYTE keyoff)
{
	t->flg=flg;
	t->pts=pts;
	t->susbeg=susbeg;
	t->susend=susend;
	t->beg=beg;
	t->end=end;
	t->env=p;
	t->p=0;
	t->a=0;
	t->b=((t->flg&EF_SUSTAIN)&&(!(keyoff&KEY_OFF)))?0:1;

	/* Imago Orpheus sometimes stores an extra initial point in the envelope */
	if ((t->pts>=2)&&(t->env[0].pos==t->env[1].pos)) {
		t->a++;t->b++;
	}

	/* Fit in the envelope, still */
	if (t->a >= t->pts)
		t->a = t->pts - 1;
	if (t->b >= t->pts)
		t->b = t->pts-1;

	return t->env[t->a].val;
}

/* This procedure processes all envelope types, include volume, pitch, and
   panning.  Envelopes are defined by a set of points, each with a magnitude
   [relating either to volume, panning position, or pitch modifier] and a tick
   position.

   Envelopes work in the following manner:

   (a) Each tick the envelope is moved a point further in its progression. For
       an accurate progression, magnitudes between two envelope points are
       interpolated.

   (b) When progression reaches a defined point on the envelope, values are
       shifted to interpolate between this point and the next, and checks for
       loops or envelope end are done.

   Misc:
     Sustain loops are loops that are only active as long as the keyoff flag is
     clear.  When a volume envelope terminates, so does the current fadeout.
*/
static SWORD ProcessEnvelope(MP_VOICE *aout, ENVPR *t, SWORD v)
{
	if (t->flg & EF_ON) {
		UBYTE a, b;		/* actual points in the envelope */
		UWORD p;		/* the 'tick counter' - real point being played */

		a = (UBYTE)t->a;
		b = (UBYTE)t->b;
		p = t->p;

		/*
		 * Sustain loop on one point (XM type).
		 * Not processed if KEYOFF.
		 * Don't move and don't interpolate when the point is reached
		 */
		if ((t->flg & EF_SUSTAIN) && t->susbeg == t->susend &&
		   (!(aout->main.keyoff & KEY_OFF) && p == t->env[t->susbeg].pos)) {
			v = t->env[t->susbeg].val;
		} else {
			/*
			 * All following situations will require interpolation between
			 * two envelope points.
			 */

			/*
			 * Sustain loop between two points (IT type).
			 * Not processed if KEYOFF.
			 */
			/* if we were on a loop point, loop now */
			if ((t->flg & EF_SUSTAIN) && !(aout->main.keyoff & KEY_OFF) &&
			   a >= t->susend) {
				a = t->susbeg;
				b = (t->susbeg==t->susend)?a:a+1;
				p = t->env[a].pos;
				v = t->env[a].val;
			} else
			/*
			 * Regular loop.
			 * Be sure to correctly handle single point loops.
			 */
			if ((t->flg & EF_LOOP) && a >= t->end) {
				a = t->beg;
				b = t->beg == t->end ? a : a + 1;
				p = t->env[a].pos;
				v = t->env[a].val;
			} else
			/*
			 * Non looping situations.
			 */
			if (a != b)
				v = InterpolateEnv(p, &t->env[a], &t->env[b]);
			else
				v = t->env[a].val;

			/*
			 * Start to fade if the volume envelope is finished.
			 */
			if (p >= t->env[t->pts - 1].pos) {
				if (t->flg & EF_VOLENV) {
					aout->main.keyoff |= KEY_FADE;
					if (!v)
						aout->main.fadevol = 0;
				}
			} else {
				p++;
				/* did pointer reach point b? */
				if (p >= t->env[b].pos)
					a = b++; /* shift points a and b */
			}
			t->a = a;
			t->b = b;
			t->p = p;
		}
	}
	return v;
}

/* XM linear period to frequency conversion */
ULONG getfrequency(UWORD flags,ULONG period)
{
	if (flags & UF_LINEAR) {
		SLONG shift = ((SLONG)period / 768) - HIGH_OCTAVE;

		if (shift >= 0)
			return lintab[period % 768] >> shift;
		else
			return lintab[period % 768] << (-shift);
	} else
		return (8363L*1712L)/(period?period:1);
}

/*========== Protracker effects */

static void DoArpeggio(UWORD tick, UWORD flags, MP_CONTROL *a, UBYTE style)
{
	UBYTE note=a->main.note;

	if (a->arpmem) {
		switch (style) {
		case 0:		/* mod style: N, N+x, N+y */
			switch (tick % 3) {
			/* case 0: unchanged */
			case 1:
				note += (a->arpmem >> 4);
				break;
			case 2:
				note += (a->arpmem & 0xf);
				break;
			}
			break;
		case 3:		/* okt arpeggio 3: N-x, N, N+y */
			switch (tick % 3) {
			case 0:
				note -= (a->arpmem >> 4);
				break;
			/* case 1: unchanged */
			case 2:
				note += (a->arpmem & 0xf);
				break;
			}
			break;
		case 4:		/* okt arpeggio 4: N, N+y, N, N-x */
			switch (tick % 4) {
			/* case 0, case 2: unchanged */
			case 1:
				note += (a->arpmem & 0xf);
				break;
			case 3:
				note -= (a->arpmem >> 4);
				break;
			}
			break;
		case 5:		/* okt arpeggio 5: N-x, N+y, N, and nothing at tick 0 */
			if (!tick)
				break;
			switch (tick % 3) {
			/* case 0: unchanged */
			case 1:
				note -= (a->arpmem >> 4);
				break;
			case 2:
				note += (a->arpmem & 0xf);
				break;
			}
			break;
		}
		a->main.period = GetPeriod(flags, (UWORD)note << 1, a->speed);
		a->ownper = 1;
	}
}

static int DoPTEffect0(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat = UniGetByte();
	if (!tick) {
		if (!dat && (flags & UF_ARPMEM))
			dat=a->arpmem;
		else
			a->arpmem=dat;
	}
	if (a->main.period)
		DoArpeggio(tick, flags, a, 0);

	return 0;
}

static int DoPTEffect1(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat = UniGetByte();
	if (!tick && dat)
		a->slidespeed = (UWORD)dat << 2;
	if (a->main.period)
		if (tick)
      a->tmpperiod = a->tmpperiod - a->slidespeed;

	return 0;
}

static int DoPTEffect2(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat = UniGetByte();
	if (!tick && dat)
		a->slidespeed = (UWORD)dat << 2;
	if (a->main.period)
		if (tick)
      a->tmpperiod = a->tmpperiod + a->slidespeed;

	return 0;
}

static void DoToneSlide(UWORD tick, MP_CONTROL *a)
{
	if (!a->main.fadevol)
		a->main.kick = (a->main.kick == KICK_NOTE)? KICK_NOTE : KICK_KEYOFF;
	else
		a->main.kick = (a->main.kick == KICK_NOTE)? KICK_ENV : KICK_ABSENT;

	if (tick != 0) {
		int dist;

		/* We have to slide a->main.period towards a->wantedperiod, so compute
		   the difference between those two values */
		dist=a->main.period-a->wantedperiod;

		/* if they are equal or if portamentospeed is too big ...*/
		if (dist == 0 || a->portspeed > abs(dist))
			/* ...make tmpperiod equal tperiod */
			a->tmpperiod=a->main.period=a->wantedperiod;
		else if (dist>0)
    {
      a->tmpperiod = a->tmpperiod - a->portspeed;
      a->main.period = a->main.period - a->portspeed; /* dist>0, slide up */
		} else
    {
      a->tmpperiod = a->tmpperiod + a->portspeed;
      a->main.period = a->main.period + a->portspeed; /* dist<0, slide down */
		}
	} else
		a->tmpperiod=a->main.period;
	a->ownper = 1;
}

static int DoPTEffect3(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if ((!tick)&&(dat)) a->portspeed=(UWORD)dat<<2;
	if (a->main.period)
		DoToneSlide(tick, a);

	return 0;
}

static void DoVibrato(UWORD tick, MP_CONTROL *a)
{
	UBYTE q;
	UWORD temp = 0;	/* silence warning */

	if (!tick)
		return;

	q=(a->vibpos>>2)&0x1f;

	switch (a->wavecontrol&3) {
	case 0: /* sine */
		temp=VibratoTable[q];
		break;
	case 1: /* ramp down */
		q<<=3;
		if (a->vibpos<0) q=255-q;
		temp=q;
		break;
	case 2: /* square wave */
		temp=255;
		break;
	case 3: /* random wave */
		temp=(UWORD)getrandom(256);
		break;
	}

  temp = temp * a->vibdepth;
	temp>>=7;temp<<=2;

	if (a->vibpos>=0)
		a->main.period=a->tmpperiod+temp;
	else
		a->main.period=a->tmpperiod-temp;
	a->ownper = 1;

	if (tick != 0)
    a->vibpos = a->vibpos + a->vibspd;
}

static int DoPTEffect4(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->vibdepth=dat&0xf;
		if (dat&0xf0) a->vibspd=(dat&0xf0)>>2;
	}
	if (a->main.period)
		DoVibrato(tick, a);

	return 0;
}

static void DoVolSlide(MP_CONTROL *a, UBYTE dat)
{
	if (dat&0xf) {
		a->tmpvolume-=(dat&0x0f);
		if (a->tmpvolume<0)
			a->tmpvolume=0;
	} else {
		a->tmpvolume+=(dat>>4);
		if (a->tmpvolume>64)
			a->tmpvolume=64;
	}
}

static int DoPTEffect5(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if (a->main.period)
		DoToneSlide(tick, a);

	if (tick)
		DoVolSlide(a, dat);

	return 0;
}

/* DoPTEffect6 after DoPTEffectA */

static int DoPTEffect7(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;
	UBYTE q;
	UWORD temp = 0;	/* silence warning */

	dat=UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->trmdepth=dat&0xf;
		if (dat&0xf0) a->trmspd=(dat&0xf0)>>2;
	}
	if (a->main.period) {
		q=(a->trmpos>>2)&0x1f;

		switch ((a->wavecontrol>>4)&3) {
		case 0: /* sine */
			temp=VibratoTable[q];
			break;
		case 1: /* ramp down */
			q<<=3;
			if (a->trmpos<0) q=255-q;
			temp=q;
			break;
		case 2: /* square wave */
			temp=255;
			break;
		case 3: /* random wave */
			temp= (UWORD)getrandom(256);
			break;
		}
    temp = temp * a->trmdepth;
		temp>>=6;

		if (a->trmpos>=0) {
			a->volume=a->tmpvolume+temp;
			if (a->volume>64) a->volume=64;
		} else {
			a->volume=a->tmpvolume-temp;
			if (a->volume<0) a->volume=0;
		}
		a->ownvol = 1;

		if (tick)
      a->trmpos = a->trmpos + a->trmspd;
	}

	return 0;
}

static int DoPTEffect8(UWORD, UWORD, MP_CONTROL *a, MODULE *mod, SWORD channel )
{
	UBYTE dat;

	dat = UniGetByte();
	if (mod->panflag)
		a->main.panning = mod->panning[channel] = dat;

	return 0;
}

static int DoPTEffect9(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick) {
		if (dat) a->soffset=(UWORD)dat<<8;
		a->main.start=a->hioffset|a->soffset;

		if ((a->main.s)&&((ULONG)a->main.start>a->main.s->length))
			a->main.start=a->main.s->flags&(SF_LOOP|SF_BIDI)?
			    a->main.s->loopstart:a->main.s->length;
	}

	return 0;
}

static int DoPTEffectA(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if (tick)
		DoVolSlide(a, dat);

	return 0;
}

static int DoPTEffect6(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	if (a->main.period)
		DoVibrato(tick, a);
	DoPTEffectA(tick, flags, a, mod, channel);

	return 0;
}

static int DoPTEffectB(UWORD tick, UWORD flags, MP_CONTROL*, MODULE *mod, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();

	if (tick || mod->patdly2)
		return 0;

	/* Vincent Voois uses a nasty trick in "Universal Bolero" */
	if (dat == mod->sngpos && mod->patbrk == mod->patpos)
		return 0;

	if (!mod->loop && !mod->patbrk &&
	    (dat < mod->sngpos ||
		 (mod->sngpos == (mod->numpos - 1) && !mod->patbrk) ||
	     (dat == mod->sngpos && (flags & UF_NOWRAP))
		)) {
		/* if we don't loop, better not to skip the end of the
		   pattern, after all... so:
		mod->patbrk=0; */
		mod->posjmp=3;
	} else {
		/* if we were fading, adjust... */
		if (mod->sngpos == (mod->numpos-1))
			mod->volume=mod->initvolume>128?128:mod->initvolume;
		mod->sngpos=dat;
		mod->posjmp=2;
		mod->patpos=0;
	}

	return 0;
}

static int DoPTEffectC(UWORD tick, UWORD, MP_CONTROL *a, MODULE*, SWORD )
{
	UBYTE dat;

			dat=UniGetByte();
			if (tick) return 0;
			if (dat==(UBYTE)-1) a->anote=dat=0; /* note cut */
			else if (dat>64) dat=64;
			a->tmpvolume=dat;

	return 0;
}

static int DoPTEffectD(UWORD tick, UWORD flags, MP_CONTROL*, MODULE *mod, SWORD )
{
	UBYTE dat;

			dat=UniGetByte();
			if ((tick)||(mod->patdly2)) return 0;
			if ((mod->positions[mod->sngpos]!=LAST_PATTERN)&&
			   (dat>mod->pattrows[mod->positions[mod->sngpos]]))
				dat=(UBYTE)mod->pattrows[mod->positions[mod->sngpos]];
			mod->patbrk=dat;
			if (!mod->posjmp) {
				/* don't ask me to explain this code - it makes
				   backwards.s3m and children.xm (heretic's version) play
				   correctly, among others. Take that for granted, or write
				   the page of comments yourself... you might need some
				   aspirin - Miod */
				if ((mod->sngpos==mod->numpos-1)&&(dat)&&((mod->loop)||
				               (mod->positions[mod->sngpos]==(mod->numpat-1)
								&& !(flags&UF_NOWRAP)))) {
					mod->sngpos=0;
					mod->posjmp=2;
				} else
					mod->posjmp=3;
			}

	return 0;
}

static void DoEEffects(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod,
	SWORD channel, UBYTE dat)
{
	UBYTE nib = dat & 0xf;

	switch (dat>>4) {
	case 0x0: /* hardware filter toggle, not supported */
		break;
	case 0x1: /* fineslide up */
		if (a->main.period)
			if (!tick)
				a->tmpperiod-=(nib<<2);
		break;
	case 0x2: /* fineslide dn */
		if (a->main.period)
			if (!tick)
				a->tmpperiod+=(nib<<2);
		break;
	case 0x3: /* glissando ctrl */
		a->glissando=nib;
		break;
	case 0x4: /* set vibrato waveform */
		a->wavecontrol&=0xf0;
		a->wavecontrol|=nib;
		break;
	case 0x5: /* set finetune */
		if (a->main.period) {
			if (flags&UF_XMPERIODS)
				a->speed=nib+128;
			else
				a->speed=finetune[nib];
			a->tmpperiod=GetPeriod(flags, (UWORD)a->main.note<<1,a->speed);
		}
		break;
	case 0x6: /* set patternloop */
		if (tick)
			break;
		if (nib) { /* set reppos or repcnt ? */
			/* set repcnt, so check if repcnt already is set, which means we
			   are already looping */
			if (a->pat_repcnt)
				a->pat_repcnt--; /* already looping, decrease counter */
			else {
#if 0
				/* this would make walker.xm, shipped with Xsoundtracker,
				   play correctly, but it's better to remain compatible
				   with FT2 */
				if ((!(flags&UF_NOWRAP))||(a->pat_reppos!=POS_NONE))
#endif
					a->pat_repcnt=nib; /* not yet looping, so set repcnt */
			}

			if (a->pat_repcnt) { /* jump to reppos if repcnt>0 */
				if (a->pat_reppos==POS_NONE)
					a->pat_reppos=mod->patpos-1;
				if (a->pat_reppos==-1) {
					mod->pat_repcrazy=1;
					mod->patpos=0;
				} else
					mod->patpos=a->pat_reppos;
			} else a->pat_reppos=POS_NONE;
		} else
			a->pat_reppos=mod->patpos-1; /* set reppos - can be (-1) */
		break;
	case 0x7: /* set tremolo waveform */
		a->wavecontrol&=0x0f;
		a->wavecontrol|=nib<<4;
		break;
	case 0x8: /* set panning */
		if (mod->panflag) {
			if (nib<=8) nib<<=4;
			else nib*=17;
			a->main.panning=mod->panning[channel]=nib;
		}
		break;
	case 0x9: /* retrig note */
		/* do not retrigger on tick 0, until we are emulating FT2 and effect
		   data is zero */
		if (!tick && !((flags & UF_FT2QUIRKS) && (!nib)))
			break;
		/* only retrigger if data nibble > 0, or if tick 0 (FT2 compat) */
		if (nib || !tick) {
			if (!a->retrig) {
				/* when retrig counter reaches 0, reset counter and restart
				   the sample */
				if (a->main.period) a->main.kick=KICK_NOTE;
				a->retrig=nib;
			}
			a->retrig--; /* countdown */
		}
		break;
	case 0xa: /* fine volume slide up */
		if (tick)
			break;
    a->tmpvolume = a->tmpvolume + nib;
		if (a->tmpvolume>64) a->tmpvolume=64;
		break;
	case 0xb: /* fine volume slide dn  */
		if (tick)
			break;
    a->tmpvolume = (SWORD)( a->tmpvolume - nib );
		if (a->tmpvolume<0)a->tmpvolume=0;
		break;
	case 0xc: /* cut note */
		/* When tick reaches the cut-note value, turn the volume to
		   zero (just like on the amiga) */
		if (tick>=nib)
			a->tmpvolume=0; /* just turn the volume down */
		break;
	case 0xd: /* note delay */
		/* delay the start of the sample until tick==nib */
		if (!tick)
			a->main.notedelay=nib;
		else if (a->main.notedelay)
			a->main.notedelay--;
		break;
	case 0xe: /* pattern delay */
		if (!tick)
			if (!mod->patdly2)
				mod->patdly=nib+1; /* only once, when tick=0 */
		break;
	case 0xf: /* invert loop, not supported  */
		break;
	}
}

static int DoPTEffectE(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoEEffects(tick, flags, a, mod, channel, UniGetByte());

	return 0;
}

static int DoPTEffectF(UWORD tick, UWORD flags, MP_CONTROL*, MODULE *mod, SWORD )
{
	UBYTE dat;

	dat=UniGetByte();
	if (tick||mod->patdly2) return 0;
	if (mod->extspd&&(dat>=mod->bpmlimit))
		mod->bpm=dat;
	else
	  if (dat) {
		mod->sngspd=(dat>=mod->bpmlimit)?mod->bpmlimit-1:dat;
		mod->vbtick=0;
	}

	return 0;
}

/*========== Scream Tracker effects */

static int DoS3MEffectA(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE speed;

	speed = UniGetByte();

	if (tick || mod->patdly2)
		return 0;

	if (speed > 128)
		speed -= 128;
	if (speed) {
		mod->sngspd = speed;
		mod->vbtick = 0;
	}

	return 0;
}

static void DoS3MVolSlide(UWORD tick, UWORD flags, MP_CONTROL *a, UBYTE inf)
{
	UBYTE lo, hi;

	if (inf)
		a->s3mvolslide=inf;
	else
		inf=a->s3mvolslide;

	lo=inf&0xf;
	hi=inf>>4;

	if (!lo) {
    if ((tick)||(flags&UF_S3MSLIDES)) a->tmpvolume = a->tmpvolume + hi;
	} else
	  if (!hi) {
      if ((tick)||(flags&UF_S3MSLIDES)) a->tmpvolume = a->tmpvolume - lo;
	} else
	  if (lo==0xf) {
		if (!tick) a->tmpvolume+=(hi?hi:0xf);
	} else
	  if (hi==0xf) {
		if (!tick) a->tmpvolume-=(lo?lo:0xf);
	} else
	  return;

	if (a->tmpvolume<0)
		a->tmpvolume=0;
	else if (a->tmpvolume>64)
		a->tmpvolume=64;
}

static int DoS3MEffectD(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoS3MVolSlide(tick, flags, a, UniGetByte());

	return 1;
}

static void DoS3MSlideDn(UWORD tick, MP_CONTROL *a, UBYTE inf)
{
	UBYTE hi,lo;

	if (inf)
		a->slidespeed=inf;
	else
		inf=(UBYTE)a->slidespeed;

	hi=inf>>4;
	lo=inf&0xf;

	if (hi==0xf) {
		if (!tick) a->tmpperiod+=(UWORD)lo<<2;
	} else
	  if (hi==0xe) {
      if (!tick) a->tmpperiod = a->tmpperiod + lo;
	} else {
		if (tick) a->tmpperiod+=(UWORD)inf<<2;
	}
}

static int DoS3MEffectE(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (a->main.period)
		DoS3MSlideDn(tick, a,dat);

	return 0;
}

static void DoS3MSlideUp(UWORD tick, MP_CONTROL *a, UBYTE inf)
{
	UBYTE hi,lo;

	if (inf) a->slidespeed=inf;
	else inf=(UBYTE)a->slidespeed;

	hi=inf>>4;
	lo=inf&0xf;

	if (hi==0xf) {
		if (!tick) a->tmpperiod-=(UWORD)lo<<2;
	} else
	  if (hi==0xe) {
      if (!tick) a->tmpperiod = a->tmpperiod - lo;
	} else {
		if (tick) a->tmpperiod-=(UWORD)inf<<2;
	}
}

static int DoS3MEffectF(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (a->main.period)
		DoS3MSlideUp(tick, a,dat);

	return 0;
}

static int DoS3MEffectI(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, on, off;

	inf = UniGetByte();
	if (inf)
		a->s3mtronof = inf;
	else {
		inf = a->s3mtronof;
		if (!inf)
			return 0;
	}

	if (!tick)
		return 0;

	on=(inf>>4)+1;
	off=(inf&0xf)+1;
	a->s3mtremor%=(on+off);
	a->volume=(a->s3mtremor<on)?a->tmpvolume:0;
	a->ownvol=1;
	a->s3mtremor++;

	return 0;
}

static int DoS3MEffectQ(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf;

	inf = UniGetByte();
	if (a->main.period) {
		if (inf) {
			a->s3mrtgslide=inf>>4;
			a->s3mrtgspeed=inf&0xf;
		}

		/* only retrigger if low nibble > 0 */
		if (a->s3mrtgspeed>0) {
			if (!a->retrig) {
				/* when retrig counter reaches 0, reset counter and restart the
				   sample */
				if (a->main.kick!=KICK_NOTE) a->main.kick=KICK_KEYOFF;
				a->retrig=a->s3mrtgspeed;

				if ((tick)||(flags&UF_S3MSLIDES)) {
					switch (a->s3mrtgslide) {
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
						a->tmpvolume-=(1<<(a->s3mrtgslide-1));
						break;
					case 6:
						a->tmpvolume=(2*a->tmpvolume)/3;
						break;
					case 7:
						a->tmpvolume>>=1;
						break;
					case 9:
					case 0xa:
					case 0xb:
					case 0xc:
					case 0xd:
						a->tmpvolume+=(1<<(a->s3mrtgslide-9));
						break;
					case 0xe:
						a->tmpvolume=(3*a->tmpvolume)>>1;
						break;
					case 0xf:
						a->tmpvolume=a->tmpvolume<<1;
						break;
					}
					if (a->tmpvolume<0)
						a->tmpvolume=0;
					else if (a->tmpvolume>64)
						a->tmpvolume=64;
				}
			}
			a->retrig--; /* countdown  */
		}
	}

	return 0;
}

static int DoS3MEffectR(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, q;
	UWORD temp=0;	/* silence warning */

	dat = UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->trmdepth=dat&0xf;
		if (dat&0xf0) a->trmspd=(dat&0xf0)>>2;
	}

	q=(a->trmpos>>2)&0x1f;

	switch ((a->wavecontrol>>4)&3) {
	case 0: /* sine */
		temp=VibratoTable[q];
		break;
	case 1: /* ramp down */
		q<<=3;
		if (a->trmpos<0) q=255-q;
		temp=q;
		break;
	case 2: /* square wave */
		temp=255;
		break;
	case 3: /* random */
		temp= (UWORD)getrandom(256);
		break;
	}

  temp = temp * a->trmdepth;
	temp>>=7;

	if (a->trmpos>=0) {
		a->volume=a->tmpvolume+temp;
		if (a->volume>64) a->volume=64;
	} else {
		a->volume=a->tmpvolume-temp;
		if (a->volume<0) a->volume=0;
	}
	a->ownvol = 1;

	if (tick)
    a->trmpos = a->trmpos + a->trmspd;

	return 0;
}

static int DoS3MEffectT(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE tempo;

	tempo = UniGetByte();

	if (tick || mod->patdly2)
		return 0;

	mod->bpm = (tempo < 32) ? 32 : tempo;

	return 0;
}

static int DoS3MEffectU(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, q;
	UWORD temp = 0;	/* silence warning */

	dat = UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->vibdepth=dat&0xf;
		if (dat&0xf0) a->vibspd=(dat&0xf0)>>2;
	} else
		if (a->main.period) {
			q=(a->vibpos>>2)&0x1f;

			switch (a->wavecontrol&3) {
			case 0: /* sine */
				temp=VibratoTable[q];
				break;
			case 1: /* ramp down */
				q<<=3;
				if (a->vibpos<0) q=255-q;
				temp=q;
				break;
			case 2: /* square wave */
				temp=255;
				break;
			case 3: /* random */
				temp= (UWORD)getrandom(256);
				break;
			}

			temp*=a->vibdepth;
			temp>>=8;

			if (a->vibpos>=0)
				a->main.period=a->tmpperiod+temp;
			else
				a->main.period=a->tmpperiod-temp;
			a->ownper = 1;

			a->vibpos+=a->vibspd;
	}

	return 0;
}

/*========== Envelope helpers */

static int DoKeyOff(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	a->main.keyoff|=KEY_OFF;
	if ((!(a->main.volflg&EF_ON))||(a->main.volflg&EF_LOOP))
		a->main.keyoff=KEY_KILL;

	return 0;
}

static int DoKeyFade(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if ((tick>=dat)||(tick==mod->sngspd-1)) {
		a->main.keyoff=KEY_KILL;
		if (!(a->main.volflg&EF_ON))
			a->main.fadevol=0;
	}

	return 0;
}

/*========== Fast Tracker effects */

/* DoXMEffect6 after DoXMEffectA */

static int DoXMEffectA(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, lo, hi;

	inf = UniGetByte();
	if (inf)
		a->s3mvolslide = inf;
	else
		inf = a->s3mvolslide;

	if (tick) {
		lo=inf&0xf;
		hi=inf>>4;

		if (!hi) {
			a->tmpvolume-=lo;
			if (a->tmpvolume<0) a->tmpvolume=0;
		} else {
			a->tmpvolume+=hi;
			if (a->tmpvolume>64) a->tmpvolume=64;
		}
	}

	return 0;
}

static int DoXMEffect6(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	if (a->main.period)
		DoVibrato(tick, a);

	return DoXMEffectA(tick, flags, a, mod, channel);
}

static int DoXMEffectE1(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick) {
		if (dat) a->fportupspd=dat;
		if (a->main.period)
			a->tmpperiod-=(a->fportupspd<<2);
	}

	return 0;
}

static int DoXMEffectE2(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick) {
		if (dat) a->fportdnspd=dat;
		if (a->main.period)
			a->tmpperiod+=(a->fportdnspd<<2);
	}

	return 0;
}

static int DoXMEffectEA(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick)
		if (dat) a->fslideupspd=dat;
	a->tmpvolume+=a->fslideupspd;
	if (a->tmpvolume>64) a->tmpvolume=64;

	return 0;
}

static int DoXMEffectEB(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if (!tick)
		if (dat) a->fslidednspd=dat;
	a->tmpvolume-=a->fslidednspd;
	if (a->tmpvolume<0) a->tmpvolume=0;

	return 0;
}

static int DoXMEffectG(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	mod->volume=UniGetByte()<<1;
	if (mod->volume>128) mod->volume=128;

	return 0;
}

static int DoXMEffectH(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf;

	inf = UniGetByte();

	if (tick) {
		if (inf) mod->globalslide=inf;
		else inf=mod->globalslide;
		if (inf & 0xf0) inf&=0xf0;
		mod->volume=mod->volume+((inf>>4)-(inf&0xf))*2;

		if (mod->volume<0)
			mod->volume=0;
		else if (mod->volume>128)
			mod->volume=128;
	}

	return 0;
}

static int DoXMEffectL(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat=UniGetByte();
	if ((!tick)&&(a->main.i)) {
		UWORD points;
		INSTRUMENT *i=a->main.i;
		MP_VOICE *aout;

		if ((aout=a->slave)) {
			if (aout->venv.env) {
				points=i->volenv[i->volpts-1].pos;
				aout->venv.p=aout->venv.env[(dat>points)?points:dat].pos;
			}
			if (aout->penv.env) {
				points=i->panenv[i->panpts-1].pos;
				aout->penv.p=aout->penv.env[(dat>points)?points:dat].pos;
			}
		}
	}

	return 0;
}

static int DoXMEffectP(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, lo, hi;
	SWORD pan;

	inf = UniGetByte();
	if (!mod->panflag)
		return 0;

	if (inf)
		a->pansspd = inf;
	else
		inf =a->pansspd;

	if (tick) {
		lo=inf&0xf;
		hi=inf>>4;

		/* slide right has absolute priority */
		if (hi)
			lo = 0;

		pan=((a->main.panning==PAN_SURROUND)?PAN_CENTER:a->main.panning)+hi-lo;
		a->main.panning=(pan<PAN_LEFT)?PAN_LEFT:(pan>PAN_RIGHT?PAN_RIGHT:pan);
	}

	return 0;
}

static int DoXMEffectX1(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat = UniGetByte();
	if (dat)
		a->ffportupspd = dat;
	else
		dat = a->ffportupspd;

	if (a->main.period)
		if (!tick) {
			a->main.period-=dat;
			a->tmpperiod-=dat;
			a->ownper = 1;
		}

	return 0;
}

static int DoXMEffectX2(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat;

	dat = UniGetByte();
	if (dat)
		a->ffportdnspd=dat;
	else
		dat = a->ffportdnspd;

	if (a->main.period)
		if (!tick) {
			a->main.period+=dat;
			a->tmpperiod+=dat;
			a->ownper = 1;
		}

	return 0;
}

/*========== Impulse Tracker effects */

static void DoITToneSlide(UWORD tick, MP_CONTROL *a, UBYTE dat)
{
	if (dat)
		a->portspeed = dat;

	/* if we don't come from another note, ignore the slide and play the note
	   as is */
	if (!a->oldnote || !a->main.period)
			return;

	if ((!tick)&&(a->newsamp)){
		a->main.kick=KICK_NOTE;
		a->main.start=-1;
	} else
		a->main.kick=(a->main.kick==KICK_NOTE)?KICK_ENV:KICK_ABSENT;

	if (tick) {
		int dist;

		/* We have to slide a->main.period towards a->wantedperiod, compute the
		   difference between those two values */
		dist=a->main.period-a->wantedperiod;

	    /* if they are equal or if portamentospeed is too big... */
		if ((!dist)||((a->portspeed<<2)>abs(dist)))
			/* ... make tmpperiod equal tperiod */
			a->tmpperiod=a->main.period=a->wantedperiod;
		else
		  if (dist>0) {
			a->tmpperiod-=a->portspeed<<2;
			a->main.period-=a->portspeed<<2; /* dist>0 slide up */
		} else {
			a->tmpperiod+=a->portspeed<<2;
			a->main.period+=a->portspeed<<2; /* dist<0 slide down */
		}
	} else
		a->tmpperiod=a->main.period;
	a->ownper=1;
}

static int DoITEffectG(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoITToneSlide(tick, a, UniGetByte());

	return 0;
}

static void DoITVibrato(UWORD tick, MP_CONTROL *a, UBYTE dat)
{
	UBYTE q;
	UWORD temp=0;

	if (!tick) {
		if (dat&0x0f) a->vibdepth=dat&0xf;
		if (dat&0xf0) a->vibspd=(dat&0xf0)>>2;
	}
	if (!a->main.period)
			return;

	q=(a->vibpos>>2)&0x1f;

	switch (a->wavecontrol&3) {
	case 0: /* sine */
		temp=VibratoTable[q];
		break;
	case 1: /* square wave */
		temp=255;
		break;
	case 2: /* ramp down */
		q<<=3;
		if (a->vibpos<0) q=255-q;
		temp=q;
		break;
	case 3: /* random */
		temp=getrandom(256);
		break;
	}

	temp*=a->vibdepth;
	temp>>=8;
	temp<<=2;

	if (a->vibpos>=0)
		a->main.period=a->tmpperiod+temp;
	else
		a->main.period=a->tmpperiod-temp;
	a->ownper=1;

	a->vibpos+=a->vibspd;
}

static int DoITEffectH(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoITVibrato(tick, a, UniGetByte());

	return 0;
}

static int DoITEffectI(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, on, off;

	inf = UniGetByte();
	if (inf)
		a->s3mtronof = inf;
	else {
		inf = a->s3mtronof;
		if (!inf)
			return 0;
	}

	on=(inf>>4);
	off=(inf&0xf);

	a->s3mtremor%=(on+off);
	a->volume=(a->s3mtremor<on)?a->tmpvolume:0;
	a->ownvol = 1;
	a->s3mtremor++;

	return 0;
}

static int DoITEffectM(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	a->main.chanvol=UniGetByte();
	if (a->main.chanvol>64)
		a->main.chanvol=64;
	else if (a->main.chanvol<0)
		a->main.chanvol=0;

	return 0;
}

static int DoITEffectN(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, lo, hi;

	inf = UniGetByte();

	if (inf)
		a->chanvolslide = inf;
	else
		inf = a->chanvolslide;

	lo=inf&0xf;
	hi=inf>>4;

	if (!hi)
		a->main.chanvol-=lo;
	else
	  if (!lo) {
		a->main.chanvol+=hi;
	} else
	  if (hi==0xf) {
		if (!tick) a->main.chanvol-=lo;
	} else
	  if (lo==0xf) {
		if (!tick) a->main.chanvol+=hi;
	}

	if (a->main.chanvol<0)
		a->main.chanvol=0;
	else if (a->main.chanvol>64)
		a->main.chanvol=64;

	return 0;
}

static int DoITEffectP(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, lo, hi;
	SWORD pan;

	inf = UniGetByte();
	if (inf)
		a->pansspd = inf;
	else
		inf = a->pansspd;

	if (!mod->panflag)
		return 0;

	lo=inf&0xf;
	hi=inf>>4;

	pan=(a->main.panning==PAN_SURROUND)?PAN_CENTER:a->main.panning;

	if (!hi)
		pan+=lo<<2;
	else
	  if (!lo) {
		pan-=hi<<2;
	} else
	  if (hi==0xf) {
		if (!tick) pan+=lo<<2;
	} else
	  if (lo==0xf) {
		if (!tick) pan-=hi<<2;
	}
	a->main.panning=
	  (pan<PAN_LEFT)?PAN_LEFT:(pan>PAN_RIGHT?PAN_RIGHT:pan);

	return 0;
}

static int DoITEffectT(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE tempo;
	SWORD temp;

   	tempo = UniGetByte();

	if (mod->patdly2)
		return 0;

	temp = mod->bpm;
	if (tempo & 0x10)
		temp += (tempo & 0x0f);
	else
		temp -= tempo;

	mod->bpm=(temp>255)?255:(temp<1?1:temp);

	return 0;
}

static int DoITEffectU(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, q;
	UWORD temp = 0;	/* silence warning */

	dat = UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->vibdepth=dat&0xf;
		if (dat&0xf0) a->vibspd=(dat&0xf0)>>2;
	}
	if (a->main.period) {
		q=(a->vibpos>>2)&0x1f;

		switch (a->wavecontrol&3) {
		case 0: /* sine */
			temp=VibratoTable[q];
			break;
		case 1: /* square wave */
			temp=255;
			break;
		case 2: /* ramp down */
			q<<=3;
			if (a->vibpos<0) q=255-q;
			temp=q;
			break;
		case 3: /* random */
			temp=getrandom(256);
			break;
		}

		temp*=a->vibdepth;
		temp>>=8;

		if (a->vibpos>=0)
			a->main.period=a->tmpperiod+temp;
		else
			a->main.period=a->tmpperiod-temp;
		a->ownper = 1;

		a->vibpos+=a->vibspd;
	}

	return 0;
}

static int DoITEffectW(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE inf, lo, hi;

	inf = UniGetByte();

	if (inf)
		mod->globalslide = inf;
	else
		inf = mod->globalslide;

	lo=inf&0xf;
	hi=inf>>4;

	if (!lo) {
		if (tick) mod->volume+=hi;
	} else
	  if (!hi) {
		if (tick) mod->volume-=lo;
	} else
	  if (lo==0xf) {
		if (!tick) mod->volume+=hi;
	} else
	  if (hi==0xf) {
		if (!tick) mod->volume-=lo;
	}

	if (mod->volume<0)
		mod->volume=0;
	else if (mod->volume>128)
		mod->volume=128;

	return 0;
}

static int DoITEffectY(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, q;
	SLONG temp = 0;	/* silence warning */


	dat=UniGetByte();
	if (!tick) {
		if (dat&0x0f) a->panbdepth=(dat&0xf);
		if (dat&0xf0) a->panbspd=(dat&0xf0)>>4;
	}
	if (mod->panflag) {
		q=a->panbpos;

		switch (a->panbwave) {
		case 0: /* sine */
			temp=PanbrelloTable[q];
			break;
		case 1: /* square wave */
			temp=(q<0x80)?64:0;
			break;
		case 2: /* ramp down */
			q<<=3;
			temp=q;
			break;
		case 3: /* random */
			temp=getrandom(256);
			break;
		}

		temp*=a->panbdepth;
		temp=(temp/8)+mod->panning[channel];

		a->main.panning=(SWORD)(
			(temp<PAN_LEFT)?PAN_LEFT:(temp>PAN_RIGHT?PAN_RIGHT:temp) );
		a->panbpos+=a->panbspd;

	}

	return 0;
}

static void DoNNAEffects(MODULE *, MP_CONTROL *, UBYTE);

/* Impulse/Scream Tracker Sxx effects.
   All Sxx effects share the same memory space. */
static int DoITEffectS0(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, inf, c;

	dat = UniGetByte();
	inf=dat&0xf;
	c=dat>>4;

	if (!dat) {
		c=a->sseffect;
		inf=a->ssdata;
	} else {
		a->sseffect=c;
		a->ssdata=inf;
	}

	switch (c) {
	case SS_GLISSANDO: /* S1x set glissando voice */
		DoEEffects(tick, flags, a, mod, channel, 0x30|inf);
		break;
	case SS_FINETUNE: /* S2x set finetune */
		DoEEffects(tick, flags, a, mod, channel, 0x50|inf);
		break;
	case SS_VIBWAVE: /* S3x set vibrato waveform */
		DoEEffects(tick, flags, a, mod, channel, 0x40|inf);
		break;
	case SS_TREMWAVE: /* S4x set tremolo waveform */
		DoEEffects(tick, flags, a, mod, channel, 0x70|inf);
		break;
	case SS_PANWAVE: /* S5x panbrello */
		a->panbwave=inf;
		break;
	case SS_FRAMEDELAY: /* S6x delay x number of frames (patdly) */
		DoEEffects(tick, flags, a, mod, channel, 0xe0|inf);
		break;
	case SS_S7EFFECTS: /* S7x instrument / NNA commands */
		DoNNAEffects(mod, a, inf);
		break;
	case SS_PANNING: /* S8x set panning position */
		DoEEffects(tick, flags, a, mod, channel, 0x80 | inf);
		break;
	case SS_SURROUND: /* S9x set surround sound */
		if (mod->panflag)
			a->main.panning = mod->panning[channel] = PAN_SURROUND;
		break;
	case SS_HIOFFSET: /* SAy set high order sample offset yxx00h */
		if (!tick) {
			a->hioffset=inf<<16;
			a->main.start=a->hioffset|a->soffset;

			if ((a->main.s)&&((ULONG)a->main.start>a->main.s->length))
				a->main.start=a->main.s->flags&(SF_LOOP|SF_BIDI)?
				    a->main.s->loopstart:a->main.s->length;
		}
		break;
	case SS_PATLOOP: /* SBx pattern loop */
		DoEEffects(tick, flags, a, mod, channel, 0x60|inf);
		break;
	case SS_NOTECUT: /* SCx notecut */
		if (!inf) inf = 1;
		DoEEffects(tick, flags, a, mod, channel, 0xC0|inf);
		break;
	case SS_NOTEDELAY: /* SDx notedelay */
		DoEEffects(tick, flags, a, mod, channel, 0xD0|inf);
		break;
	case SS_PATDELAY: /* SEx patterndelay */
		DoEEffects(tick, flags, a, mod, channel, 0xE0|inf);
		break;
	}

	return 0;
}

/*========== Impulse Tracker Volume/Pan Column effects */

/*
 * All volume/pan column effects share the same memory space.
 */

static int DoVolEffects(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE c, inf;

	c = UniGetByte();
	inf = UniGetByte();

	if ((!c)&&(!inf)) {
		c=a->voleffect;
		inf=a->voldata;
	} else {
		a->voleffect=c;
		a->voldata=inf;
	}

	if (c)
		switch (c) {
		case VOL_VOLUME:
			if (tick) break;
			if (inf>64) inf=64;
			a->tmpvolume=inf;
			break;
		case VOL_PANNING:
			if (mod->panflag)
				a->main.panning=inf;
			break;
		case VOL_VOLSLIDE:
			DoS3MVolSlide(tick, flags, a, inf);
			return 1;
		case VOL_PITCHSLIDEDN:
			if (a->main.period)
				DoS3MSlideDn(tick, a, inf);
			break;
		case VOL_PITCHSLIDEUP:
			if (a->main.period)
				DoS3MSlideUp(tick, a, inf);
			break;
		case VOL_PORTAMENTO:
			DoITToneSlide(tick, a, inf);
			break;
		case VOL_VIBRATO:
			DoITVibrato(tick, a, inf);
			break;
	}

	return 0;
}

/*========== UltraTracker effects */

static int DoULTEffect9(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UWORD offset=UniGetWord();

	if (offset)
		a->ultoffset=offset;

	a->main.start=a->ultoffset<<2;
	if ((a->main.s)&&((ULONG)a->main.start>a->main.s->length))
		a->main.start=a->main.s->flags&(SF_LOOP|SF_BIDI)?
		    a->main.s->loopstart:a->main.s->length;

	return 0;
}

/*========== OctaMED effects */

static int DoMEDSpeed(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UWORD speed=UniGetWord();

	mod->bpm=speed;

	return 0;
}

static int DoMEDEffectF1(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoEEffects(tick, flags, a, mod, channel, 0x90|(mod->sngspd/2));

	return 0;
}

static int DoMEDEffectF2(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoEEffects(tick, flags, a, mod, channel, 0xd0|(mod->sngspd/2));

	return 0;
}

static int DoMEDEffectF3(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	DoEEffects(tick, flags, a, mod, channel, 0x90|(mod->sngspd/3));

	return 0;
}

/*========== Oktalyzer effects */

static int DoOktArp(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UBYTE dat, dat2;

	dat2 = UniGetByte();	/* arpeggio style */
	dat = UniGetByte();
	if (!tick) {
		if (!dat && (flags & UF_ARPMEM))
			dat=a->arpmem;
		else
			a->arpmem=dat;
	}
	if (a->main.period)
		DoArpeggio(tick, flags, a, dat2);

	return 0;
}

/*========== General player functions */

static int DoNothing(UWORD tick, UWORD flags, MP_CONTROL *a, MODULE *mod, SWORD channel)
{
	UniSkipOpcode();

	return 0;
}

typedef int (*effect_func) (UWORD, UWORD, MP_CONTROL *, MODULE *, SWORD);

static effect_func effects[UNI_LAST] = {
		DoNothing,		/* 0 */
		DoNothing,		/* UNI_NOTE */
		DoNothing,		/* UNI_INSTRUMENT */
		DoPTEffect0,	/* UNI_PTEFFECT0 */
		DoPTEffect1,	/* UNI_PTEFFECT1 */
		DoPTEffect2,	/* UNI_PTEFFECT2 */
		DoPTEffect3,	/* UNI_PTEFFECT3 */
		DoPTEffect4,	/* UNI_PTEFFECT4 */
		DoPTEffect5,	/* UNI_PTEFFECT5 */
		DoPTEffect6,	/* UNI_PTEFFECT6 */
		DoPTEffect7,	/* UNI_PTEFFECT7 */
		DoPTEffect8,	/* UNI_PTEFFECT8 */
		DoPTEffect9,	/* UNI_PTEFFECT9 */
		DoPTEffectA,	/* UNI_PTEFFECTA */
		DoPTEffectB,	/* UNI_PTEFFECTB */
		DoPTEffectC,	/* UNI_PTEFFECTC */
		DoPTEffectD,	/* UNI_PTEFFECTD */
		DoPTEffectE,	/* UNI_PTEFFECTE */
		DoPTEffectF,	/* UNI_PTEFFECTF */
		DoS3MEffectA,	/* UNI_S3MEFFECTA */
		DoS3MEffectD,	/* UNI_S3MEFFECTD */
		DoS3MEffectE,	/* UNI_S3MEFFECTE */
		DoS3MEffectF,	/* UNI_S3MEFFECTF */
		DoS3MEffectI,	/* UNI_S3MEFFECTI */
		DoS3MEffectQ,	/* UNI_S3MEFFECTQ */
		DoS3MEffectR,	/* UNI_S3MEFFECTR */
		DoS3MEffectT,	/* UNI_S3MEFFECTT */
		DoS3MEffectU,	/* UNI_S3MEFFECTU */
		DoKeyOff,		/* UNI_KEYOFF */
		DoKeyFade,		/* UNI_KEYFADE */
		DoVolEffects,	/* UNI_VOLEFFECTS */
		DoPTEffect4,	/* UNI_XMEFFECT4 */
		DoXMEffect6,	/* UNI_XMEFFECT6 */
		DoXMEffectA,	/* UNI_XMEFFECTA */
		DoXMEffectE1,	/* UNI_XMEFFECTE1 */
		DoXMEffectE2,	/* UNI_XMEFFECTE2 */
		DoXMEffectEA,	/* UNI_XMEFFECTEA */
		DoXMEffectEB,	/* UNI_XMEFFECTEB */
		DoXMEffectG,	/* UNI_XMEFFECTG */
		DoXMEffectH,	/* UNI_XMEFFECTH */
		DoXMEffectL,	/* UNI_XMEFFECTL */
		DoXMEffectP,	/* UNI_XMEFFECTP */
		DoXMEffectX1,	/* UNI_XMEFFECTX1 */
		DoXMEffectX2,	/* UNI_XMEFFECTX2 */
		DoITEffectG,	/* UNI_ITEFFECTG */
		DoITEffectH,	/* UNI_ITEFFECTH */
		DoITEffectI,	/* UNI_ITEFFECTI */
		DoITEffectM,	/* UNI_ITEFFECTM */
		DoITEffectN,	/* UNI_ITEFFECTN */
		DoITEffectP,	/* UNI_ITEFFECTP */
		DoITEffectT,	/* UNI_ITEFFECTT */
		DoITEffectU,	/* UNI_ITEFFECTU */
		DoITEffectW,	/* UNI_ITEFFECTW */
		DoITEffectY,	/* UNI_ITEFFECTY */
		DoNothing,		/* UNI_ITEFFECTZ */
		DoITEffectS0,	/* UNI_ITEFFECTS0 */
		DoULTEffect9,	/* UNI_ULTEFFECT9 */
		DoMEDSpeed,		/* UNI_MEDSPEED */
		DoMEDEffectF1,	/* UNI_MEDEFFECTF1 */
		DoMEDEffectF2,	/* UNI_MEDEFFECTF2 */
		DoMEDEffectF3,	/* UNI_MEDEFFECTF3 */
		DoOktArp,		/* UNI_OKTARP */
};

static int pt_playeffects(MODULE *mod, SWORD channel, MP_CONTROL *a)
{
	UWORD tick = mod->vbtick;
	UWORD flags = mod->flags;
	UBYTE c;
	int explicitslides = 0;
	effect_func f;

	while((c=UniGetByte())) {
		f = effects[c];
		if (f != DoNothing)
				a->sliding = 0;
		explicitslides |= f(tick, flags, a, mod, channel);
	}
	return explicitslides;
}

static void DoNNAEffects(MODULE *mod, MP_CONTROL *a, UBYTE dat)
{
	int t;
	MP_VOICE *aout;

	dat&=0xf;
	aout=(a->slave)?a->slave:NULL;

	switch (dat) {
	case 0x0: /* past note cut */
		for (t=0;t<md_sngchn;t++)
			if (mod->voice[t].master==a)
				mod->voice[t].main.fadevol=0;
		break;
	case 0x1: /* past note off */
		for (t=0;t<md_sngchn;t++)
			if (mod->voice[t].master==a) {
				mod->voice[t].main.keyoff|=KEY_OFF;
				if ((!(mod->voice[t].venv.flg & EF_ON))||
				   (mod->voice[t].venv.flg & EF_LOOP))
					mod->voice[t].main.keyoff=KEY_KILL;
			}
		break;
	case 0x2: /* past note fade */
		for (t=0;t<md_sngchn;t++)
			if (mod->voice[t].master==a)
				mod->voice[t].main.keyoff|=KEY_FADE;
		break;
	case 0x3: /* set NNA note cut */
		a->main.nna=(a->main.nna&~NNA_MASK)|NNA_CUT;
		break;
	case 0x4: /* set NNA note continue */
		a->main.nna=(a->main.nna&~NNA_MASK)|NNA_CONTINUE;
		break;
	case 0x5: /* set NNA note off */
		a->main.nna=(a->main.nna&~NNA_MASK)|NNA_OFF;
		break;
	case 0x6: /* set NNA note fade */
		a->main.nna=(a->main.nna&~NNA_MASK)|NNA_FADE;
		break;
	case 0x7: /* disable volume envelope */
		if (aout)
			aout->main.volflg&=~EF_ON;
		break;
	case 0x8: /* enable volume envelope  */
		if (aout)
			aout->main.volflg|=EF_ON;
		break;
	case 0x9: /* disable panning envelope */
		if (aout)
			aout->main.panflg&=~EF_ON;
		break;
	case 0xa: /* enable panning envelope */
		if (aout)
			aout->main.panflg|=EF_ON;
		break;
	case 0xb: /* disable pitch envelope */
		if (aout)
			aout->main.pitflg&=~EF_ON;
		break;
	case 0xc: /* enable pitch envelope */
		if (aout)
			aout->main.pitflg|=EF_ON;
		break;
	}
}

void pt_UpdateVoices(MODULE *mod, int max_volume)
{
	SWORD envpan,envvol,envpit,channel;
	UWORD playperiod;
	SLONG vibval,vibdpt;
	ULONG tmpvol;

	MP_VOICE *aout;
	INSTRUMENT *i;
	SAMPLE *s;

	mod->totalchn=mod->realchn=0;
	for (channel=0;channel<md_sngchn;channel++) {
		aout=&mod->voice[channel];
		i=aout->main.i;
		s=aout->main.s;

		if (!s || !s->length) continue;

		if (aout->main.period<40)
			aout->main.period=40;
		else if (aout->main.period>50000)
			aout->main.period=50000;

		if ((aout->main.kick==KICK_NOTE)||(aout->main.kick==KICK_KEYOFF)) {
			Voice_Play_internal((SBYTE)channel,s,(aout->main.start==-1)?
			    ((s->flags&SF_UST_LOOP)?s->loopstart:0):aout->main.start);
			aout->main.fadevol=32768;
			aout->aswppos=0;
		}

		envvol = 256;
		envpan = PAN_CENTER;
		envpit = 32;
		if (i && ((aout->main.kick==KICK_NOTE)||(aout->main.kick==KICK_ENV))) {
			if (aout->main.volflg & EF_ON)
				envvol = StartEnvelope(&aout->venv,aout->main.volflg,
				  i->volpts,i->volsusbeg,i->volsusend,
				  i->volbeg,i->volend,i->volenv,aout->main.keyoff);
			if (aout->main.panflg & EF_ON)
				envpan = StartEnvelope(&aout->penv,aout->main.panflg,
				  i->panpts,i->pansusbeg,i->pansusend,
				  i->panbeg,i->panend,i->panenv,aout->main.keyoff);
			if (aout->main.pitflg & EF_ON)
				envpit = StartEnvelope(&aout->cenv,aout->main.pitflg,
				  i->pitpts,i->pitsusbeg,i->pitsusend,
				  i->pitbeg,i->pitend,i->pitenv,aout->main.keyoff);

			if (aout->cenv.flg & EF_ON)
				aout->masterperiod=GetPeriod(mod->flags,
				  (UWORD)aout->main.note<<1, aout->master->speed);
		} else {
			if (aout->main.volflg & EF_ON)
				envvol = ProcessEnvelope(aout,&aout->venv,256);
			if (aout->main.panflg & EF_ON)
				envpan = ProcessEnvelope(aout,&aout->penv,PAN_CENTER);
			if (aout->main.pitflg & EF_ON)
				envpit = ProcessEnvelope(aout,&aout->cenv,32);
		}
		if (aout->main.kick == KICK_NOTE) {
			aout->main.kick_flag = 1;
		}
		aout->main.kick=KICK_ABSENT;

		tmpvol = aout->main.fadevol;	/* max 32768 */
		tmpvol *= aout->main.chanvol;	/* * max 64 */
		tmpvol *= aout->main.outvolume;	/* * max 256 */
		tmpvol /= (256 * 64);			/* tmpvol is max 32768 again */
		aout->totalvol = tmpvol >> 2;	/* used to determine samplevolume */
		tmpvol *= envvol;				/* * max 256 */
		tmpvol *= mod->volume;			/* * max 128 */
		tmpvol /= (128 * 256 * 128);

		/* fade out */
		if (mod->sngpos>=mod->numpos)
			tmpvol=0;
		else
			tmpvol=(tmpvol*max_volume)/128;

		if ((aout->masterchn!=-1)&& mod->control[aout->masterchn].muted)
			Voice_SetVolume_internal((SBYTE)channel,0);
		else {
			Voice_SetVolume_internal((SBYTE)channel,(UWORD)tmpvol);
			if ((tmpvol)&&(aout->master)&&(aout->master->slave==aout))
				mod->realchn++;
			mod->totalchn++;
		}

		if (aout->main.panning==PAN_SURROUND)
			Voice_SetPanning_internal((SBYTE)channel,PAN_SURROUND);
		else
			if ((mod->panflag)&&(aout->penv.flg & EF_ON))
				Voice_SetPanning_internal((SBYTE)channel,
				    DoPan(envpan,aout->main.panning));
			else
				Voice_SetPanning_internal((SBYTE)channel,aout->main.panning);

		if (aout->main.period && s->vibdepth)
			switch (s->vibtype) {
			case 0:
				vibval=avibtab[s->avibpos&127];
				if (aout->avibpos & 0x80) vibval=-vibval;
				break;
			case 1:
				vibval=64;
				if (aout->avibpos & 0x80) vibval=-vibval;
				break;
			case 2:
				vibval=63-(((aout->avibpos+128)&255)>>1);
				break;
			default:
				vibval=(((aout->avibpos+128)&255)>>1)-64;
				break;
			}
		else
			vibval=0;

		if (s->vibflags & AV_IT) {
			if ((aout->aswppos>>8)<s->vibdepth) {
				aout->aswppos += s->vibsweep;
				vibdpt=aout->aswppos;
			} else
				vibdpt=s->vibdepth<<8;
			vibval=(vibval*vibdpt)>>16;
			if (aout->mflag) {
				if (!(mod->flags&UF_LINEAR)) vibval>>=1;
				aout->main.period-=(SWORD)vibval;
			}
		} else {
			/* do XM style auto-vibrato */
			if (!(aout->main.keyoff & KEY_OFF)) {
				if (aout->aswppos<s->vibsweep) {
					vibdpt=(aout->aswppos*s->vibdepth)/s->vibsweep;
					aout->aswppos++;
				} else
					vibdpt=s->vibdepth;
			} else {
				/* keyoff -> depth becomes 0 if final depth wasn't reached or
				   stays at final level if depth WAS reached */
				if (aout->aswppos>=s->vibsweep)
					vibdpt=s->vibdepth;
				else
					vibdpt=0;
			}
			vibval=(vibval*vibdpt)>>8;
			aout->main.period-=(SWORD)vibval;
		}

		/* update vibrato position */
		aout->avibpos=(aout->avibpos+s->vibrate)&0xff;

		/* process pitch envelope */
		playperiod=aout->main.period;

		if ((aout->main.pitflg&EF_ON)&&(envpit!=32)) {
			long p1;

			envpit-=32;
			if ((aout->main.note<<1)+envpit<=0) envpit=-(aout->main.note<<1);

			p1=GetPeriod(mod->flags, ((UWORD)aout->main.note<<1)+envpit,
			    aout->master->speed)-aout->masterperiod;
			if (p1>0) {
				if ((UWORD)(playperiod+p1)<=playperiod) {
					p1=0;
					aout->main.keyoff|=KEY_OFF;
				}
			} else if (p1<0) {
				if ((UWORD)(playperiod+p1)>=playperiod) {
					p1=0;
					aout->main.keyoff|=KEY_OFF;
				}
			}
			playperiod+= (UWORD)p1;
		}

		if (!aout->main.fadevol) { /* check for a dead note (fadevol=0) */
			Voice_Stop_internal((SBYTE)channel);
			mod->totalchn--;
			if ((tmpvol)&&(aout->master)&&(aout->master->slave==aout))
				mod->realchn--;
		} else {
			Voice_SetFrequency_internal((SBYTE)channel,
			                            getfrequency(mod->flags,playperiod));

			/* if keyfade, start substracting fadeoutspeed from fadevol: */
			if ((i)&&(aout->main.keyoff&KEY_FADE)) {
				if (aout->main.fadevol>=i->volfade)
					aout->main.fadevol-=i->volfade;
				else
					aout->main.fadevol=0;
			}
		}

		md_bpm=mod->bpm+mod->relspd;
		if (md_bpm<32)
			md_bpm=32;
		else if ((!(mod->flags&UF_HIGHBPM)) && md_bpm>255)
			md_bpm=255;
	}
}

/* Handles new notes or instruments */
void pt_Notes(MODULE *mod)
{
	SWORD channel;
	MP_CONTROL *a;
	UBYTE c,inst;
	int tr,funky; /* funky is set to indicate note or instrument change */

	for (channel=0;channel<mod->numchn;channel++) {
		a=&mod->control[channel];

		if (mod->sngpos>=mod->numpos) {
			tr=mod->numtrk;
			mod->numrow=0;
		} else {
			tr=mod->patterns[(mod->positions[mod->sngpos]*mod->numchn)+channel];
			mod->numrow=mod->pattrows[mod->positions[mod->sngpos]];
		}

		a->row=(tr<mod->numtrk)?UniFindRow(mod->tracks[tr],mod->patpos):NULL;
		a->newsamp=0;
		if (!mod->vbtick) a->main.notedelay=0;

		if (!a->row) continue;
		UniSetRow(a->row);
		funky=0;

		while((c=UniGetByte()))
			switch (c) {
			case UNI_NOTE:
				funky|=1;
				a->oldnote=a->anote,a->anote=UniGetByte();
				a->main.kick =KICK_NOTE;
				a->main.start=-1;
				a->sliding=0;

				/* retrig tremolo and vibrato waves ? */
				if (!(a->wavecontrol & 0x80)) a->trmpos=0;
				if (!(a->wavecontrol & 0x08)) a->vibpos=0;
				if (!a->panbwave) a->panbpos=0;
				break;
			case UNI_INSTRUMENT:
				inst=UniGetByte();
				if (inst>=mod->numins) break; /* safety valve */
				funky|=2;
				a->main.i=(mod->flags & UF_INST)?&mod->instruments[inst]:NULL;
				a->retrig=0;
				a->s3mtremor=0;
				a->ultoffset=0;
				a->main.sample=inst;
				break;
			default:
				UniSkipOpcode();
				break;
			}

		if (funky) {
			INSTRUMENT *i;
			SAMPLE *s;

			if ((i=a->main.i)) {
				if (i->samplenumber[a->anote] >= mod->numsmp) continue;
				s=&mod->samples[i->samplenumber[a->anote]];
				a->main.note=i->samplenote[a->anote];
			} else {
				a->main.note=a->anote;
				s=&mod->samples[a->main.sample];
			}

			if (a->main.s!=s) {
				a->main.s=s;
				a->newsamp=a->main.period;
			}

			/* channel or instrument determined panning ? */
			a->main.panning=mod->panning[channel];
			if (s->flags & SF_OWNPAN)
				a->main.panning=s->panning;
			else if ((i)&&(i->flags & IF_OWNPAN))
				a->main.panning=i->panning;

			a->main.handle=s->handle;
			a->speed=s->speed;

			if (i) {
				if ((mod->panflag)&&(i->flags & IF_PITCHPAN)
				   &&(a->main.panning!=PAN_SURROUND)){
					a->main.panning+=
					    ((a->anote-i->pitpancenter)*i->pitpansep)/8;
					if (a->main.panning<PAN_LEFT)
						a->main.panning=PAN_LEFT;
					else if (a->main.panning>PAN_RIGHT)
						a->main.panning=PAN_RIGHT;
				}
				a->main.pitflg=i->pitflg;
				a->main.volflg=i->volflg;
				a->main.panflg=i->panflg;
				a->main.nna=i->nnatype;
				a->dca=i->dca;
				a->dct=i->dct;
			} else {
				a->main.pitflg=a->main.volflg=a->main.panflg=0;
				a->main.nna=a->dca=0;
				a->dct=DCT_OFF;
			}

			if (funky&2) /* instrument change */ {
				/* IT random volume variations: 0:8 bit fixed, and one bit for
				   sign. */
				a->volume=a->tmpvolume=s->volume;
				if ((s)&&(i)) {
					if (i->rvolvar) {
						a->volume=a->tmpvolume=(SWORD)( s->volume+
						  ((s->volume*((SLONG)i->rvolvar*(SLONG)getrandom(512)
						  ))/25600) );
						if (a->volume<0)
							a->volume=a->tmpvolume=0;
						else if (a->volume>64)
							a->volume=a->tmpvolume=64;
					}
					if ((mod->panflag)&&(a->main.panning!=PAN_SURROUND)) {
						a->main.panning+= (SWORD)( ((a->main.panning*((SLONG)i->rpanvar*
						  (SLONG)getrandom(512)))/25600) );
						if (a->main.panning<PAN_LEFT)
							a->main.panning=PAN_LEFT;
						else if (a->main.panning>PAN_RIGHT)
							a->main.panning=PAN_RIGHT;
					}
				}
			}

			a->wantedperiod=a->tmpperiod=
			    GetPeriod(mod->flags, (UWORD)a->main.note<<1,a->speed);
			a->main.keyoff=KEY_KICK;
		}
	}
}

/* Handles effects */
void pt_EffectsPass1(MODULE *mod)
{
	SWORD channel;
	MP_CONTROL *a;
	MP_VOICE *aout;
	int explicitslides;

	for (channel=0;channel<mod->numchn;channel++) {
		a=&mod->control[channel];

		if ((aout=a->slave)) {
			a->main.fadevol=aout->main.fadevol;
			a->main.period=aout->main.period;
			if (a->main.kick==KICK_KEYOFF)
				a->main.keyoff=aout->main.keyoff;
		}

		if (!a->row) continue;
		UniSetRow(a->row);

		a->ownper=a->ownvol=0;
		explicitslides = pt_playeffects(mod, channel, a);

		/* continue volume slide if necessary for XM and IT */
		if (mod->flags&UF_BGSLIDES) {
			if (!explicitslides && a->sliding)
				DoS3MVolSlide(mod->vbtick, mod->flags, a, 0);
			else if (a->tmpvolume)
				a->sliding = explicitslides;
		}

		if (!a->ownper)
			a->main.period=a->tmpperiod;
		if (!a->ownvol)
			a->volume=a->tmpvolume;

		if (a->main.s) {
			if (a->main.i)
				a->main.outvolume=
				    (a->volume*a->main.s->globvol*a->main.i->globvol)>>10;
			else
				a->main.outvolume=(a->volume*a->main.s->globvol)>>4;
			if (a->main.outvolume>256)
				a->main.outvolume=256;
			else if (a->main.outvolume<0)
				a->main.outvolume=0;
		}
	}
}

/* NNA management */
void pt_NNA(MODULE *mod)
{
	SWORD channel;
	MP_CONTROL *a;

	for (channel=0;channel<mod->numchn;channel++) {
		a=&mod->control[channel];

		if (a->main.kick==KICK_NOTE) {
			BOOL kill=0;

			if (a->slave) {
				MP_VOICE *aout;

				aout=a->slave;
				if (aout->main.nna & NNA_MASK) {
					/* Make sure the old MP_VOICE channel knows it has no
					   master now ! */
					a->slave=NULL;
					/* assume the channel is taken by NNA */
					aout->mflag=0;

					switch (aout->main.nna) {
					case NNA_CONTINUE: /* continue note, do nothing */
						break;
					case NNA_OFF: /* note off */
						aout->main.keyoff|=KEY_OFF;
						if ((!(aout->main.volflg & EF_ON))||
							  (aout->main.volflg & EF_LOOP))
							aout->main.keyoff=KEY_KILL;
						break;
					case NNA_FADE:
						aout->main.keyoff |= KEY_FADE;
						break;
					}
				}
			}

			if (a->dct!=DCT_OFF) {
				int t;

				for (t=0;t<md_sngchn;t++)
					if ((!Voice_Stopped_internal(t))&&
					   (mod->voice[t].masterchn==channel)&&
					   (a->main.sample==mod->voice[t].main.sample)) {
						kill=0;
						switch (a->dct) {
						case DCT_NOTE:
							if (a->main.note==mod->voice[t].main.note)
								kill=1;
							break;
						case DCT_SAMPLE:
							if (a->main.handle==mod->voice[t].main.handle)
								kill=1;
							break;
						case DCT_INST:
							kill=1;
							break;
						}
						if (kill)
							switch (a->dca) {
							case DCA_CUT:
								mod->voice[t].main.fadevol=0;
								break;
							case DCA_OFF:
								mod->voice[t].main.keyoff|=KEY_OFF;
								if ((!(mod->voice[t].main.volflg&EF_ON))||
								    (mod->voice[t].main.volflg&EF_LOOP))
									mod->voice[t].main.keyoff=KEY_KILL;
								break;
							case DCA_FADE:
								mod->voice[t].main.keyoff|=KEY_FADE;
								break;
							}
					}
			}
		} /* if (a->main.kick==KICK_NOTE) */
	}
}

/* Setup module and NNA voices */
void pt_SetupVoices(MODULE *mod)
{
	SWORD channel;
	MP_CONTROL *a;
	MP_VOICE *aout;

	for (channel=0;channel<mod->numchn;channel++) {
		a=&mod->control[channel];

		if (a->main.notedelay) continue;
		if (a->main.kick==KICK_NOTE) {
			/* if no channel was cut above, find an empty or quiet channel
			   here */
			if (mod->flags&UF_NNA) {
				if (!a->slave) {
					int newchn;

					if ((newchn=MP_FindEmptyChannel(mod))!=-1)
						a->slave=&mod->voice[a->slavechn=newchn];
				}
			} else
				a->slave=&mod->voice[a->slavechn=(UBYTE)channel];

			/* assign parts of MP_VOICE only done for a KICK_NOTE */
			if ((aout=a->slave)) {
				if (aout->mflag && aout->master) aout->master->slave=NULL;
				aout->master=a;
				a->slave=aout;
				aout->masterchn=channel;
				aout->mflag=1;
			}
		} else
			aout=a->slave;

		if (aout)
			aout->main=a->main;
		a->main.kick=KICK_ABSENT;
	}
}

/* second effect pass */
void pt_EffectsPass2(MODULE *mod)
{
	SWORD channel;
	MP_CONTROL *a;
	UBYTE c;

	for (channel=0;channel<mod->numchn;channel++) {
		a=&mod->control[channel];

		if (!a->row) continue;
		UniSetRow(a->row);

		while((c=UniGetByte()))
			if (c==UNI_ITEFFECTS0) {
				c=UniGetByte();
				if ((c>>4)==SS_S7EFFECTS)
					DoNNAEffects(mod, a, c&0xf);
			} else
				UniSkipOpcode();
	}
}

void Player_HandleTick(void)
{
	SWORD channel;
	int max_volume;

#if 0
	/* don't handle the very first ticks, this allows the other hardware to
	   settle down so we don't loose any starting notes */
	if (isfirst) {
		isfirst--;
		return;
	}
#endif

	if ((!pf)||(pf->forbid)||(pf->sngpos>=pf->numpos)) return;

	/* update time counter (sngtime is in milliseconds (in fact 2^-10)) */
	pf->sngremainder+=(1<<9)*5; /* thus 2.5*(1<<10), since fps=0.4xtempo */
	pf->sngtime+=pf->sngremainder/pf->bpm;
	pf->sngremainder%=pf->bpm;

	if (++pf->vbtick>=pf->sngspd) {
		if (pf->pat_repcrazy)
			pf->pat_repcrazy=0; /* play 2 times row 0 */
		else
			pf->patpos++;
		pf->vbtick=0;

		/* process pattern-delay. pf->patdly2 is the counter and pf->patdly is
		   the command memory. */
		if (pf->patdly)
			pf->patdly2=pf->patdly,pf->patdly=0;
		if (pf->patdly2) {
			/* patterndelay active */
			if (--pf->patdly2)
				/* so turn back pf->patpos by 1 */
				if (pf->patpos) pf->patpos--;
		}

		/* do we have to get a new patternpointer ? (when pf->patpos reaches the
		   pattern size, or when a patternbreak is active) */
		if (((pf->patpos>=pf->numrow)&&(pf->numrow>0))&&(!pf->posjmp))
			pf->posjmp=3;

		if (pf->posjmp) {
			pf->patpos=pf->numrow?(pf->patbrk%pf->numrow):0;
			pf->pat_repcrazy=0;
			pf->sngpos+=(pf->posjmp-2);
			for (channel=0;channel<pf->numchn;channel++)
				pf->control[channel].pat_reppos=-1;

			pf->patbrk=pf->posjmp=0;
			/* handle the "---" (end of song) pattern since it can occur
			   *inside* the module in some formats */
			if ((pf->sngpos>=pf->numpos)||
				(pf->positions[pf->sngpos]==LAST_PATTERN)) {
				if (!pf->wrap) return;
				if (!(pf->sngpos=pf->reppos)) {
				    pf->volume=pf->initvolume>128?128:pf->initvolume;
					if(pf->initspeed!=0)
						pf->sngspd=pf->initspeed<32?pf->initspeed:32;
					else
						pf->sngspd=6;
					pf->bpm=pf->inittempo<32?32:pf->inittempo;
				}
			}
			if (pf->sngpos<0) pf->sngpos=pf->numpos-1;
		}

		if (!pf->patdly2)
			pt_Notes(pf);
	}

	/* Fade global volume if enabled and we're playing the last pattern */
	if (((pf->sngpos==pf->numpos-1)||
		 (pf->positions[pf->sngpos+1]==LAST_PATTERN))&&
	    (pf->fadeout))
		max_volume=pf->numrow?((pf->numrow-pf->patpos)*128)/pf->numrow:0;
	else
		max_volume=128;

	pt_EffectsPass1(pf);
	if (pf->flags&UF_NNA)
		pt_NNA(pf);
	pt_SetupVoices(pf);
	pt_EffectsPass2(pf);

	/* now set up the actual hardware channel playback information */
	pt_UpdateVoices(pf, max_volume);
}

static void Player_Init_internal(MODULE* mod)
{
	int t;

	for (t=0;t<mod->numchn;t++) {
		mod->control[t].main.chanvol=mod->chanvol[t];
		mod->control[t].main.panning=mod->panning[t];
	}

	mod->sngtime=0;
	mod->sngremainder=0;

	mod->pat_repcrazy=0;
	mod->sngpos=0;
	if(mod->initspeed!=0)
		mod->sngspd=mod->initspeed<32?mod->initspeed:32;
	else
		mod->sngspd=6;
	mod->volume=mod->initvolume>128?128:mod->initvolume;

	mod->vbtick=mod->sngspd;
	mod->patdly=0;
	mod->patdly2=0;
	mod->bpm=mod->inittempo<32?32:mod->inittempo;
	mod->realchn=0;

	mod->patpos=0;
	mod->posjmp=2; /* make sure the player fetches the first note */
	mod->numrow=-1;
	mod->patbrk=0;
}

BOOL Player_Init(MODULE* mod)
{
	mod->extspd=1;
	mod->panflag=1;
	mod->wrap=0;
	mod->loop=1;
	mod->fadeout=0;

	mod->relspd=0;

	/* make sure the player doesn't start with garbage */
	if (!(mod->control=(MP_CONTROL*)MikMod_calloc(mod->numchn,sizeof(MP_CONTROL))))
		return 1;
	if (!(mod->voice=(MP_VOICE*)MikMod_calloc(md_sngchn,sizeof(MP_VOICE))))
		return 1;

	Player_Init_internal(mod);
	return 0;
}

void Player_Exit_internal(MODULE* mod)
{
	if (!mod)
		return;

	/* Stop playback if necessary */
	if (mod==pf) {
		Player_Stop_internal();
		pf=NULL;
	}

	if (mod->control)
		MikMod_free(mod->control);
	if (mod->voice)
		MikMod_free(mod->voice);
	mod->control=NULL;
	mod->voice=NULL;
}

void Player_Exit(MODULE* mod)
{
	MUTEX_LOCK(vars);
	Player_Exit_internal(mod);
	MUTEX_UNLOCK(vars);
}

void Player_SetVolume(SWORD volume)
{
	MUTEX_LOCK(vars);
	if (pf)
		pf->volume=(volume<0)?0:(volume>128)?128:volume;
	MUTEX_UNLOCK(vars);
}

MODULE* Player_GetModule(void)
{
	MODULE* result;

	MUTEX_LOCK(vars);
	result=pf;
	MUTEX_UNLOCK(vars);

	return result;
}

void Player_Start(MODULE *mod)
{
	int t;

	if (!mod)
		return;

	if (!MikMod_Active())
		MikMod_EnableOutput();

	mod->forbid=0;

	MUTEX_LOCK(vars);
	if (pf!=mod) {
		/* new song is being started, so completely stop out the old one. */
		if (pf) pf->forbid=1;
		for (t=0;t<md_sngchn;t++) Voice_Stop_internal(t);
	}
	pf=mod;
	MUTEX_UNLOCK(vars);
}

void Player_Stop_internal(void)
{
	if (!md_sfxchn) MikMod_DisableOutput_internal();
	if (pf) pf->forbid=1;
	pf=NULL;
}

void Player_Stop(void)
{
	MUTEX_LOCK(vars);
		Player_Stop_internal();
	MUTEX_UNLOCK(vars);
}

BOOL Player_Active(void)
{
	BOOL result=0;

	MUTEX_LOCK(vars);
	if (pf)
		result=(!(pf->sngpos>=pf->numpos));
	MUTEX_UNLOCK(vars);

	return result;
}

void Player_NextPosition(void)
{
	MUTEX_LOCK(vars);
	if (pf) {
		int t;

		pf->forbid=1;
		pf->posjmp=3;
		pf->patbrk=0;
		pf->vbtick=pf->sngspd;

		for (t=0;t<md_sngchn;t++) {
			Voice_Stop_internal(t);
			pf->voice[t].main.i=NULL;
			pf->voice[t].main.s=NULL;
		}
		for (t=0;t<pf->numchn;t++) {
			pf->control[t].main.i=NULL;
			pf->control[t].main.s=NULL;
		}
		pf->forbid=0;
	}
	MUTEX_UNLOCK(vars);
}

void Player_PrevPosition(void)
{
	MUTEX_LOCK(vars);
	if (pf) {
		int t;

		pf->forbid=1;
		pf->posjmp=1;
		pf->patbrk=0;
		pf->vbtick=pf->sngspd;

		for (t=0;t<md_sngchn;t++) {
			Voice_Stop_internal(t);
			pf->voice[t].main.i=NULL;
			pf->voice[t].main.s=NULL;
		}
		for (t=0;t<pf->numchn;t++) {
			pf->control[t].main.i=NULL;
			pf->control[t].main.s=NULL;
		}
		pf->forbid=0;
	}
	MUTEX_UNLOCK(vars);
}

void Player_SetPosition(UWORD pos)
{
	MUTEX_LOCK(vars);
	if (pf) {
		int t;

		pf->forbid=1;
		if (pos>=pf->numpos) pos=pf->numpos;
		pf->posjmp=2;
		pf->patbrk=0;
		pf->sngpos=pos;
		pf->vbtick=pf->sngspd;

		for (t=0;t<md_sngchn;t++) {
			Voice_Stop_internal(t);
			pf->voice[t].main.i=NULL;
			pf->voice[t].main.s=NULL;
		}
		for (t=0;t<pf->numchn;t++) {
			pf->control[t].main.i=NULL;
			pf->control[t].main.s=NULL;
		}
		pf->forbid=0;

		if (!pos)
			Player_Init_internal(pf);
	}
	MUTEX_UNLOCK(vars);
}

static void Player_Unmute_internal(SLONG arg1,va_list ap)
{
	SLONG t,arg2,arg3=0;

	if (pf) {
		switch (arg1) {
		case MUTE_INCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			   (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (;arg2<pf->numchn && arg2<=arg3;arg2++)
				pf->control[arg2].muted=0;
			break;
		case MUTE_EXCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			   (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (t=0;t<pf->numchn;t++) {
				if ((t>=arg2) && (t<=arg3))
					continue;
				pf->control[t].muted=0;
			}
			break;
		default:
			if (arg1<pf->numchn) pf->control[arg1].muted=0;
			break;
		}
	}
}

void Player_Unmute(SLONG arg1, ...)
{
	va_list args;

	va_start(args,arg1);
	MUTEX_LOCK(vars);
	Player_Unmute_internal(arg1,args);
	MUTEX_UNLOCK(vars);
	va_end(args);
}

static void Player_Mute_internal(SLONG arg1,va_list ap)
{
	SLONG t,arg2,arg3=0;

	if (pf) {
		switch (arg1) {
		case MUTE_INCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			    (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (;arg2<pf->numchn && arg2<=arg3;arg2++)
				pf->control[arg2].muted=1;
			break;
		case MUTE_EXCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			    (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (t=0;t<pf->numchn;t++) {
				if ((t>=arg2) && (t<=arg3))
					continue;
				pf->control[t].muted=1;
			}
			break;
		default:
			if (arg1<pf->numchn)
				pf->control[arg1].muted=1;
			break;
		}
	}
}

void Player_Mute(SLONG arg1,...)
{
	va_list args;

	va_start(args,arg1);
	MUTEX_LOCK(vars);
	Player_Mute_internal(arg1,args);
	MUTEX_UNLOCK(vars);
	va_end(args);
}

static void Player_ToggleMute_internal(SLONG arg1,va_list ap)
{
	SLONG arg2,arg3=0;
	ULONG t;

	if (pf) {
		switch (arg1) {
		case MUTE_INCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			    (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (;arg2<pf->numchn && arg2<=arg3;arg2++)
				pf->control[arg2].muted=1-pf->control[arg2].muted;
			break;
		case MUTE_EXCLUSIVE:
			if (((!(arg2=va_arg(ap,SLONG)))&&(!(arg3=va_arg(ap,SLONG))))||
			    (arg2>arg3)||(arg3>=pf->numchn))
				return;
			for (t=0;t<pf->numchn;t++) {
				if ((t>=(ULONG)arg2) && (t<=(ULONG)arg3))
					continue;
				pf->control[t].muted=1-pf->control[t].muted;
			}
			break;
		default:
			if (arg1<pf->numchn)
				pf->control[arg1].muted=1-pf->control[arg1].muted;
			break;
		}
	}
}

void Player_ToggleMute(SLONG arg1,...)
{
	va_list args;

	va_start(args,arg1);
	MUTEX_LOCK(vars);
	Player_ToggleMute_internal(arg1,args);
	MUTEX_UNLOCK(vars);
	va_end(args);
}

BOOL Player_Muted(UBYTE chan)
{
	BOOL result=1;

	MUTEX_LOCK(vars);
	if (pf)
		result=(chan<pf->numchn)?pf->control[chan].muted:1;
	MUTEX_UNLOCK(vars);

	return result;
}

int Player_GetChannelVoice(UBYTE chan)
{
	int result=0;

	MUTEX_LOCK(vars);
	if (pf)
		result=(chan<pf->numchn)?pf->control[chan].slavechn:-1;
	MUTEX_UNLOCK(vars);

	return result;
}

UWORD Player_GetChannelPeriod(UBYTE chan)
{
	UWORD result=0;

	MUTEX_LOCK(vars);
    if (pf)
	    result=(chan<pf->numchn)?pf->control[chan].main.period:0;
	MUTEX_UNLOCK(vars);

	return result;
}

BOOL Player_Paused_internal(void)
{
	return pf?pf->forbid:1;
}

BOOL Player_Paused(void)
{
	BOOL result;

	MUTEX_LOCK(vars);
	result=Player_Paused_internal();
	MUTEX_UNLOCK(vars);

	return result;
}

void Player_TogglePause(void)
{
	MUTEX_LOCK(vars);
	if (pf)
		pf->forbid=1-pf->forbid;
	MUTEX_UNLOCK(vars);
}

void Player_SetSpeed(UWORD speed)
{
	MUTEX_LOCK(vars);
	if (pf)
		pf->sngspd=speed?(speed<32?speed:32):1;
	MUTEX_UNLOCK(vars);
}

void Player_SetTempo(UWORD tempo)
{
	if (tempo<32) tempo=32;
	MUTEX_LOCK(vars);
	if (pf) {
		if ((!(pf->flags&UF_HIGHBPM))&&(tempo>255)) tempo=255;
		pf->bpm=tempo;
	}
	MUTEX_UNLOCK(vars);
}

int Player_QueryVoices(UWORD numvoices, VOICEINFO *vinfo)
{
	int i;

	if (numvoices > md_sngchn)
		numvoices = md_sngchn;

	MUTEX_LOCK(vars);
	if (pf)
		for (i = 0; i < md_sngchn; i++) {
			vinfo [i].i = pf->voice[i].main.i;
			vinfo [i].s = pf->voice[i].main.s;
			vinfo [i].panning = pf->voice [i].main.panning;
			vinfo [i].volume = pf->voice [i].main.chanvol;
			vinfo [i].period = pf->voice [i].main.period;
			vinfo [i].kick = pf->voice [i].main.kick_flag;
			pf->voice [i].main.kick_flag = 0;
		}
	MUTEX_UNLOCK(vars);

	return numvoices;
}


// Get current module order
int Player_GetOrder(void)
{
	int ret;
	MUTEX_LOCK(vars);
	ret = pf ? pf->positions[pf->sngpos ? pf->sngpos-1 : 0]: 0;
	MUTEX_UNLOCK(vars);
	return ret;
}

// Get current module row
int Player_GetRow(void)
{
	int ret;
	MUTEX_LOCK(vars);
	ret = pf ? pf->patpos : 0;
	MUTEX_UNLOCK(vars);
	return ret;
}


CHAR *STM_Signatures[STM_NTRACKERS] = {
	"!Scream!",
	"BMOD2STM",
	"WUZAMOD!"
};

CHAR *STM_Version[STM_NTRACKERS] = {
	"Screamtracker 2",
	"Converted by MOD2STM (STM format)",
	"Wuzamod (STM format)"
};

/*========== Shared loader variables */

SBYTE  remap[UF_MAXCHAN];   /* for removing empty channels */
UBYTE* poslookup=NULL;      /* lookup table for pattern jumps after blank
                               pattern removal */
UBYTE  poslookupcnt;
UWORD* origpositions=NULL;

BOOL   filters;             /* resonant filters in use */
UBYTE  activemacro;         /* active midi macro number for Sxx,xx<80h */
UBYTE  filtermacros[UF_MAXMACRO];    /* midi macro settings */
FILTER filtersettings[UF_MAXFILTER]; /* computed filter settings */

/*========== Linear periods stuff */

int*   noteindex=NULL;      /* remap value for linear period modules */
static int noteindexcount=0;

int *AllocLinear(void)
{
	if(of.numsmp>noteindexcount) {
		noteindexcount=of.numsmp;
		noteindex=(int*)MikMod_realloc(noteindex,noteindexcount*sizeof(int));
	}
	return noteindex;
}

void FreeLinear(void)
{
	if(noteindex) {
		MikMod_free(noteindex);
		noteindex=NULL;
	}
	noteindexcount=0;
}

int speed_to_finetune(ULONG speed,int sample)
{
    int ctmp=0,tmp,note=1,finetune=0;

    speed>>=1;
    while((tmp=getfrequency(of.flags,getlinearperiod(note<<1,0)))<(int)speed) {
        ctmp=tmp;
        note++;
    }

    if(tmp!=speed) {
        if((tmp-speed)<(speed-ctmp))
            while(tmp>(int)speed)
                tmp=getfrequency(of.flags,getlinearperiod(note<<1,--finetune));
        else {
            note--;
            while(ctmp<(int)speed)
                ctmp=getfrequency(of.flags,getlinearperiod(note<<1,++finetune));
        }
    }

    noteindex[sample]=note-4*OCTAVE;
    return finetune;
}

/*========== Order stuff */

/* handles S3M and IT orders */
void S3MIT_CreateOrders(BOOL curious)
{
	int t;

	of.numpos = 0;
	memset(of.positions,0,poslookupcnt*sizeof(UWORD));
	memset(poslookup,-1,256);
	for(t=0;t<poslookupcnt;t++) {
		int order=origpositions[t];
		if(order==255) order=LAST_PATTERN;
		of.positions[of.numpos]=order;
		poslookup[t]=(UBYTE)of.numpos; /* bug fix for freaky S3Ms / ITs */
		if(origpositions[t]<254) of.numpos++;
		else
			/* end of song special order */
			if((order==LAST_PATTERN)&&(!(curious--))) break;
	}
}

/*========== Effect stuff */

/* handles S3M and IT effects */
void S3MIT_ProcessCmd(UBYTE cmd,UBYTE inf,unsigned int flags)
{
	UBYTE hi,lo;

	lo=inf&0xf;
	hi=inf>>4;

	/* process S3M / IT specific command structure */

	if(cmd!=255) {
		switch(cmd) {
			case 1: /* Axx set speed to xx */
				UniEffect(UNI_S3MEFFECTA,inf);
				break;
			case 2: /* Bxx position jump */
				if (inf<poslookupcnt) {
					/* switch to curious mode if necessary, for example
					   sympex.it, deep joy.it */
					if(((SBYTE)poslookup[inf]<0)&&(origpositions[inf]!=255))
						S3MIT_CreateOrders(1);

					if(!((SBYTE)poslookup[inf]<0))
						UniPTEffect(0xb,poslookup[inf]);
				}
				break;
			case 3: /* Cxx patternbreak to row xx */
				if ((flags & S3MIT_OLDSTYLE) && !(flags & S3MIT_IT))
					UniPTEffect(0xd,(inf>>4)*10+(inf&0xf));
				else
					UniPTEffect(0xd,inf);
				break;
			case 4: /* Dxy volumeslide */
				UniEffect(UNI_S3MEFFECTD,inf);
				break;
			case 5: /* Exy toneslide down */
				UniEffect(UNI_S3MEFFECTE,inf);
				break;
			case 6: /* Fxy toneslide up */
				UniEffect(UNI_S3MEFFECTF,inf);
				break;
			case 7: /* Gxx Tone portamento, speed xx */
				if (flags & S3MIT_OLDSTYLE)
					UniPTEffect(0x3,inf);
				else
					UniEffect(UNI_ITEFFECTG,inf);
				break;
			case 8: /* Hxy vibrato */
				if (flags & S3MIT_OLDSTYLE)
					UniPTEffect(0x4,inf);
				else
					UniEffect(UNI_ITEFFECTH,inf);
				break;
			case 9: /* Ixy tremor, ontime x, offtime y */
				if (flags & S3MIT_OLDSTYLE)
					UniEffect(UNI_S3MEFFECTI,inf);
				else
					UniEffect(UNI_ITEFFECTI,inf);
				break;
			case 0xa: /* Jxy arpeggio */
				UniPTEffect(0x0,inf);
				break;
			case 0xb: /* Kxy Dual command H00 & Dxy */
				if (flags & S3MIT_OLDSTYLE)
					UniPTEffect(0x4,0);
				else
					UniEffect(UNI_ITEFFECTH,0);
				UniEffect(UNI_S3MEFFECTD,inf);
				break;
			case 0xc: /* Lxy Dual command G00 & Dxy */
				if (flags & S3MIT_OLDSTYLE)
					UniPTEffect(0x3,0);
				else
					UniEffect(UNI_ITEFFECTG,0);
				UniEffect(UNI_S3MEFFECTD,inf);
				break;
			case 0xd: /* Mxx Set Channel Volume */
				UniEffect(UNI_ITEFFECTM,inf);
				break;
			case 0xe: /* Nxy Slide Channel Volume */
				UniEffect(UNI_ITEFFECTN,inf);
				break;
			case 0xf: /* Oxx set sampleoffset xx00h */
				UniPTEffect(0x9,inf);
				break;
			case 0x10: /* Pxy Slide Panning Commands */
				UniEffect(UNI_ITEFFECTP,inf);
				break;
			case 0x11: /* Qxy Retrig (+volumeslide) */
				UniWriteByte(UNI_S3MEFFECTQ);
				if(inf && !lo && !(flags & S3MIT_OLDSTYLE))
					UniWriteByte(1);
				else
					UniWriteByte(inf);
				break;
			case 0x12: /* Rxy tremolo speed x, depth y */
				UniEffect(UNI_S3MEFFECTR,inf);
				break;
			case 0x13: /* Sxx special commands */
				if (inf>=0xf0) {
					/* change resonant filter settings if necessary */
					if((filters)&&((inf&0xf)!=activemacro)) {
						activemacro=inf&0xf;
						for(inf=0;inf<0x80;inf++)
							filtersettings[inf].filter=filtermacros[activemacro];
					}
				} else {
					/* Scream Tracker does not have samples larger than
					   64 Kb, thus doesn't need the SAx effect */
					if ((flags & S3MIT_SCREAM) && ((inf & 0xf0) == 0xa0))
						break;

					UniEffect(UNI_ITEFFECTS0,inf);
				}
				break;
			case 0x14: /* Txx tempo */
				if(inf>=0x20)
					UniEffect(UNI_S3MEFFECTT,inf);
				else {
					if(!(flags & S3MIT_OLDSTYLE))
						/* IT Tempo slide */
						UniEffect(UNI_ITEFFECTT,inf);
				}
				break;
			case 0x15: /* Uxy Fine Vibrato speed x, depth y */
				if(flags & S3MIT_OLDSTYLE)
					UniEffect(UNI_S3MEFFECTU,inf);
				else
					UniEffect(UNI_ITEFFECTU,inf);
				break;
			case 0x16: /* Vxx Set Global Volume */
				UniEffect(UNI_XMEFFECTG,inf);
				break;
			case 0x17: /* Wxy Global Volume Slide */
				UniEffect(UNI_ITEFFECTW,inf);
				break;
			case 0x18: /* Xxx amiga command 8xx */
				if(flags & S3MIT_OLDSTYLE) {
					if(inf>128)
						UniEffect(UNI_ITEFFECTS0,0x91); /* surround */
					else
						UniPTEffect(0x8,(inf==128)?255:(inf<<1));
				} else
					UniPTEffect(0x8,inf);
				break;
			case 0x19: /* Yxy Panbrello  speed x, depth y */
				UniEffect(UNI_ITEFFECTY,inf);
				break;
			case 0x1a: /* Zxx midi/resonant filters */
				if(filtersettings[inf].filter) {
					UniWriteByte(UNI_ITEFFECTZ);
					UniWriteByte(filtersettings[inf].filter);
					UniWriteByte(filtersettings[inf].inf);
				}
				break;
		}
	}
}

/*========== Unitrk stuff */

/* Generic effect writing routine */
void UniEffect(UWORD eff,UWORD dat)
{
	if((!eff)||(eff>=UNI_LAST)) return;

	UniWriteByte( (UBYTE)eff);
	if(unioperands[eff]==2)
		UniWriteWord(dat);
	else
		UniWriteByte( (UBYTE)dat);
}

/*  Appends UNI_PTEFFECTX opcode to the unitrk stream. */
void UniPTEffect(UBYTE eff, UBYTE dat)
{
#ifdef MIKMOD_DEBUG
	if (eff>=0x10)
		fprintf(stderr,"UniPTEffect called with incorrect eff value %d\n",eff);
	else
#endif
	if((eff)||(dat)||(of.flags&UF_ARPMEM)) UniEffect(UNI_PTEFFECT0+eff,dat);
}

/* Appends UNI_VOLEFFECT + effect/dat to unistream. */
void UniVolEffect(UWORD eff,UBYTE dat)
{
	if((eff)||(dat)) { /* don't write empty effect */
		UniWriteByte(UNI_VOLEFFECTS);
		UniWriteByte( (UBYTE)eff);
    UniWriteByte(dat);
	}
}


void MikMod_RegisterAllLoaders_internal(void)
{
	_mm_registerloader(&load_669);
	_mm_registerloader(&load_amf);
	_mm_registerloader(&load_asy);
	_mm_registerloader(&load_dsm);
	_mm_registerloader(&load_far);
	_mm_registerloader(&load_gdm);
	_mm_registerloader(&load_gt2);
	_mm_registerloader(&load_it);
	_mm_registerloader(&load_imf);
	_mm_registerloader(&load_mod);
	_mm_registerloader(&load_med);
	_mm_registerloader(&load_mtm);
	_mm_registerloader(&load_okt);
	_mm_registerloader(&load_s3m);
	_mm_registerloader(&load_stm);
	_mm_registerloader(&load_stx);
	_mm_registerloader(&load_ult);
	_mm_registerloader(&load_uni);
	_mm_registerloader(&load_xm);

	_mm_registerloader(&load_m15);
}

void MikMod_RegisterAllLoaders(void)
{
	MUTEX_LOCK(lists);
	MikMod_RegisterAllLoaders_internal();
	MUTEX_UNLOCK(lists);
}


		MREADER *modreader;
		MODULE of;

static	MLOADER *firstloader=NULL;

UWORD finetune[16]={
	8363,8413,8463,8529,8581,8651,8723,8757,
	7895,7941,7985,8046,8107,8169,8232,8280
};

CHAR* MikMod_InfoLoader(void)
{
	int len=0;
	MLOADER *l;
	CHAR *list=NULL;

	MUTEX_LOCK(lists);
	/* compute size of buffer */
	for(l=firstloader;l;l=l->next) len+=1+(l->next?1:0)+(int)strlen(l->version);

	if(len)
		if((list=(CHAR*)MikMod_malloc(len*sizeof(CHAR)))) {
			list[0]=0;
			/* list all registered module loders */
			for(l=firstloader;l;l=l->next)
				sprintf(list,(l->next)?"%s%s\n":"%s%s",list,l->version);
		}
	MUTEX_UNLOCK(lists);
	return list;
}

void _mm_registerloader(MLOADER* ldr)
{
	MLOADER *cruise=firstloader;

	if(cruise)
  {
    if ( strcmp( cruise->type, ldr->type ) == 0 )
    {
      return;
    }
		while(cruise->next)
    {
      cruise = cruise->next;
      if ( strcmp( cruise->type, ldr->type ) == 0 )
      {
        return;
      }
    }
		cruise->next=ldr;
	} else
		firstloader=ldr;
}

void MikMod_RegisterLoader(struct MLOADER* ldr)
{
	/* if we try to register an invalid loader, or an already registered loader,
	   ignore this attempt */
	if ((!ldr)||(ldr->next))
		return;

	MUTEX_LOCK(lists);
	_mm_registerloader(ldr);
	MUTEX_UNLOCK(lists);
}

BOOL ReadComment(UWORD len)
{
	if(len) {
		int i;

		if(!(of.comment=(CHAR*)MikMod_malloc(len+1))) return 0;
		_mm_read_UBYTES(of.comment,len,modreader);

		/* translate IT linefeeds */
		for(i=0;i<len;i++)
			if(of.comment[i]=='\r') of.comment[i]='\n';

		of.comment[len]=0;	/* just in case */
	}
	if(!of.comment[0]) {
		MikMod_free(of.comment);
		of.comment=NULL;
	}
	return 1;
}

BOOL ReadLinedComment(UWORD len,UWORD linelen)
{
	CHAR *tempcomment,*line,*storage;
	UWORD total=0,t,lines;
	int i;

	lines = (len + linelen - 1) / linelen;
	if (len) {
		if(!(tempcomment=(CHAR*)MikMod_malloc(len+1))) return 0;
		if(!(storage=(CHAR*)MikMod_malloc(linelen+1))) {
			MikMod_free(tempcomment);
			return 0;
		}
		memset(tempcomment, ' ', len);
		_mm_read_UBYTES(tempcomment,len,modreader);

		/* compute message length */
		for(line=tempcomment,total=t=0;t<lines;t++,line+=linelen) {
			for(i=linelen;(i>=0)&&(line[i]==' ');i--) line[i]=0;
			for(i=0;i<linelen;i++) if (!line[i]) break;
			total+=1+i;
		}

		if(total>lines) {
			if(!(of.comment=(CHAR*)MikMod_malloc(total+1))) {
				MikMod_free(storage);
				MikMod_free(tempcomment);
				return 0;
			}

			/* convert message */
			for(line=tempcomment,t=0;t<lines;t++,line+=linelen) {
				for(i=0;i<linelen;i++) if(!(storage[i]=line[i])) break;
				storage[i]=0; /* if (i==linelen) */
				strcat(of.comment,storage);strcat(of.comment,"\r");
			}
			MikMod_free(storage);
			MikMod_free(tempcomment);
		}
	}
	return 1;
}

BOOL AllocPositions(int total)
{
	if(!total) {
		_mm_errno=MMERR_NOT_A_MODULE;
		return 0;
	}
	if(!(of.positions=(UWORD*)MikMod_calloc(total,sizeof(UWORD)))) return 0;
	return 1;
}

BOOL AllocPatterns(void)
{
	int s,t,tracks = 0;

	if((!of.numpat)||(!of.numchn)) {
		_mm_errno=MMERR_NOT_A_MODULE;
		return 0;
	}
	/* Allocate track sequencing array */
	if(!(of.patterns=(UWORD*)MikMod_calloc((ULONG)(of.numpat+1)*of.numchn,sizeof(UWORD)))) return 0;
	if(!(of.pattrows=(UWORD*)MikMod_calloc(of.numpat+1,sizeof(UWORD)))) return 0;

	for(t=0;t<=of.numpat;t++) {
		of.pattrows[t]=64;
		for(s=0;s<of.numchn;s++)
		of.patterns[(t*of.numchn)+s]=tracks++;
	}

	return 1;
}

BOOL AllocTracks(void)
{
	if(!of.numtrk) {
		_mm_errno=MMERR_NOT_A_MODULE;
		return 0;
	}
	if(!(of.tracks=(UBYTE **)MikMod_calloc(of.numtrk,sizeof(UBYTE *)))) return 0;
	return 1;
}

BOOL AllocInstruments(void)
{
	int t,n;

	if(!of.numins) {
		_mm_errno=MMERR_NOT_A_MODULE;
		return 0;
	}
	if(!(of.instruments=(INSTRUMENT*)MikMod_calloc(of.numins,sizeof(INSTRUMENT))))
		return 0;

	for(t=0;t<of.numins;t++) {
		for(n=0;n<INSTNOTES;n++) {
			/* Init note / sample lookup table */
			of.instruments[t].samplenote[n]   = n;
			of.instruments[t].samplenumber[n] = t;
		}
		of.instruments[t].globvol = 64;
	}
	return 1;
}

BOOL AllocSamples(void)
{
	UWORD u;

	if(!of.numsmp) {
		_mm_errno=MMERR_NOT_A_MODULE;
		return 0;
	}
	if(!(of.samples=(SAMPLE*)MikMod_calloc(of.numsmp,sizeof(SAMPLE)))) return 0;

	for(u=0;u<of.numsmp;u++) {
		of.samples[u].panning = 128; /* center */
		of.samples[u].handle  = -1;
		of.samples[u].globvol = 64;
		of.samples[u].volume  = 64;
	}
	return 1;
}

static BOOL ML_LoadSamples(void)
{
	SAMPLE *s;
	int u;

	for(u=of.numsmp,s=of.samples;u;u--,s++)
		if(s->length) SL_RegisterSample(s,MD_MUSIC,modreader);

	return 1;
}

/* Creates a CSTR out of a character buffer of 'len' bytes, but strips any
   terminating non-printing characters like 0, spaces etc.                    */
CHAR *DupStr(CHAR* s,UWORD len,BOOL strict)
{
	UWORD t;
	CHAR *d=NULL;

	/* Scan for last printing char in buffer [includes high ascii up to 254] */
	while(len) {
		if(s[len-1]>0x20) break;
		len--;
	}

	/* Scan forward for possible NULL character */
	if(strict) {
		for(t=0;t<len;t++) if (!s[t]) break;
		if (t<len) len=t;
	}

	/* When the buffer wasn't completely empty, allocate a cstring and copy the
	   buffer into that string, except for any control-chars */
	if((d=(CHAR*)MikMod_malloc(sizeof(CHAR)*(len+1))))
  {
		for(t=0;t<len;t++) d[t]=(s[t]<32)?'.':s[t];
		d[len]=0;
	}
	return d;
}

static void ML_XFreeSample(SAMPLE *s)
{
	if(s->handle>=0)
		MD_SampleUnload(s->handle);
	if ( s->samplename )
  {
    //dh::Log( "Freeing MOD Sample name %x", s->samplename );
    MikMod_free(s->samplename);
  }
}

static void ML_XFreeInstrument(INSTRUMENT *i)
{
	if(i->insname) MikMod_free(i->insname);
}

static void ML_FreeEx(MODULE *mf)
{
	UWORD t;

	if(mf->songname) MikMod_free(mf->songname);
	if(mf->comment)  MikMod_free(mf->comment);

	//if(mf->modtype)   MikMod_free(mf->modtype);
  if(mf->modtype)   free(mf->modtype);
	if(mf->positions) MikMod_free(mf->positions);
	if(mf->patterns)  MikMod_free(mf->patterns);
	if(mf->pattrows)  MikMod_free(mf->pattrows);

	if(mf->tracks) {
		for(t=0;t<mf->numtrk;t++)
			if(mf->tracks[t]) MikMod_free(mf->tracks[t]);
		MikMod_free(mf->tracks);
	}
	if(mf->instruments) {
		for(t=0;t<mf->numins;t++)
			ML_XFreeInstrument(&mf->instruments[t]);
		MikMod_free(mf->instruments);
	}
	if(mf->samples) {
		for(t=0;t<mf->numsmp;t++)
    {
			//if(mf->samples[t].length)
      {
        ML_XFreeSample(&mf->samples[t]);
      }
    }
		MikMod_free(mf->samples);
	}
	memset(mf,0,sizeof(MODULE));
	if(mf!=&of) MikMod_free(mf);
}

static MODULE *ML_AllocUniMod(void)
{
	MODULE *mf;

	return (mf=(MODULE*)MikMod_malloc(sizeof(MODULE)));
}

void Player_Free_internal(MODULE *mf)
{
	if(mf) {
		Player_Exit_internal(mf);
		ML_FreeEx(mf);
	}
}

void Player_Free(MODULE *mf)
{
	MUTEX_LOCK(vars);
	Player_Free_internal(mf);
	MUTEX_UNLOCK(vars);
}

static CHAR* Player_LoadTitle_internal(MREADER *reader)
{
	MLOADER *l;

	modreader=reader;
	_mm_errno = 0;
	_mm_critical = 0;
	_mm_iobase_setcur(modreader);

	/* Try to find a loader that recognizes the module */
	for(l=firstloader;l;l=l->next) {
		_mm_rewind(modreader);
		if(l->Test()) break;
	}

	if(!l) {
		_mm_errno = MMERR_NOT_A_MODULE;
		if(_mm_errorhandler) _mm_errorhandler();
		return NULL;
	}

	return l->LoadTitle();
}

CHAR* Player_LoadTitleFP(FILE *fp)
{
	CHAR* result=NULL;
	MREADER* reader;

	if(fp && (reader=_mm_new_file_reader(fp))) {
		MUTEX_LOCK(lists);
		result=Player_LoadTitle_internal(reader);
		MUTEX_UNLOCK(lists);
		_mm_delete_file_reader(reader);
	}
	return result;
}

CHAR* Player_LoadTitleMem(const char *buffer,int len)
{
	CHAR *result=NULL;
	MREADER* reader;

	if ((reader=_mm_new_mem_reader(buffer,len)))
	{
		MUTEX_LOCK(lists);
		result=Player_LoadTitle_internal(reader);
		MUTEX_UNLOCK(lists);
		_mm_delete_mem_reader(reader);
	}


	return result;
}

CHAR* Player_LoadTitleGeneric(MREADER *reader)
{
	CHAR *result=NULL;

	if (reader) {
		MUTEX_LOCK(lists);
		result=Player_LoadTitle_internal(reader);
		MUTEX_UNLOCK(lists);
	}
	return result;
}

CHAR* Player_LoadTitle( const CHAR* filename)
{
	CHAR* result=NULL;
	FILE* fp;
	MREADER* reader;

	if((fp=_mm_fopen(filename,"rb"))) {
		if((reader=_mm_new_file_reader(fp))) {
			MUTEX_LOCK(lists);
			result=Player_LoadTitle_internal(reader);
			MUTEX_UNLOCK(lists);
			_mm_delete_file_reader(reader);
		}
		_mm_fclose(fp);
	}
	return result;
}

/* Loads a module given an reader */
MODULE* Player_LoadGeneric_internal(MREADER *reader,int maxchan,BOOL curious)
{
	int t;
	MLOADER *l;
	BOOL ok;
	MODULE *mf;

	modreader = reader;
	_mm_errno = 0;
	_mm_critical = 0;
	_mm_iobase_setcur(modreader);

	/* Try to find a loader that recognizes the module */
	for(l=firstloader;l;l=l->next) {
		_mm_rewind(modreader);
		if(l->Test()) break;
	}

	if(!l) {
		_mm_errno = MMERR_NOT_A_MODULE;
		if(_mm_errorhandler) _mm_errorhandler();
		_mm_rewind(modreader);_mm_iobase_revert(modreader);
		return NULL;
	}

	/* init unitrk routines */
	if(!UniInit()) {
		if(_mm_errorhandler) _mm_errorhandler();
		_mm_rewind(modreader);_mm_iobase_revert(modreader);
		return NULL;
	}

	/* init the module structure with vanilla settings */
	memset(&of,0,sizeof(MODULE));
	of.bpmlimit = 33;
	of.initvolume = 128;
	for (t = 0; t < UF_MAXCHAN; t++) of.chanvol[t] = 64;
	for (t = 0; t < UF_MAXCHAN; t++)
		of.panning[t] = ((t + 1) & 2) ? PAN_RIGHT : PAN_LEFT;

	/* init module loader and load the header / patterns */
	if (!l->Init || l->Init()) {
		_mm_rewind(modreader);
		ok = l->Load(curious);
		if (ok) {
			/* propagate inflags=flags for in-module samples */
			for (t = 0; t < of.numsmp; t++)
				if (of.samples[t].inflags == 0)
					of.samples[t].inflags = of.samples[t].flags;
		}
	} else
		ok = 0;

	/* free loader and unitrk allocations */
	if (l->Cleanup) l->Cleanup();
	UniCleanup();

	if(!ok) {
		ML_FreeEx(&of);
		if(_mm_errorhandler) _mm_errorhandler();
		_mm_rewind(modreader);_mm_iobase_revert(modreader);
		return NULL;
	}

	if(!ML_LoadSamples()) {
		ML_FreeEx(&of);
		if(_mm_errorhandler) _mm_errorhandler();
		_mm_rewind(modreader);_mm_iobase_revert(modreader);
		return NULL;
	}

	if(!(mf=ML_AllocUniMod())) {
		ML_FreeEx(&of);
		_mm_rewind(modreader);_mm_iobase_revert(modreader);
		if(_mm_errorhandler) _mm_errorhandler();
		return NULL;
	}

	/* If the module doesn't have any specific panning, create a
	   MOD-like panning, with the channels half-separated. */
	if (!(of.flags & UF_PANNING))
		for (t = 0; t < of.numchn; t++)
			of.panning[t] = ((t + 1) & 2) ? PAN_HALFRIGHT : PAN_HALFLEFT;

	/* Copy the static MODULE contents into the dynamic MODULE struct. */
	memcpy(mf,&of,sizeof(MODULE));

	if(maxchan>0) {
		if(!(mf->flags&UF_NNA)&&(mf->numchn<maxchan))
			maxchan = mf->numchn;
		else
		  if((mf->numvoices)&&(mf->numvoices<maxchan))
			maxchan = mf->numvoices;

		if(maxchan<mf->numchn) mf->flags |= UF_NNA;

		if(MikMod_SetNumVoices_internal(maxchan,-1)) {
			_mm_iobase_revert(modreader);
			Player_Free(mf);
			return NULL;
		}
	}
	if(SL_LoadSamples()) {
		_mm_iobase_revert(modreader);
		Player_Free_internal(mf);
		return NULL;
	}
	if(Player_Init(mf)) {
		_mm_iobase_revert(modreader);
		Player_Free_internal(mf);
		mf=NULL;
	}
	_mm_iobase_revert(modreader);
	return mf;
}

MODULE* Player_LoadGeneric(MREADER *reader,int maxchan,BOOL curious)
{
	MODULE* result;

	MUTEX_LOCK(vars);
	MUTEX_LOCK(lists);
		result=Player_LoadGeneric_internal(reader,maxchan,curious);
	MUTEX_UNLOCK(lists);
	MUTEX_UNLOCK(vars);

	return result;
}

MODULE* Player_LoadMem(const char *buffer,int len,int maxchan,BOOL curious)
{
	MODULE* result=NULL;
	MREADER* reader;

	if ((reader=_mm_new_mem_reader(buffer, len))) {
		result=Player_LoadGeneric(reader,maxchan,curious);
		_mm_delete_mem_reader(reader);
	}
	return result;
}

/* Loads a module given a file pointer.
   File is loaded from the current file seek position. */
MODULE* Player_LoadFP(FILE* fp,int maxchan,BOOL curious)
{
	MODULE* result=NULL;
	struct MREADER* reader=_mm_new_file_reader (fp);

	if (reader) {
		result=Player_LoadGeneric(reader,maxchan,curious);
		_mm_delete_file_reader(reader);
	}
	return result;
}

/* Open a module via its filename.  The loader will initialize the specified
   song-player 'player'. */
MODULE* Player_Load( const CHAR* filename,int maxchan,BOOL curious)
{
	FILE *fp;
	MODULE *mf=NULL;

	if((fp=_mm_fopen(filename,"rb"))) {
		mf=Player_LoadFP(fp,maxchan,curious);
		_mm_fclose(fp);
	}
	return mf;
}


static unsigned char ulaw_comp_table[16384] = {
	0xff, 0xfe, 0xfe, 0xfd, 0xfd, 0xfc, 0xfc, 0xfb,
	0xfb, 0xfa, 0xfa, 0xf9, 0xf9, 0xf8, 0xf8, 0xf7,
	0xf7, 0xf6, 0xf6, 0xf5, 0xf5, 0xf4, 0xf4, 0xf3,
	0xf3, 0xf2, 0xf2, 0xf1, 0xf1, 0xf0, 0xf0, 0xef,
	0xef, 0xef, 0xef, 0xee, 0xee, 0xee, 0xee, 0xed,
	0xed, 0xed, 0xed, 0xec, 0xec, 0xec, 0xec, 0xeb,
	0xeb, 0xeb, 0xeb, 0xea, 0xea, 0xea, 0xea, 0xe9,
	0xe9, 0xe9, 0xe9, 0xe8, 0xe8, 0xe8, 0xe8, 0xe7,
	0xe7, 0xe7, 0xe7, 0xe6, 0xe6, 0xe6, 0xe6, 0xe5,
	0xe5, 0xe5, 0xe5, 0xe4, 0xe4, 0xe4, 0xe4, 0xe3,
	0xe3, 0xe3, 0xe3, 0xe2, 0xe2, 0xe2, 0xe2, 0xe1,
	0xe1, 0xe1, 0xe1, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf,
	0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xde,
	0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xdd,
	0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdc,
	0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdb,
	0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xda,
	0xda, 0xda, 0xda, 0xda, 0xda, 0xda, 0xda, 0xd9,
	0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xd8,
	0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd7,
	0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xd6,
	0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd5,
	0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd4,
	0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd3,
	0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd2,
	0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd1,
	0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1, 0xd0,
	0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xcf,
	0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf,
	0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xce,
	0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce,
	0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd,
	0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcc,
	0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
	0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcb,
	0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb,
	0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xca,
	0xca, 0xca, 0xca, 0xca, 0xca, 0xca, 0xca, 0xca,
	0xca, 0xca, 0xca, 0xca, 0xca, 0xca, 0xca, 0xc9,
	0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9,
	0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc8,
	0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8,
	0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc7,
	0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7,
	0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7, 0xc6,
	0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6,
	0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc5,
	0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5,
	0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc4,
	0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4,
	0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc3,
	0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
	0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc2,
	0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2,
	0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc1,
	0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1,
	0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc0,
	0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
	0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbd,
	0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd,
	0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd,
	0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd,
	0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbc,
	0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc,
	0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc,
	0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc,
	0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbb,
	0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
	0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
	0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
	0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xba,
	0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba,
	0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba,
	0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba,
	0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xba, 0xb9,
	0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9,
	0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9,
	0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9,
	0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb8,
	0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8,
	0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8,
	0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8,
	0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb7,
	0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7,
	0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7,
	0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7,
	0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb7, 0xb6,
	0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6,
	0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6,
	0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6,
	0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb5,
	0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5,
	0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5,
	0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5,
	0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb4,
	0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
	0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
	0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
	0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb3,
	0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,
	0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,
	0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3,
	0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb2,
	0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2,
	0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2,
	0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2,
	0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb1,
	0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1,
	0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1,
	0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1,
	0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb0,
	0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
	0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
	0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0,
	0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae,
	0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xae, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad,
	0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
	0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9,
	0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa9, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
	0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7,
	0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6,
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
	0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
	0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
	0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
	0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e,
	0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d,
	0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c,
	0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b,
	0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a,
	0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
	0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
	0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
	0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96,
	0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x96, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95,
	0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x95, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94,
	0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x94, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93,
	0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x93, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92,
	0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91,
	0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x91, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e,
	0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8e, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d,
	0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c,
	0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8c, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b,
	0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8b, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a,
	0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89,
	0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x89, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87,
	0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86,
	0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84,
	0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82,
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
	0x16, 0x16, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
	0x17, 0x17, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19,
	0x19, 0x19, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
	0x1a, 0x1a, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
	0x1b, 0x1b, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
	0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
	0x1d, 0x1d, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
	0x1e, 0x1e, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1f, 0x1f, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21,
	0x21, 0x21, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
	0x23, 0x23, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
	0x24, 0x24, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26,
	0x26, 0x26, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	0x29, 0x29, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a,
	0x2a, 0x2a, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
	0x2b, 0x2b, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
	0x2e, 0x2e, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32,
	0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
	0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
	0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
	0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
	0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
	0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
	0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
	0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
	0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
	0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
	0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x39, 0x39, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
	0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
	0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
	0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
	0x3b, 0x3b, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
	0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
	0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
	0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
	0x3d, 0x3d, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e,
	0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e, 0x3e,
	0x3e, 0x3e, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
	0x3f, 0x3f, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43,
	0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43,
	0x43, 0x43, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
	0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
	0x44, 0x44, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45,
	0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45,
	0x45, 0x45, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46,
	0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46,
	0x46, 0x46, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
	0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
	0x47, 0x47, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
	0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
	0x48, 0x48, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49,
	0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49,
	0x49, 0x49, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a,
	0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a,
	0x4a, 0x4a, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b,
	0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b,
	0x4b, 0x4b, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c,
	0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c,
	0x4c, 0x4c, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d,
	0x4d, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d,
	0x4d, 0x4d, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e,
	0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e,
	0x4e, 0x4e, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f,
	0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f,
	0x4f, 0x4f, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
	0x50, 0x50, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51,
	0x51, 0x51, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52,
	0x52, 0x52, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
	0x53, 0x53, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
	0x54, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56,
	0x56, 0x56, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57,
	0x57, 0x57, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
	0x58, 0x58, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
	0x59, 0x59, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
	0x5a, 0x5a, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b, 0x5b,
	0x5b, 0x5b, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5d, 0x5d, 0x5d, 0x5d, 0x5d, 0x5d,
	0x5d, 0x5d, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
	0x5e, 0x5e, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x5f, 0x5f, 0x60, 0x60, 0x60, 0x60, 0x61, 0x61,
	0x61, 0x61, 0x62, 0x62, 0x62, 0x62, 0x63, 0x63,
	0x63, 0x63, 0x64, 0x64, 0x64, 0x64, 0x65, 0x65,
	0x65, 0x65, 0x66, 0x66, 0x66, 0x66, 0x67, 0x67,
	0x67, 0x67, 0x68, 0x68, 0x68, 0x68, 0x69, 0x69,
	0x69, 0x69, 0x6a, 0x6a, 0x6a, 0x6a, 0x6b, 0x6b,
	0x6b, 0x6b, 0x6c, 0x6c, 0x6c, 0x6c, 0x6d, 0x6d,
	0x6d, 0x6d, 0x6e, 0x6e, 0x6e, 0x6e, 0x6f, 0x6f,
	0x6f, 0x6f, 0x70, 0x70, 0x71, 0x71, 0x72, 0x72,
	0x73, 0x73, 0x74, 0x74, 0x75, 0x75, 0x76, 0x76,
	0x77, 0x77, 0x78, 0x78, 0x79, 0x79, 0x7a, 0x7a,
	0x7b, 0x7b, 0x7c, 0x7c, 0x7d, 0x7d, 0x7e, 0x7e
};

#define st_linear_to_ulaw(x) ulaw_comp_table[(x / 4) & 0x3fff]

/* convert unsigned linear data from Mixer_WriteBytes() to ulaw */
void unsignedtoulaw(char *buf, int nsamp)
{
	while (nsamp--) {
		register long datum = (long)*((unsigned char *)buf);

		datum ^= 128;			/* convert to signed */
		datum <<= 8;
		/* round up to 12 bits of data */
		datum += 0x8;			/* + 0b1000 */
		datum = st_linear_to_ulaw(datum);
		*buf++ = (char)datum;
	}
}


static	MDRIVER *firstdriver=NULL;
MDRIVER *md_driver=NULL;
extern	MODULE *pf; /* modfile being played */

/* Initial global settings */
UWORD md_device         = 0;	/* autodetect */
UWORD md_mixfreq        = 44100;
UWORD md_mode           = DMODE_STEREO | DMODE_16BITS |
				  DMODE_SURROUND |DMODE_SOFT_MUSIC |
				  DMODE_SOFT_SNDFX;
UBYTE md_pansep         = 128;	/* 128 == 100% (full left/right) */
UBYTE md_reverb         = 0;	/* no reverb */
UBYTE md_volume         = 128;	/* global sound volume (0-128) */
UBYTE md_musicvolume    = 128;	/* volume of song */
UBYTE md_sndfxvolume    = 128;	/* volume of sound effects */
			UWORD md_bpm            = 125;	/* tempo */

/* Do not modify the numchn variables yourself!  use MD_SetVoices() */
			UBYTE md_numchn=0,md_sngchn=0,md_sfxchn=0;
			UBYTE md_hardchn=0,md_softchn=0;

			void (*md_player)(void) = Player_HandleTick;
static		BOOL  isplaying=0, initialized = 0;
static		UBYTE *sfxinfo;
static		int sfxpool;

static		SAMPLE **md_sample = NULL;

/* Previous driver in use */
static		SWORD olddevice = -1;

/* Limits the number of hardware voices to the specified amount.
   This function should only be used by the low-level drivers. */
static	void LimitHardVoices(int limit)
{
	int t=0;

	if (!(md_mode & DMODE_SOFT_SNDFX) && (md_sfxchn>limit)) md_sfxchn=limit;
	if (!(md_mode & DMODE_SOFT_MUSIC) && (md_sngchn>limit)) md_sngchn=limit;

	if (!(md_mode & DMODE_SOFT_SNDFX))
		md_hardchn=md_sfxchn;
	else
		md_hardchn=0;

	if (!(md_mode & DMODE_SOFT_MUSIC)) md_hardchn += md_sngchn;

	while (md_hardchn>limit) {
		if (++t & 1) {
			if (!(md_mode & DMODE_SOFT_SNDFX) && (md_sfxchn>4)) md_sfxchn--;
		} else {
			if (!(md_mode & DMODE_SOFT_MUSIC) && (md_sngchn>8)) md_sngchn--;
		}

		if (!(md_mode & DMODE_SOFT_SNDFX))
			md_hardchn=md_sfxchn;
		else
			md_hardchn=0;

		if (!(md_mode & DMODE_SOFT_MUSIC))
			md_hardchn+=md_sngchn;
	}
	md_numchn=md_hardchn+md_softchn;
}

/* Limits the number of hardware voices to the specified amount.
   This function should only be used by the low-level drivers. */
static	void LimitSoftVoices(int limit)
{
	int t=0;

	if ((md_mode & DMODE_SOFT_SNDFX) && (md_sfxchn>limit)) md_sfxchn=limit;
	if ((md_mode & DMODE_SOFT_MUSIC) && (md_sngchn>limit)) md_sngchn=limit;

	if (md_mode & DMODE_SOFT_SNDFX)
		md_softchn=md_sfxchn;
	else
		md_softchn=0;

	if (md_mode & DMODE_SOFT_MUSIC) md_softchn+=md_sngchn;

	while (md_softchn>limit) {
		if (++t & 1) {
			if ((md_mode & DMODE_SOFT_SNDFX) && (md_sfxchn>4)) md_sfxchn--;
		} else {
			if ((md_mode & DMODE_SOFT_MUSIC) && (md_sngchn>8)) md_sngchn--;
		}

		if (!(md_mode & DMODE_SOFT_SNDFX))
			md_softchn=md_sfxchn;
		else
			md_softchn=0;

		if (!(md_mode & DMODE_SOFT_MUSIC))
			md_softchn+=md_sngchn;
	}
	md_numchn=md_hardchn+md_softchn;
}

/* Note: 'type' indicates whether the returned value should be for music or for
   sound effects. */
ULONG MD_SampleSpace(int type)
{
	if(type==MD_MUSIC)
		type=(md_mode & DMODE_SOFT_MUSIC)?MD_SOFTWARE:MD_HARDWARE;
	else if(type==MD_SNDFX)
		type=(md_mode & DMODE_SOFT_SNDFX)?MD_SOFTWARE:MD_HARDWARE;

	return md_driver->FreeSampleSpace(type);
}

ULONG MD_SampleLength(int type,SAMPLE* s)
{
	if(type==MD_MUSIC)
		type=(md_mode & DMODE_SOFT_MUSIC)?MD_SOFTWARE:MD_HARDWARE;
	else
	  if(type==MD_SNDFX)
		type=(md_mode & DMODE_SOFT_SNDFX)?MD_SOFTWARE:MD_HARDWARE;

	return md_driver->RealSampleLength(type,s);
}

CHAR* MikMod_InfoDriver(void)
{
	int t;
	size_t len=0;
	MDRIVER *l;
	CHAR *list=NULL;

	MUTEX_LOCK(lists);
	/* compute size of buffer */
	for(l=firstdriver;l;l=l->next)
		len+=4+(l->next?1:0)+strlen(l->Version);

	if(len)
		if((list=(CHAR*)MikMod_malloc(len*sizeof(CHAR)))) {
			list[0]=0;
			/* list all registered device drivers : */
			for(t=1,l=firstdriver;l;l=l->next,t++)
				sprintf(list,(l->next)?"%s%2d %s\n":"%s%2d %s",
				    list,t,l->Version);
		}
	MUTEX_UNLOCK(lists);
	return list;
}

void _mm_registerdriver(struct MDRIVER* drv)
{
	MDRIVER *cruise = firstdriver;

	/* don't register a MISSING() driver */
	if ((drv->Name) && (drv->Version)) {
		if (cruise)
    {
      if ( strcmp( cruise->Name, drv->Name ) == 0 )
      {
        return;
      }
			while (cruise->next)
      {
        cruise = cruise->next;
        if ( strcmp( cruise->Name, drv->Name ) == 0 )
        {
          return;
        }
      }
			cruise->next = drv;
		} else
			firstdriver = drv;
	}
}

void MikMod_RegisterDriver(struct MDRIVER* drv)
{
	/* if we try to register an invalid driver, or an already registered driver,
	   ignore this attempt */
	if ((!drv)||(drv->next)||(!drv->Name))
		return;

	MUTEX_LOCK(lists);
	_mm_registerdriver(drv);
	MUTEX_UNLOCK(lists);
}

int MikMod_DriverFromAlias(CHAR *alias)
{
	int rank=1;
	MDRIVER *cruise;

	MUTEX_LOCK(lists);
	cruise=firstdriver;
	while(cruise) {
		if (cruise->Alias) {
			if (!(strcasecmp(alias,cruise->Alias))) break;
			rank++;
		}
		cruise=cruise->next;
	}
	if(!cruise) rank=0;
	MUTEX_UNLOCK(lists);

	return rank;
}

MDRIVER *MikMod_DriverByOrdinal(int ordinal)
{
        MDRIVER *cruise;

        /* Allow only driver ordinals > 0 */
        if (!ordinal)
                return 0;

        MUTEX_LOCK(lists);
        cruise = firstdriver;
        while (cruise && --ordinal)
                cruise = cruise->next;
        MUTEX_UNLOCK(lists);
        return cruise;
}

SWORD MD_SampleLoad(SAMPLOAD* s, int type)
{
	SWORD result;

	if(type==MD_MUSIC)
		type=(md_mode & DMODE_SOFT_MUSIC)?MD_SOFTWARE:MD_HARDWARE;
	else if(type==MD_SNDFX)
		type=(md_mode & DMODE_SOFT_SNDFX)?MD_SOFTWARE:MD_HARDWARE;

	SL_Init(s);
	result=md_driver->SampleLoad(s,type);
	SL_Exit(s);

	return result;
}

void MD_SampleUnload(SWORD handle)
{
	md_driver->SampleUnload(handle);
}

MikMod_player_t MikMod_RegisterPlayer(MikMod_player_t player)
{
	MikMod_player_t result;

	MUTEX_LOCK(vars);
	result=md_player;
	md_player=player;
	MUTEX_UNLOCK(vars);

	return result;
}

void MikMod_Update(void)
{
	MUTEX_LOCK(vars);
	if(isplaying) {
		if((!pf)||(!pf->forbid))
			md_driver->Update();
		else {
			if (md_driver->Pause)
				md_driver->Pause();
		}
	}
	MUTEX_UNLOCK(vars);
}

void Voice_SetVolume_internal(SBYTE voice,UWORD vol)
{
	ULONG  tmp;

	if((voice<0)||(voice>=md_numchn)) return;

	/* range checks */
	if(md_musicvolume>128) md_musicvolume=128;
	if(md_sndfxvolume>128) md_sndfxvolume=128;
	if(md_volume>128) md_volume=128;

	tmp=(ULONG)vol*(ULONG)md_volume*
	     ((voice<md_sngchn)?(ULONG)md_musicvolume:(ULONG)md_sndfxvolume);
	md_driver->VoiceSetVolume(voice, (UWORD)( tmp/16384UL ) );
}

void Voice_SetVolume(SBYTE voice,UWORD vol)
{
	MUTEX_LOCK(vars);
	Voice_SetVolume_internal(voice,vol);
	MUTEX_UNLOCK(vars);
}

UWORD Voice_GetVolume(SBYTE voice)
{
	UWORD result=0;

	MUTEX_LOCK(vars);
	if((voice>=0)&&(voice<md_numchn))
		result=md_driver->VoiceGetVolume(voice);
	MUTEX_UNLOCK(vars);

	return result;
}

void Voice_SetFrequency_internal(SBYTE voice,ULONG frq)
{
	if((voice<0)||(voice>=md_numchn)) return;
	if((md_sample[voice])&&(md_sample[voice]->divfactor))
		frq/=md_sample[voice]->divfactor;
	md_driver->VoiceSetFrequency(voice,frq);
}

void Voice_SetFrequency(SBYTE voice,ULONG frq)
{
	MUTEX_LOCK(vars);
	Voice_SetFrequency_internal(voice,frq);
	MUTEX_UNLOCK(vars);
}

ULONG Voice_GetFrequency(SBYTE voice)
{
	ULONG result=0;

	MUTEX_LOCK(vars);
	if((voice>=0)&&(voice<md_numchn))
		result=md_driver->VoiceGetFrequency(voice);
	MUTEX_UNLOCK(vars);

	return result;
}

void Voice_SetPanning_internal(SBYTE voice,ULONG pan)
{
	if((voice<0)||(voice>=md_numchn)) return;
	if(pan!=PAN_SURROUND) {
		if(md_pansep>128) md_pansep=128;
		if(md_mode & DMODE_REVERSE) pan=255-pan;
		pan = (((SWORD)(pan-128)*md_pansep)/128)+128;
	}
	md_driver->VoiceSetPanning(voice, pan);
}

void Voice_SetPanning(SBYTE voice,ULONG pan)
{
#ifdef MIKMOD_DEBUG
	if((pan!=PAN_SURROUND)&&((pan<0)||(pan>255)))
		fprintf(stderr,"\rVoice_SetPanning called with pan=%ld\n",(long)pan);
#endif

	MUTEX_LOCK(vars);
	Voice_SetPanning_internal(voice,pan);
	MUTEX_UNLOCK(vars);
}

ULONG Voice_GetPanning(SBYTE voice)
{
	ULONG result=PAN_CENTER;

	MUTEX_LOCK(vars);
	if((voice>=0)&&(voice<md_numchn))
		result=md_driver->VoiceGetPanning(voice);
	MUTEX_UNLOCK(vars);

	return result;
}

void Voice_Play_internal(SBYTE voice,SAMPLE* s,ULONG start)
{
	ULONG  repend;

	if((voice<0)||(voice>=md_numchn)) return;

	md_sample[voice]=s;
	repend=s->loopend;

	if(s->flags&SF_LOOP)
		/* repend can't be bigger than size */
		if(repend>s->length) repend=s->length;

	md_driver->VoicePlay(voice,s->handle,start,s->length,s->loopstart,repend,s->flags);
}

void Voice_Play(SBYTE voice,SAMPLE* s,ULONG start)
{
	if(start>s->length) return;

	MUTEX_LOCK(vars);
	Voice_Play_internal(voice,s,start);
	MUTEX_UNLOCK(vars);
}

void Voice_Stop_internal(SBYTE voice)
{
	if((voice<0)||(voice>=md_numchn)) return;
	if(voice>=md_sngchn)
		/* It is a sound effects channel, so flag the voice as non-critical! */
		sfxinfo[voice-md_sngchn]=0;
	md_driver->VoiceStop(voice);
}

void Voice_Stop(SBYTE voice)
{
	MUTEX_LOCK(vars);
	Voice_Stop_internal(voice);
	MUTEX_UNLOCK(vars);
}

BOOL Voice_Stopped_internal(SBYTE voice)
{
	if((voice<0)||(voice>=md_numchn)) return 0;
	return(md_driver->VoiceStopped(voice));
}

BOOL Voice_Stopped(SBYTE voice)
{
	BOOL result;

	MUTEX_LOCK(vars);
	result=Voice_Stopped_internal(voice);
	MUTEX_UNLOCK(vars);

	return result;
}

SLONG Voice_GetPosition(SBYTE voice)
{
	SLONG result=0;

	MUTEX_LOCK(vars);
	if((voice>=0)&&(voice<md_numchn)) {
		if (md_driver->VoiceGetPosition)
			result=(md_driver->VoiceGetPosition(voice));
		else
			result=-1;
	}
	MUTEX_UNLOCK(vars);

	return result;
}

ULONG Voice_RealVolume(SBYTE voice)
{
	ULONG result=0;

	MUTEX_LOCK(vars);
	if((voice>=0)&&(voice<md_numchn)&& md_driver->VoiceRealVolume)
		result=(md_driver->VoiceRealVolume(voice));
	MUTEX_UNLOCK(vars);

	return result;
}

extern MikMod_callback_t vc_callback;

void VC_SetCallback(MikMod_callback_t callback)
{
	vc_callback = callback;
}

static BOOL _mm_init(CHAR *cmdline)
{
	UWORD t;

	_mm_critical = 1;

	/* if md_device==0, try to find a device number */
	if(!md_device) {
		cmdline=NULL;

		for(t=1,md_driver=firstdriver;md_driver;md_driver=md_driver->next,t++)
			if(md_driver->IsPresent()) break;

		if(!md_driver) {
			_mm_errno = MMERR_DETECTING_DEVICE;
			if(_mm_errorhandler) _mm_errorhandler();
			md_driver = &drv_nos;
			return 1;
		}

		md_device = t;
	} else {
		/* if n>0, use that driver */
		for(t=1,md_driver=firstdriver;(md_driver)&&(t!=md_device);md_driver=md_driver->next)
			t++;

		if(!md_driver) {
			_mm_errno = MMERR_INVALID_DEVICE;
			if(_mm_errorhandler) _mm_errorhandler();
			md_driver = &drv_nos;
			return 1;
		}

		/* arguments here might be necessary for the presence check to succeed */
		if(cmdline&&(md_driver->CommandLine))
			md_driver->CommandLine(cmdline);

		if(!md_driver->IsPresent()) {
			_mm_errno = MMERR_DETECTING_DEVICE;
			if(_mm_errorhandler) _mm_errorhandler();
			md_driver = &drv_nos;
			return 1;
		}
	}

	olddevice = md_device;
	if(md_driver->Init()) {
		MikMod_Exit_internal();
		if(_mm_errorhandler) _mm_errorhandler();
		return 1;
	}

	initialized=1;
	_mm_critical=0;

	return 0;
}

BOOL MikMod_Init(CHAR *cmdline)
{
	BOOL result;

	MUTEX_LOCK(vars);
	MUTEX_LOCK(lists);
	result=_mm_init(cmdline);
	MUTEX_UNLOCK(lists);
	MUTEX_UNLOCK(vars);

	return result;
}

void MikMod_Exit_internal(void)
{
	MikMod_DisableOutput_internal();
  if ( md_driver )
  {
	  md_driver->Exit();
  }
	md_numchn = md_sfxchn = md_sngchn = 0;
	md_driver = &drv_nos;

	if(sfxinfo) MikMod_free(sfxinfo);
	if(md_sample) MikMod_free(md_sample);
	md_sample  = NULL;
	sfxinfo    = NULL;

	initialized = 0;
}

void MikMod_Exit(void)
{
	MUTEX_LOCK(vars);
	MUTEX_LOCK(lists);
	MikMod_Exit_internal();
	MUTEX_UNLOCK(lists);
	MUTEX_UNLOCK(vars);
}

/* Reset the driver using the new global variable settings.
   If the driver has not been initialized, it will be now. */
static BOOL _mm_reset(CHAR *cmdline)
{
	BOOL wasplaying = 0;

	if(!initialized) return _mm_init(cmdline);

	if (isplaying) {
		wasplaying = 1;
		md_driver->PlayStop();
	}

	if((!md_driver->Reset)||(md_device != olddevice)) {
		/* md_driver->Reset was NULL, or md_device was changed, so do a full
		   reset of the driver. */
		md_driver->Exit();
		if(_mm_init(cmdline)) {
			MikMod_Exit_internal();
			if(_mm_errno)
				if(_mm_errorhandler) _mm_errorhandler();
			return 1;
		}
	} else {
		if(md_driver->Reset()) {
			MikMod_Exit_internal();
			if(_mm_errno)
				if(_mm_errorhandler) _mm_errorhandler();
			return 1;
		}
	}

	if (wasplaying) md_driver->PlayStart();
	return 0;
}

BOOL MikMod_Reset(CHAR *cmdline)
{
	BOOL result;

	MUTEX_LOCK(vars);
	MUTEX_LOCK(lists);
	result=_mm_reset(cmdline);
	MUTEX_UNLOCK(lists);
	MUTEX_UNLOCK(vars);

	return result;
}

/* If either parameter is -1, the current set value will be retained. */
BOOL MikMod_SetNumVoices_internal(int music, int sfx)
{
	BOOL resume = 0;
	int t, oldchn = 0;

	if((!music)&&(!sfx)) return 1;
	_mm_critical = 1;
	if(isplaying) {
		MikMod_DisableOutput_internal();
		oldchn = md_numchn;
		resume = 1;
	}

	if(sfxinfo) MikMod_free(sfxinfo);
	if(md_sample) MikMod_free(md_sample);
	md_sample  = NULL;
	sfxinfo    = NULL;

	if(music!=-1) md_sngchn = music;
	if(sfx!=-1)   md_sfxchn = sfx;
	md_numchn = md_sngchn + md_sfxchn;

	LimitHardVoices(md_driver->HardVoiceLimit);
	LimitSoftVoices(md_driver->SoftVoiceLimit);

	if(md_driver->SetNumVoices()) {
		MikMod_Exit_internal();
		if(_mm_errno)
			if(_mm_errorhandler!=NULL) _mm_errorhandler();
		md_numchn = md_softchn = md_hardchn = md_sfxchn = md_sngchn = 0;
		return 1;
	}

	if(md_sngchn+md_sfxchn)
		md_sample=(SAMPLE**)MikMod_calloc(md_sngchn+md_sfxchn,sizeof(SAMPLE*));
	if(md_sfxchn)
		sfxinfo = (UBYTE *)MikMod_calloc(md_sfxchn,sizeof(UBYTE));

	/* make sure the player doesn't start with garbage */
	for(t=oldchn;t<md_numchn;t++)  Voice_Stop_internal(t);

	sfxpool = 0;
	if(resume) MikMod_EnableOutput_internal();
	_mm_critical = 0;

	return 0;
}

BOOL MikMod_SetNumVoices(int music, int sfx)
{
	BOOL result;

	MUTEX_LOCK(vars);
	result=MikMod_SetNumVoices_internal(music,sfx);
	MUTEX_UNLOCK(vars);

	return result;
}

BOOL MikMod_EnableOutput_internal(void)
{
	_mm_critical = 1;
	if(!isplaying) {
		if(md_driver->PlayStart()) return 1;
		isplaying = 1;
	}
	_mm_critical = 0;
	return 0;
}

BOOL MikMod_EnableOutput(void)
{
	BOOL result;

	MUTEX_LOCK(vars);
	result=MikMod_EnableOutput_internal();
	MUTEX_UNLOCK(vars);

	return result;
}

void MikMod_DisableOutput_internal(void)
{
	if(isplaying && md_driver) {
		isplaying = 0;
		md_driver->PlayStop();
	}
}

void MikMod_DisableOutput(void)
{
	MUTEX_LOCK(vars);
	MikMod_DisableOutput_internal();
	MUTEX_UNLOCK(vars);
}

BOOL MikMod_Active_internal(void)
{
	return isplaying;
}

BOOL MikMod_Active(void)
{
	BOOL result;

	MUTEX_LOCK(vars);
	result=MikMod_Active_internal();
	MUTEX_UNLOCK(vars);

	return result;
}

/* Plays a sound effects sample.  Picks a voice from the number of voices
   allocated for use as sound effects (loops through voices, skipping all active
   criticals).

   Returns the voice that the sound is being played on.                       */
SBYTE Sample_Play_internal(SAMPLE *s,ULONG start,UBYTE flags)
{
	int orig=sfxpool;/* for cases where all channels are critical */
	int c;

	if(!md_sfxchn) return -1;
	if(s->volume>64) s->volume = 64;

	/* check the first location after sfxpool */
	do {
		if(sfxinfo[sfxpool]&SFX_CRITICAL) {
			if(md_driver->VoiceStopped(c=sfxpool+md_sngchn)) {
				sfxinfo[sfxpool]=flags;
				Voice_Play_internal(c,s,start);
				md_driver->VoiceSetVolume(c,s->volume<<2);
				Voice_SetPanning_internal(c,s->panning);
				md_driver->VoiceSetFrequency(c,s->speed);
				sfxpool++;
				if(sfxpool>=md_sfxchn) sfxpool=0;
				return c;
			}
		} else {
			sfxinfo[sfxpool]=flags;
			Voice_Play_internal(c=sfxpool+md_sngchn,s,start);
			md_driver->VoiceSetVolume(c,s->volume<<2);
			Voice_SetPanning_internal(c,s->panning);
			md_driver->VoiceSetFrequency(c,s->speed);
			sfxpool++;
			if(sfxpool>=md_sfxchn) sfxpool=0;
			return c;
		}

		sfxpool++;
		if(sfxpool>=md_sfxchn) sfxpool = 0;
	} while(sfxpool!=orig);

	return -1;
}

SBYTE Sample_Play(SAMPLE *s,ULONG start,UBYTE flags)
{
	SBYTE result;

	MUTEX_LOCK(vars);
	result=Sample_Play_internal(s,start,flags);
	MUTEX_UNLOCK(vars);

	return result;
}

long MikMod_GetVersion(void)
{
	return LIBMIKMOD_VERSION;
}

/*========== MT-safe stuff */

#ifdef HAVE_PTHREAD
#define INIT_MUTEX(name) \
	pthread_mutex_t _mm_mutex_##name=PTHREAD_MUTEX_INITIALIZER
#elif defined(__OS2__)||defined(__EMX__)
#define INIT_MUTEX(name) \
	HMTX _mm_mutex_##name
#elif defined(WIN32)
#define INIT_MUTEX(name) \
	HANDLE _mm_mutex_##name
#else
#define INIT_MUTEX(name) \
	void *_mm_mutex_##name = NULL
#endif

INIT_MUTEX(vars);
INIT_MUTEX(lists);

BOOL MikMod_InitThreads(void)
{
	static int firstcall=1;
	static int result=0;

	if (firstcall) {
		firstcall=0;
#ifdef HAVE_PTHREAD
		result=1;
#elif defined(__OS2__)||defined(__EMX__)
		if(DosCreateMutexSem((PSZ)NULL,&_mm_mutex_lists,0,0) ||
		   DosCreateMutexSem((PSZ)NULL,&_mm_mutex_vars,0,0)) {
			_mm_mutex_lists=_mm_mutex_vars=(HMTX)NULL;
			result=0;
		} else
			result=1;
#elif defined(WIN32)
		if((!(_mm_mutex_lists=CreateMutex(NULL,FALSE,"libmikmod(lists)")))||
		   (!(_mm_mutex_vars=CreateMutex(NULL,FALSE,"libmikmod(vars)"))))
			result=0;
		else
			result=1;
#endif
	}
	return result;
}

void MikMod_Unlock(void)
{
	MUTEX_UNLOCK(lists);
	MUTEX_UNLOCK(vars);
}

void MikMod_Lock(void)
{
	MUTEX_LOCK(vars);
	MUTEX_LOCK(lists);
}

/*========== Parameter extraction helper */

CHAR *MD_GetAtom(CHAR *atomname,CHAR *cmdline,BOOL implicit)
{
	CHAR *ret=NULL;

	if(cmdline) {
		CHAR *buf=strstr(cmdline,atomname);

		if((buf)&&((buf==cmdline)||(*(buf-1)==','))) {
			CHAR *ptr=buf+strlen(atomname);

			if(*ptr=='=') {
				for(buf=++ptr;(*ptr)&&((*ptr)!=',');ptr++);
				ret=(CHAR*)MikMod_malloc((1+ptr-buf)*sizeof(CHAR));
				if(ret)
					strncpy(ret,buf,ptr-buf);
			} else if((*ptr==',')||(!*ptr)) {
				if(implicit) {
					ret=(CHAR*)MikMod_malloc((1+ptr-buf)*sizeof(CHAR));
					if(ret)
						strncpy(ret,buf,ptr-buf);
				}
			}
		}
	}
	return ret;
}

#if defined unix || (defined __APPLE__ && defined __MACH__)

/*========== Posix helper functions */

/* Check if the file is a regular or nonexistant file (or a link to a such a
   file), and that, should the calling program be setuid, the access rights are
   reasonable. Returns 1 if it is safe to rewrite the file, 0 otherwise.
   The goal is to prevent a setuid root libmikmod application from overriding
   files like /etc/passwd with digital sound... */
BOOL MD_Access(CHAR *filename)
{
	struct stat buf;

	if(!stat(filename,&buf)) {
		/* not a regular file ? */
		if(!S_ISREG(buf.st_mode)) return 0;
		/* more than one hard link to the file ? */
		if(buf.st_nlink>1) return 0;
		/* check access rights with the real user and group id */
		if(getuid()==buf.st_uid) {
			if(!(buf.st_mode&S_IWUSR)) return 0;
		} else if(getgid()==buf.st_gid) {
			if(!(buf.st_mode&S_IWGRP)) return 0;
		} else
			if(!(buf.st_mode&S_IWOTH)) return 0;
	}

	return 1;
}

/* Drop all root privileges we might have */
BOOL MD_DropPrivileges(void)
{
	if(!geteuid()) {
		if(getuid()) {
			/* we are setuid root -> drop setuid to become the real user */
			if(setuid(getuid())) return 1;
		} else {
			/* we are run as root -> drop all and become user 'nobody' */
			struct passwd *nobody;
			int uid;

			if(!(nobody=getpwnam("nobody"))) return 1; /* no such user ? */
			uid=nobody->pw_uid;
			if (!uid) /* user 'nobody' has root privileges ? weird... */
				return 1;
			if (setuid(uid)) return 1;
		}
	}
	return 0;
}

#endif

void _mm_registeralldrivers(void)
{
	/* Register network drivers */
#ifdef DRV_AF
	_mm_registerdriver(&drv_AF);
#endif
#ifdef DRV_ESD
	_mm_registerdriver(&drv_esd);
#endif
#ifdef DRV_NAS
	_mm_registerdriver(&drv_nas);
#endif

	/* Register hardware drivers - hardware mixing */
#ifdef DRV_ULTRA
	_mm_registerdriver(&drv_ultra);
#endif

	/* Register hardware drivers - software mixing */
#ifdef DRV_AIX
	_mm_registerdriver(&drv_aix);
#endif
#ifdef DRV_ALSA
	_mm_registerdriver(&drv_alsa);
#endif
#ifdef DRV_HP
	_mm_registerdriver(&drv_hp);
#endif
#ifdef DRV_OSS
	_mm_registerdriver(&drv_oss);
#endif
#ifdef DRV_SGI
	_mm_registerdriver(&drv_sgi);
#endif
#ifdef DRV_SUN
	_mm_registerdriver(&drv_sun);
#endif
#ifdef DRV_DART
	_mm_registerdriver(&drv_dart);
#endif
#ifdef DRV_OS2
	_mm_registerdriver(&drv_os2);
#endif
//#ifdef DRV_DS
	_mm_registerdriver(&drv_ds);
//#endif
#ifdef DRV_WIN
	_mm_registerdriver(&drv_win);
#endif
#ifdef DRV_MAC
	_mm_registerdriver(&drv_mac);
#endif
#ifdef DRV_OSX
	_mm_registerdriver(&drv_osx);
#endif
#ifdef DRV_GP32
	_mm_registerdriver(&drv_gp32);
#endif

	/* dos drivers */
#ifdef DRV_WSS
	/* wss first, since some cards emulate sb */
	_mm_registerdriver(&drv_wss);
#endif
#ifdef DRV_SB
	_mm_registerdriver(&drv_sb);
#endif

	/* Register disk writers */
	//_mm_registerdriver(&drv_raw);
	//_mm_registerdriver(&drv_wav);
#ifdef DRV_AIFF
	_mm_registerdriver(&drv_aiff);
#endif

	/* Register other drivers */
#ifdef DRV_PIPE
	_mm_registerdriver(&drv_pipe);
#endif
  /*
#ifndef macintosh
	_mm_registerdriver(&drv_stdout);
#endif
  */

	_mm_registerdriver(&drv_nos);
}

void MikMod_RegisterAllDrivers(void)
{
	MUTEX_LOCK(lists);
	_mm_registeralldrivers();
	MUTEX_UNLOCK(lists);
}


#include "load_669.cpp"
#include "load_amf.cpp"
#include "load_asy.cpp"
#include "load_dsm.cpp"
#include "load_far.cpp"
#include "load_gdm.cpp"
#include "load_gt2.cpp"
#include "load_imf.cpp"
#include "load_it.cpp"
#include "load_m15.cpp"
#include "load_med.cpp"
#include "load_mod.cpp"
#include "load_mtm.cpp"
#include "load_okt.cpp"
#include "load_s3m.cpp"
#include "load_stm.cpp"
#include "load_stx.cpp"
#include "load_ult.cpp"
#include "load_uni.cpp"
#include "load_xm.cpp"


#ifdef SUNOS
extern int fclose(FILE *);
extern int fgetc(FILE *);
extern int fputc(int, FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern int fseek(FILE *, long, int);
extern size_t fwrite(const void *, size_t, size_t, FILE *);
#endif

#define COPY_BUFSIZE  1024

/* some prototypes */
static BOOL _mm_MemReader_Eof(MREADER* reader);
static BOOL _mm_MemReader_Read(MREADER* reader,void* ptr,size_t size);
static int _mm_MemReader_Get(MREADER* reader);
static BOOL _mm_MemReader_Seek(MREADER* reader,long offset,int whence);
static long _mm_MemReader_Tell(MREADER* reader);

//static long _mm_iobase=0,temp_iobase=0;

FILE* _mm_fopen( const CHAR* fname,CHAR* attrib)
{
	FILE *fp;

	if(!(fp=fopen(fname,attrib))) {
		_mm_errno = MMERR_OPENING_FILE;
		if(_mm_errorhandler) _mm_errorhandler();
	}
	return fp;
}

BOOL _mm_FileExists(CHAR* fname)
{
	FILE *fp;

	if(!(fp=fopen(fname,"r"))) return 0;
	fclose(fp);

	return 1;
}

int _mm_fclose(FILE *fp)
{
	return fclose(fp);
}

/* Sets the current file-position as the new iobase */
void _mm_iobase_setcur(MREADER* reader)
{
	reader->prev_iobase=reader->iobase;  /* store old value in case of revert */
	reader->iobase=reader->Tell(reader);
}

/* Reverts to the last known iobase value. */
void _mm_iobase_revert(MREADER* reader)
{
	reader->iobase=reader->prev_iobase;
}

/*========== File Reader */

typedef struct MFILEREADER {
	MREADER core;
	FILE*   file;
} MFILEREADER;

static BOOL _mm_FileReader_Eof(MREADER* reader)
{
	return feof(((MFILEREADER*)reader)->file);
}

static BOOL _mm_FileReader_Read(MREADER* reader,void* ptr,size_t size)
{
	return !!fread(ptr,size,1,((MFILEREADER*)reader)->file);
}

static int _mm_FileReader_Get(MREADER* reader)
{
	return fgetc(((MFILEREADER*)reader)->file);
}

static BOOL _mm_FileReader_Seek(MREADER* reader,long offset,int whence)
{
	return fseek(((MFILEREADER*)reader)->file,
				 (whence==SEEK_SET)?offset+reader->iobase:offset,whence);
}

static long _mm_FileReader_Tell(MREADER* reader)
{
	return ftell(((MFILEREADER*)reader)->file)-reader->iobase;
}

MREADER *_mm_new_file_reader(FILE* fp)
{
	MFILEREADER* reader=(MFILEREADER*)MikMod_malloc(sizeof(MFILEREADER));
	if (reader) {
		reader->core.Eof =&_mm_FileReader_Eof;
		reader->core.Read=&_mm_FileReader_Read;
		reader->core.Get =&_mm_FileReader_Get;
		reader->core.Seek=&_mm_FileReader_Seek;
		reader->core.Tell=&_mm_FileReader_Tell;
		reader->file=fp;
	}
	return (MREADER*)reader;
}

void _mm_delete_file_reader (MREADER* reader)
{
	if(reader) MikMod_free(reader);
}

/*========== File Writer */

typedef struct MFILEWRITER {
	MWRITER core;
	FILE*   file;
} MFILEWRITER;

static BOOL _mm_FileWriter_Seek(MWRITER* writer,long offset,int whence)
{
	return fseek(((MFILEWRITER*)writer)->file,offset,whence);
}

static long _mm_FileWriter_Tell(MWRITER* writer)
{
	return ftell(((MFILEWRITER*)writer)->file);
}

static BOOL _mm_FileWriter_Write(MWRITER* writer,void* ptr,size_t size)
{
	return (fwrite(ptr,size,1,((MFILEWRITER*)writer)->file)==size);
}

static BOOL _mm_FileWriter_Put(MWRITER* writer,int value)
{
	return fputc(value,((MFILEWRITER*)writer)->file);
}

MWRITER *_mm_new_file_writer(FILE* fp)
{
	MFILEWRITER* writer=(MFILEWRITER*)MikMod_malloc(sizeof(MFILEWRITER));
	if (writer) {
		writer->core.Seek =&_mm_FileWriter_Seek;
		writer->core.Tell =&_mm_FileWriter_Tell;
		writer->core.Write=&_mm_FileWriter_Write;
		writer->core.Put  =&_mm_FileWriter_Put;
		writer->file=fp;
	}
	return (MWRITER*) writer;
}

void _mm_delete_file_writer (MWRITER* writer)
{
	if(writer) MikMod_free (writer);
}

/*========== Memory Reader */


typedef struct MMEMREADER {
	MREADER core;
	const void *buffer;
	long len;
	long pos;
} MMEMREADER;

void _mm_delete_mem_reader(MREADER* reader)
{
	if (reader) { MikMod_free(reader); }
}

MREADER *_mm_new_mem_reader(const void *buffer, int len)
{
	MMEMREADER* reader=(MMEMREADER*)MikMod_malloc(sizeof(MMEMREADER));
	if (reader)
	{
		reader->core.Eof =&_mm_MemReader_Eof;
		reader->core.Read=&_mm_MemReader_Read;
		reader->core.Get =&_mm_MemReader_Get;
		reader->core.Seek=&_mm_MemReader_Seek;
		reader->core.Tell=&_mm_MemReader_Tell;
		reader->buffer = buffer;
		reader->len = len;
		reader->pos = 0;
	}
	return (MREADER*)reader;
}

static BOOL _mm_MemReader_Eof(MREADER* reader)
{
	if (!reader) { return 1; }
	if ( ((MMEMREADER*)reader)->pos > ((MMEMREADER*)reader)->len ) {
		return 1;
	}
	return 0;
}

static BOOL _mm_MemReader_Read(MREADER* reader,void* ptr,size_t size)
{
	unsigned char *d=(unsigned char*)ptr;
	const unsigned char *s;

	if (!reader) { return 0; }

	if (reader->Eof(reader)) { return 0; }

	s = (unsigned char*)((MMEMREADER*)reader)->buffer;
	s += ((MMEMREADER*)reader)->pos;

	if ( (size_t)((MMEMREADER*)reader)->pos + size > (size_t)((MMEMREADER*)reader)->len)
	{
		((MMEMREADER*)reader)->pos = ((MMEMREADER*)reader)->len;
		return 0; /* not enough remaining bytes */
	}

	((MMEMREADER*)reader)->pos += (long)size;

	while (size--)
	{
		*d = *s;
		s++;
		d++;
	}

	return 1;
}

static int _mm_MemReader_Get(MREADER* reader)
{
	int pos;

	if (reader->Eof(reader)) { return 0; }

	pos = ((MMEMREADER*)reader)->pos;
	((MMEMREADER*)reader)->pos++;

	return ((unsigned char*)(((MMEMREADER*)reader)->buffer))[pos];
}

static BOOL _mm_MemReader_Seek(MREADER* reader,long offset,int whence)
{
	if (!reader) { return -1; }

	switch(whence)
	{
		case SEEK_CUR:
			((MMEMREADER*)reader)->pos += offset;
			break;
		case SEEK_SET:
			((MMEMREADER*)reader)->pos = offset;
			break;
		case SEEK_END:
			((MMEMREADER*)reader)->pos = ((MMEMREADER*)reader)->len - offset - 1;
			break;
	}
	if ( ((MMEMREADER*)reader)->pos < 0) { ((MMEMREADER*)reader)->pos = 0; }
	if ( ((MMEMREADER*)reader)->pos > ((MMEMREADER*)reader)->len ) {
		((MMEMREADER*)reader)->pos = ((MMEMREADER*)reader)->len;
	}
	return 0;
}

static long _mm_MemReader_Tell(MREADER* reader)
{
	if (reader) {
		return ((MMEMREADER*)reader)->pos;
	}
	return 0;
}

/*========== Write functions */

void _mm_write_string(CHAR* data,MWRITER* writer)
{
	if(data)
		_mm_write_UBYTES(data,strlen(data),writer);
}

void _mm_write_M_UWORD(UWORD data,MWRITER* writer)
{
	_mm_write_UBYTE(data>>8,writer);
	_mm_write_UBYTE(data&0xff,writer);
}

void _mm_write_I_UWORD(UWORD data,MWRITER* writer)
{
	_mm_write_UBYTE(data&0xff,writer);
	_mm_write_UBYTE(data>>8,writer);
}

void _mm_write_M_ULONG(ULONG data,MWRITER* writer)
{
	_mm_write_M_UWORD( (UWORD)( data >> 16 ),writer);
	_mm_write_M_UWORD( (UWORD)( data & 0xffff ),writer);
}

void _mm_write_I_ULONG(ULONG data,MWRITER* writer)
{
	_mm_write_I_UWORD( (UWORD)( data & 0xffff ),writer);
	_mm_write_I_UWORD( (UWORD)( data >> 16 ),writer);
}

void _mm_write_M_SWORD(SWORD data,MWRITER* writer)
{
	_mm_write_M_UWORD((UWORD)data,writer);
}

void _mm_write_I_SWORD(SWORD data,MWRITER* writer)
{
	_mm_write_I_UWORD((UWORD)data,writer);
}

void _mm_write_M_SLONG(SLONG data,MWRITER* writer)
{
	_mm_write_M_ULONG((ULONG)data,writer);
}

void _mm_write_I_SLONG(SLONG data,MWRITER* writer)
{
	_mm_write_I_ULONG((ULONG)data,writer);
}

#if defined __STDC__ || defined _MSC_VER || defined MPW_C
#define DEFINE_MULTIPLE_WRITE_FUNCTION(type_name,type)						\
void _mm_write_##type_name##S (type *buffer,int number,MWRITER* writer)		\
{																			\
	while(number-->0)														\
		_mm_write_##type_name(*(buffer++),writer);							\
}
#else
#define DEFINE_MULTIPLE_WRITE_FUNCTION(type_name,type)						\
void _mm_write_/**/type_name/**/S (type *buffer,int number,MWRITER* writer)	\
{																			\
	while(number-->0)														\
		_mm_write_/**/type_name(*(buffer++),writer);						\
}
#endif

DEFINE_MULTIPLE_WRITE_FUNCTION(M_SWORD,SWORD)
DEFINE_MULTIPLE_WRITE_FUNCTION(M_UWORD,UWORD)
DEFINE_MULTIPLE_WRITE_FUNCTION(I_SWORD,SWORD)
DEFINE_MULTIPLE_WRITE_FUNCTION(I_UWORD,UWORD)

DEFINE_MULTIPLE_WRITE_FUNCTION(M_SLONG,SLONG)
DEFINE_MULTIPLE_WRITE_FUNCTION(M_ULONG,ULONG)
DEFINE_MULTIPLE_WRITE_FUNCTION(I_SLONG,SLONG)
DEFINE_MULTIPLE_WRITE_FUNCTION(I_ULONG,ULONG)

/*========== Read functions */

int _mm_read_string(CHAR* buffer,int number,MREADER* reader)
{
	return reader->Read(reader,buffer,number);
}

UWORD _mm_read_M_UWORD(MREADER* reader)
{
	UWORD result=((UWORD)_mm_read_UBYTE(reader))<<8;
	result|=_mm_read_UBYTE(reader);
	return result;
}

UWORD _mm_read_I_UWORD(MREADER* reader)
{
	UWORD result=_mm_read_UBYTE(reader);
	result|=((UWORD)_mm_read_UBYTE(reader))<<8;
	return result;
}

ULONG _mm_read_M_ULONG(MREADER* reader)
{
	ULONG result=((ULONG)_mm_read_M_UWORD(reader))<<16;
	result|=_mm_read_M_UWORD(reader);
	return result;
}

ULONG _mm_read_I_ULONG(MREADER* reader)
{
	ULONG result=_mm_read_I_UWORD(reader);
	result|=((ULONG)_mm_read_I_UWORD(reader))<<16;
	return result;
}

SWORD _mm_read_M_SWORD(MREADER* reader)
{
	return((SWORD)_mm_read_M_UWORD(reader));
}

SWORD _mm_read_I_SWORD(MREADER* reader)
{
	return((SWORD)_mm_read_I_UWORD(reader));
}

SLONG _mm_read_M_SLONG(MREADER* reader)
{
	return((SLONG)_mm_read_M_ULONG(reader));
}

SLONG _mm_read_I_SLONG(MREADER* reader)
{
	return((SLONG)_mm_read_I_ULONG(reader));
}

#if defined __STDC__ || defined _MSC_VER || defined MPW_C
#define DEFINE_MULTIPLE_READ_FUNCTION(type_name,type)						\
int _mm_read_##type_name##S (type *buffer,int number,MREADER* reader)		\
{																			\
	while(number-->0)														\
		*(buffer++)=_mm_read_##type_name(reader);							\
	return !reader->Eof(reader);											\
}
#else
#define DEFINE_MULTIPLE_READ_FUNCTION(type_name,type)						\
int _mm_read_/**/type_name/**/S (type *buffer,int number,MREADER* reader)	\
{																			\
	while(number-->0)														\
		*(buffer++)=_mm_read_/**/type_name(reader);							\
	return !reader->Eof(reader);											\
}
#endif

DEFINE_MULTIPLE_READ_FUNCTION(M_SWORD,SWORD)
DEFINE_MULTIPLE_READ_FUNCTION(M_UWORD,UWORD)
DEFINE_MULTIPLE_READ_FUNCTION(I_SWORD,SWORD)
DEFINE_MULTIPLE_READ_FUNCTION(I_UWORD,UWORD)

DEFINE_MULTIPLE_READ_FUNCTION(M_SLONG,SLONG)
DEFINE_MULTIPLE_READ_FUNCTION(M_ULONG,ULONG)
DEFINE_MULTIPLE_READ_FUNCTION(I_SLONG,SLONG)
DEFINE_MULTIPLE_READ_FUNCTION(I_ULONG,ULONG)


CHAR *_mm_errmsg[MMERR_MAX+1] =
{
/* No error */

	"No error",

/* Generic errors */

	"Could not open requested file",
	"Out of memory",
	"Dynamic linking failed",

/* Sample errors */

	"Out of memory to load sample",
	"Out of sample handles to load sample",
	"Sample format not recognized",

/* Module errors */

	"Failure loading module pattern",
	"Failure loading module track",
	"Failure loading module header",
	"Failure loading sampleinfo",
	"Module format not recognized",
	"Module sample format not recognized",
	"Synthsounds not supported in MED files",
	"Compressed sample is invalid",

/* Driver errors: */

	"Sound device not detected",
	"Device number out of range",
	"Software mixer failure",
	"Could not open sound device",
	"This driver supports 8 bit linear output only",
	"This driver supports 16 bit linear output only",
	"This driver supports stereo output only",
	"This driver supports uLaw output (8 bit mono, 8 kHz) only",
	"Unable to set non-blocking mode for audio device",

/* AudioFile driver errors  */

	"Cannot find suitable AudioFile audio port",

/* AIX driver errors */

	"Configuration (init step) of audio device failed",
	"Configuration (control step) of audio device failed",
	"Configuration (start step) of audio device failed",

/* ALSA driver errors */

/* EsounD driver errors */

/* Ultrasound driver errors */

	"Ultrasound driver only works in 16 bit stereo 44 KHz",
	"Ultrasound card could not be reset",
	"Could not start Ultrasound timer",

/* HP driver errors  */

	"Unable to select 16bit-linear sample format",
	"Could not select requested sample-rate",
	"Could not select requested number of channels",
	"Unable to select audio output",
	"Unable to get audio description",
	"Could not set transmission buffer size",

/* Open Sound System driver errors */

	"Could not set fragment size",
	"Could not set sample size",
	"Could not set mono/stereo setting",
	"Could not set sample rate",

/* SGI driver errors */

	"Unsupported sample rate",
	"Hardware does not support 16 bit sound",
	"Hardware does not support 8 bit sound",
	"Hardware does not support stereo sound",
	"Hardware does not support mono sound",

/* Sun driver errors */

	"Sound device initialization failed",

/* OS/2 drivers errors */

	"Could not set mixing parameters",
	"Could not create playback semaphores",
	"Could not create playback timer",
	"Could not create playback thread",

/* DirectSound driver errors */

	"Could not set playback priority",
	"Could not create playback buffers",
	"Could not set playback format",
	"Could not register callback",
	"Could not register event",
	"Could not create playback thread",
	"Could not initialize playback thread",

/* Windows Multimedia API driver errors */

	"Invalid device handle",
	"The resource is already allocated",
	"Invalid device identifier",
	"Unsupported output format",
	"Unknown error",

/* Macintosh driver errors */

	"Unsupported sample rate",
	"Could not start playback",

/* MacOS X/Darwin driver errors */

	"Unknown device",
	"Bad property",
	"Could not set playback format",
	"Could not set mono/stereo setting",
	"Could not create playback buffers",
	"Could not create playback thread",
	"Could not start audio device",
	"Could not create buffer thread",

/* DOS driver errors */

	"WSS_STARTDMA",
	"SB_STARTDMA",

/* Invalid error */

	"Invalid error code"
};

char *MikMod_strerror(int code)
{
	if ((code<0)||(code>MMERR_MAX)) code=MMERR_MAX+1;
	return _mm_errmsg[code];
}

/* User installed error callback */
MikMod_handler_t _mm_errorhandler = NULL;
int  _mm_errno = 0;
BOOL _mm_critical = 0;

MikMod_handler_t _mm_registererrorhandler(MikMod_handler_t proc)
{
	MikMod_handler_t oldproc=_mm_errorhandler;

	_mm_errorhandler = proc;
	return oldproc;
}

MikMod_handler_t MikMod_RegisterErrorHandler(MikMod_handler_t proc)
{
	MikMod_handler_t result;

	MUTEX_LOCK(vars);
		result=_mm_registererrorhandler(proc);
	MUTEX_UNLOCK(vars);

	return result;
}


#define ALIGN_STRIDE 16

static void * align_pointer(char *ptr, size_t stride)
{
	char *pptr = ptr + sizeof(void*);
	char *fptr;
	size_t err = ((size_t)pptr)&(stride-1);
	if (err)
		fptr = pptr + (stride - err);
	else
		fptr = pptr;
	*(size_t*)(fptr - sizeof(void*)) = (size_t)ptr;
	return fptr;
}

static void *get_pointer(void *data)
{
	unsigned char *_pptr = (unsigned char*)data - sizeof(void*);
	size_t _ptr = *(size_t*)_pptr;
	return (void*)_ptr;
}


void* MikMod_realloc(void *data, size_t size)
{
	if (data)
	{
#if defined __MACH__
		void *d = realloc(data, size);
		if (d)
		{
			return d;
		}
		return 0;
#elif defined _WIN32 || defined _WIN64
		//return _aligned_realloc(data, size, ALIGN_STRIDE);
    return realloc( data, size );
#else
		//unsigned char *newPtr = (unsigned char *)realloc(get_pointer(data), size + ALIGN_STRIDE + sizeof(void*));
    unsigned char *newPtr = (unsigned char *)realloc(get_pointer(data), size );
		//return align_pointer(newPtr, ALIGN_STRIDE);
    return newPtr;
#endif
	}
	return MikMod_malloc(size);
}


/* Same as malloc, but sets error variable _mm_error when fails. Returns a 16-byte aligned pointer */
void* MikMod_malloc(size_t size)
{
#if defined __MACH__
	void *d = calloc(1, size);
	if (d)
	{
		return d;
	}
	return 0;
#elif defined _WIN32 || defined _WIN64
	//void * d = _aligned_malloc(size, ALIGN_STRIDE);
  void * d = malloc( size );
	if (d)
	{
		ZeroMemory(d, size);
		return d;
	}
	return 0;
#else
	//void *d = calloc(1, size + ALIGN_STRIDE + sizeof(void*));
  void *d = calloc(1, size );

	if(!d) {
		_mm_errno = MMERR_OUT_OF_MEMORY;
		if(_mm_errorhandler) _mm_errorhandler();
	}
	//return align_pointer(d, ALIGN_STRIDE);
  return d;
#endif
}

/* Same as calloc, but sets error variable _mm_error when fails */
void* MikMod_calloc(size_t nitems,size_t size)
{
#if defined __MACH__
	void *d = calloc(nitems, size);
	if (d)
	{
		return d;
	}
	return 0;
#elif defined _WIN32 || defined _WIN64
	//void * d = _aligned_malloc(size * nitems, ALIGN_STRIDE);
  void * d = malloc(size * nitems);
	if (d)
	{
		ZeroMemory(d, size * nitems);
		return d;
	}
	return 0;
#else
	//void *d = calloc(nitems, size + ALIGN_STRIDE + sizeof(void*));
  void *d = calloc(nitems, size );

	if(!d) {
		_mm_errno = MMERR_OUT_OF_MEMORY;
		if(_mm_errorhandler) _mm_errorhandler();
	}
	//return align_pointer(d, ALIGN_STRIDE);
  return d;
#endif
}

void MikMod_free(void *data)
{
	if (data)
	{
#if defined __MACH__
		free(data);
#elif defined _WIN32 || defined _WIN64
		//_aligned_free(data);
    free(data);
#else
		//free(get_pointer(data));
    free(data);
#endif
	}
}



#define ZEROLEN 32768

static	SBYTE *zerobuf=NULL;

static BOOL NS_IsThere(void)
{
	return 1;
}

static BOOL NS_Init(void)
{
	zerobuf=(SBYTE*)MikMod_malloc(ZEROLEN);
	return VC_Init();
}

static void NS_Exit(void)
{
	VC_Exit();
	MikMod_free(zerobuf);
}

static void NS_Update(void)
{
	if (zerobuf)
		VC_WriteBytes(zerobuf,ZEROLEN);
}

MDRIVER drv_nos={
	NULL,
	"No Sound",
	"Nosound Driver v3.0",
	255,255,
	"nosound",
	NULL,
	NULL,
	NS_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	NS_Init,
	NS_Exit,
	NULL,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	NS_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};



// NEU
//DWORD dwSoundBufferHandle = 0;
//XSound* pSoundClass = NULL;

#include <memory.h>
#include <string.h>

#define INITGUID
#include <dsound.h>

/* DSBCAPS_CTRLALL is not defined anymore with DirectX 7. Of course DirectSound
   is a coherent, backwards compatible API... */
#ifndef DSBCAPS_CTRLALL
#define DSBCAPS_CTRLALL ( DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME | \
						  DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY | \
						  DSBCAPS_CTRL3D )
#endif

/* size of each buffer */
#define FRAGSIZE 16
/* buffer count */
#define UPDATES  2

static LPDIRECTSOUND pSoundCard = NULL;
static LPDIRECTSOUNDBUFFER pPrimarySoundBuffer = NULL, pSoundBuffer = NULL;
static LPDIRECTSOUNDNOTIFY pSoundBufferNotify = NULL;

static HANDLE notifyUpdateHandle = 0, updateBufferHandle = 0;
static BOOL threadInUse = FALSE;
static int fragsize=1<<FRAGSIZE;
static DWORD controlflags = DSBCAPS_CTRLALL & ~DSBCAPS_GLOBALFOCUS;

#define SAFE_RELEASE(p) \
	do { \
		if (p) { \
			(p)->Release(); \
			(p) = NULL;	\
		} \
	} while (0)

static DWORD WINAPI updateBufferProc(LPVOID lpParameter)
{
	LPVOID pBlock1 = NULL, pBlock2 = NULL;
	DWORD soundBufferCurrentPosition, blockBytes1, blockBytes2;
	DWORD start;

  DWORD   dwPrevPos = fragsize * UPDATES;

	while (threadInUse)
  {
    /*
    DWORD dwResult = MsgWaitForMultipleObjects( 1, &notifyUpdateHandle, FALSE, INFINITE, QS_ALLEVENTS );
    if ( dwResult == WAIT_OBJECT_0 )
    */
		//if ( WaitForSingleObject( notifyUpdateHandle, INFINITE ) == WAIT_OBJECT_0 )
    Sleep( 70 );
    {

			if (!threadInUse) break;

      //dh::Log( "Get Pos from %d", dwSoundBufferHandle );

      // Ersatz-Code
      //soundBufferCurrentPosition = pSoundClass->GetPos( dwSoundBufferHandle );

      // Orig-Code
			pSoundBuffer->GetCurrentPosition( &soundBufferCurrentPosition, NULL );

      DWORD     dwCurFragment = soundBufferCurrentPosition / fragsize;

      if ( dwCurFragment != dwPrevPos / fragsize )
      {
        //dh::Log( "CurPos %d < %d", soundBufferCurrentPosition, fragsize );
			  if ( soundBufferCurrentPosition < (DWORD)fragsize )
        {
				  start = fragsize;
        }
			  else
        {
				  start = 0;
        }

        //dh::Log( "lock at %d", start );

        // Orig-Code
			  if (pSoundBuffer->Lock
						  (start,fragsize,&pBlock1,&blockBytes1,
						  &pBlock2,&blockBytes2,0)==DSERR_BUFFERLOST) {
				  pSoundBuffer->Restore();
				  pSoundBuffer->Lock
						  (start,fragsize,&pBlock1,&blockBytes1,
						  &pBlock2,&blockBytes2,0);
			  }
        // Orig-Code Ende

        // ErsatzCode
              /*
        if ( !pSoundClass->LockModifyableBuffer( dwSoundBufferHandle, start, fragsize, &pBlock1, (GR::u32*)&blockBytes1, &pBlock2, (GR::u32*)&blockBytes2 ) )
        {
          dh::Log( "Lock failed" );
        }
        */

        //dh::Log( "Locked2 %x (%d)", pBlock1, blockBytes1 );

			  MUTEX_LOCK(vars);
			  if (Player_Paused_internal()) {
				  VC_SilenceBytes((SBYTE*)pBlock1,(ULONG)blockBytes1);
				  if (pBlock2)
					  VC_SilenceBytes((SBYTE*)pBlock2,(ULONG)blockBytes2);
			  } else {
				  VC_WriteBytes((SBYTE*)pBlock1,(ULONG)blockBytes1);
				  if (pBlock2)
					  VC_WriteBytes((SBYTE*)pBlock2,(ULONG)blockBytes2);
			  }
			  MUTEX_UNLOCK(vars);

        // Ersatzcode
        //pSoundClass->UnlockModifyableBuffer( dwSoundBufferHandle );

        // Orig-Code
			  pSoundBuffer->Unlock( pBlock1, blockBytes1, pBlock2, blockBytes2 );


        dwPrevPos = soundBufferCurrentPosition;
      }
		}
	}
	return 0;
}

static void DS_CommandLine(CHAR *cmdline)
{
	CHAR *ptr=MD_GetAtom("buffer",cmdline,0);

	if (ptr) {
		int buf=atoi(ptr);

		if ((buf<12)||(buf>19)) buf=FRAGSIZE;
		fragsize=1<<buf;

		MikMod_free(ptr);
	}

	if ((ptr=MD_GetAtom("globalfocus",cmdline,1))) {
		controlflags |= DSBCAPS_GLOBALFOCUS;
		MikMod_free(ptr);
	} else
		controlflags &= ~DSBCAPS_GLOBALFOCUS;
}

static BOOL DS_IsPresent(void)
{
  return 1;
  /*
	if(DirectSoundCreate(NULL,&pSoundCard,NULL)!=DS_OK)
		return 0;
	SAFE_RELEASE(pSoundCard);
	return 1;
  */
}

static BOOL DS_Init(void)
{
  DWORD updateBufferThreadID;

  // DS-eigener-Code
	DSBUFFERDESC soundBufferFormat;
	WAVEFORMATEX pcmwf;

	//DSBPOSITIONNOTIFY positionNotifications[2];


	if (DirectSoundCreate(NULL,&pSoundCard,NULL)!=DS_OK) {
		_mm_errno=MMERR_OPENING_AUDIO;
		return 1;
	}

	if (pSoundCard->SetCooperativeLevel
				( g_hwndMain,DSSCL_PRIORITY)!=DS_OK) {
		_mm_errno=MMERR_DS_PRIORITY;
		return 1;
	}

        ::memset(&soundBufferFormat,0,sizeof(DSBUFFERDESC));
    soundBufferFormat.dwSize       =sizeof(DSBUFFERDESC);
    soundBufferFormat.dwFlags      =DSBCAPS_PRIMARYBUFFER;
    soundBufferFormat.dwBufferBytes=0;
    soundBufferFormat.lpwfxFormat  =NULL;

	if (pSoundCard->CreateSoundBuffer
				(&soundBufferFormat,&pPrimarySoundBuffer,NULL)!=DS_OK) {
		_mm_errno=MMERR_DS_BUFFER;
		return 1;
	}



  ::memset(&pcmwf,0,sizeof(WAVEFORMATEX));
	pcmwf.wFormatTag     =WAVE_FORMAT_PCM;
	pcmwf.nChannels      =(md_mode&DMODE_STEREO)?2:1;
	pcmwf.nSamplesPerSec =md_mixfreq;
	pcmwf.wBitsPerSample =(md_mode&DMODE_16BITS)?16:8;
	pcmwf.nBlockAlign    =(pcmwf.wBitsPerSample * pcmwf.nChannels) / 8;
	pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec*pcmwf.nBlockAlign;

    if (pPrimarySoundBuffer->SetFormat
				(&pcmwf)!=DS_OK) {
		_mm_errno=MMERR_DS_FORMAT;
		return 1;
	}
    pPrimarySoundBuffer->Play(0,0,DSBPLAY_LOOPING);

    ::memset(&soundBufferFormat,0,sizeof(DSBUFFERDESC));
    soundBufferFormat.dwSize       =sizeof(DSBUFFERDESC);
    soundBufferFormat.dwFlags      =controlflags|DSBCAPS_GETCURRENTPOSITION2 ;
    soundBufferFormat.dwBufferBytes=fragsize*UPDATES;
    soundBufferFormat.lpwfxFormat  =&pcmwf;

    // Mein Ersatzcode
    /*
  int   nChannels = ( md_mode & DMODE_STEREO ) ? 2 : 1;
  int   nBitsPerSample = ( md_mode & DMODE_16BITS ) ? 16 : 8;
  int   nBlockAlign = ( nBitsPerSample * nChannels ) / 8;

  dwSoundBufferHandle = pSoundClass->CreateModifyableBuffer( nChannels,
                                                             md_mixfreq,
                                                             nBlockAlign,
                                                             nBitsPerSample,
                                                             fragsize * UPDATES,
                                                             false,
                                                             XSound::ST_MUSIC );
                                                             */

  // DS-Eigener code

	if (pSoundCard->CreateSoundBuffer
				(&soundBufferFormat,&pSoundBuffer,NULL)!=DS_OK) {
		_mm_errno=MMERR_DS_BUFFER;
		return 1;
	}

	pSoundBuffer->QueryInterface
				(IID_IDirectSoundNotify,(LPVOID*)&pSoundBufferNotify);
	if (!pSoundBufferNotify) {
		_mm_errno=MMERR_DS_NOTIFY;
		return 1;
	}

  // DS-Eigener code Ende


  /*
  // wird mit Sleep und FragPos nicht mehr benötigt
  notifyUpdateHandle=CreateEvent
				(NULL,FALSE,FALSE,"XMPlayer positionNotify Event" );
	if (!notifyUpdateHandle) {
		XMPlay::_mm_errno=MMERR_DS_EVENT;
		return 1;
	}
  */

  // Mein Ersatzcode
  /*
  pSoundClass->AddNotification( dwSoundBufferHandle, 0, notifyUpdateHandle );
  pSoundClass->AddNotification( dwSoundBufferHandle, fragsize, notifyUpdateHandle );
  */


	updateBufferHandle=CreateThread
				(NULL,0,updateBufferProc,NULL,CREATE_SUSPENDED,&updateBufferThreadID);
	if (!updateBufferHandle) {
		_mm_errno=MMERR_DS_THREAD;
		return 1;
	}

  /*
  // DS-Eigener Code
  XMPlay::memset(positionNotifications,0,2*sizeof(DSBPOSITIONNOTIFY));
	positionNotifications[0].dwOffset    =0;
	positionNotifications[0].hEventNotify=notifyUpdateHandle;
	positionNotifications[1].dwOffset    =fragsize;
	positionNotifications[1].hEventNotify=notifyUpdateHandle;

	if (pSoundBufferNotify->SetNotificationPositions
				(2,positionNotifications) != DS_OK) {
		_mm_errno=MMERR_DS_UPDATE;
		return 1;
	}
  */
	if (IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
	{
		md_mode|=DMODE_SIMDMIXER;
	}
	return VC_Init();
}

static void DS_Exit(void)
{
	//

	if(updateBufferHandle) {

		/* Signal thread to exit and wait for the exit */
		if (threadInUse) {
			threadInUse = 0;
			MUTEX_UNLOCK(vars);
			//SetEvent (notifyUpdateHandle);
			WaitForSingleObject (updateBufferHandle, INFINITE);
			MUTEX_LOCK(vars);
		}

		CloseHandle(updateBufferHandle),
		updateBufferHandle = 0;
	}
  /*
	if (notifyUpdateHandle)
  {
		CloseHandle(notifyUpdateHandle),
		notifyUpdateHandle = 0;
	}
  */

  // NEU
  // Mein Ersatzcode
  /*
  pSoundClass->ReleaseWave( dwSoundBufferHandle );
  dwSoundBufferHandle = 0;
  */

  // Orig-Code
  DWORD statusInfo;

	SAFE_RELEASE(pSoundBufferNotify);
	if(pSoundBuffer) {
		if(pSoundBuffer->GetStatus(&statusInfo)==DS_OK)
			if(statusInfo&DSBSTATUS_PLAYING)
				pSoundBuffer->Stop();
		SAFE_RELEASE(pSoundBuffer);
	}

	if(pPrimarySoundBuffer) {
		if(pPrimarySoundBuffer->GetStatus
						(&statusInfo)==DS_OK)
			if(statusInfo&DSBSTATUS_PLAYING)
				pPrimarySoundBuffer->Stop();
		SAFE_RELEASE(pPrimarySoundBuffer);
	}

	SAFE_RELEASE(pSoundCard);
  // Orig-Code Ende

	VC_Exit();
}

static BOOL do_update = 0;

static void DS_Update(void)
{
	LPVOID block = NULL;
	DWORD bBytes = 0;

	/* Do first update in DS_Update() to be consistent with other
	   non threaded drivers. */
	//if (do_update && pSoundBuffer)
  if ( do_update )
  {
		do_update = 0;

    // Orig-Code
		if (pSoundBuffer->Lock (0, fragsize,
										&block, &bBytes, NULL, NULL, 0)
				== DSERR_BUFFERLOST) {
			pSoundBuffer->Restore ();
			pSoundBuffer->Lock (0, fragsize,
										&block, &bBytes, NULL, NULL, 0);
		}
    // Orig-Code Ende

    // NEU - Ersatzcode
    //pSoundClass->LockModifyableBuffer( dwSoundBufferHandle, 0, fragsize, &block, (GR::u32*)&bBytes, NULL, NULL );

		if (Player_Paused_internal()) {
			VC_SilenceBytes ((SBYTE *)block, (ULONG)bBytes);
		} else {
			VC_WriteBytes ((SBYTE *)block, (ULONG)bBytes);
		}

    // Orig-Code
		pSoundBuffer->Unlock (block, bBytes, NULL, 0);
		pSoundBuffer->SetCurrentPosition(0);
		pSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
    // Orig-Code Ende

    // Ersatzcode
    /*
    pSoundClass->UnlockModifyableBuffer( dwSoundBufferHandle );

    pSoundClass->SetPos( dwSoundBufferHandle, 0 );
    pSoundClass->Loop( dwSoundBufferHandle );
    */


		threadInUse=1;
    ResumeThread( updateBufferHandle );

    //g_dwMMTimerID = timeSetEvent( 500, 0, (LPTIMECALLBACK)XMPlay::notifyUpdateHandle, NULL, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET | TIME_KILL_SYNCHRONOUS );

    //SetEvent( XMPlay::notifyUpdateHandle );
	}
}

static void DS_PlayStop(void)
{
	do_update = 0;

  // Orig-Code
	if (pSoundBuffer)
		pSoundBuffer->Stop();

  // Ersatzcode
  //pSoundClass->Stop( dwSoundBufferHandle );

	VC_PlayStop();
}

static BOOL DS_PlayStart(void)
{
	do_update = 1;
  // NEU

  // Mein Ersatzcode
  //pSoundClass->Loop( dwSoundBufferHandle );

	return VC_PlayStart();
}

MDRIVER drv_ds=
{
	NULL,
	"DirectSound",
	"DirectSound Driver (DX6+) v0.4",
	0,255,
	"ds",
	"buffer:r:12,19,16:Audio buffer log2 size\n"
        "globalfocus:b:0:Play if window does not have the focus\n",
	DS_CommandLine,
	DS_IsPresent,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	DS_Init,
	DS_Exit,
	NULL,
	VC_SetNumVoices,
	DS_PlayStart,
	DS_PlayStop,
	DS_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};


namespace VC2
{

/*
   Constant Definitions
   ====================

  MAXVOL_FACTOR (was BITSHIFT in virtch.c)
    Controls the maximum volume of the output data. All mixed data is
    divided by this number after mixing, so larger numbers result in
    quieter mixing.  Smaller numbers will increase the likeliness of
    distortion on loud modules.

  REVERBERATION_VC2
    Larger numbers result in shorter reverb duration. Longer reverb
    durations can cause unwanted static and make the reverb sound more
    like a crappy echo.

  SAMPLING_SHIFT
    Specified the shift multiplier which controls by how much the mixing
    rate is multiplied while mixing.  Higher values can improve quality by
    smoothing the sound and reducing pops and clicks. Note, this is a shift
    value, so a value of 2 becomes a mixing-rate multiplier of 4, and a
    value of 3 = 8, etc.

  FRACBITS_VC2
    The number of bits per integer devoted to the fractional part of the
    number. Generally, this number should not be changed for any reason.

  !!! IMPORTANT !!! All values below MUST ALWAYS be greater than 0

*/

#define BITSHIFT 9
#define MAXVOL_FACTOR (1<<BITSHIFT)
#define REVERBERATION_VC2 11000L

#define SAMPLING_SHIFT 2
#define SAMPLING_FACTOR (1UL<<SAMPLING_SHIFT)

#define FRACBITS_VC2 28
#define FRACMASK_VC2 ((1UL<<FRACBITS_VC2)-1UL)

#define TICKLSIZE 8192
#define TICKWSIZE_VC2 (TICKLSIZE * 2)
#define TICKBSIZE_VC2 (TICKWSIZE_VC2 * 2)

#define CLICK_SHIFT_VC2_BASE 6
#define CLICK_SHIFT_VC2 (CLICK_SHIFT_VC2_BASE + SAMPLING_SHIFT)
#define CLICK_BUFFER_VC2 (1L << CLICK_SHIFT_VC2)

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

typedef struct VINFO {
  UBYTE     kick;              /* =1 -> sample has to be restarted */
  UBYTE     active;            /* =1 -> sample is playing */
  UWORD     flags;             /* 16/8 bits looping/one-shot */
  SWORD     handle;            /* identifies the sample */
  ULONG     start;             /* start index */
  ULONG     size;              /* samplesize */
  ULONG     reppos;            /* loop start */
  ULONG     repend;            /* loop end */
  ULONG     frq;               /* current frequency */
  int       vol;               /* current volume */
  int       pan;               /* current panning position */

  int       click;
  int       rampvol;
  SLONG     lastvalL,lastvalR;
  int       lvolsel,rvolsel;   /* Volume factor in range 0-255 */
  int       oldlvol,oldrvol;

  SLONGLONG current;           /* current index in the sample */
  SLONGLONG increment;         /* increment value */
} VINFO;

static  SWORD **Samples;
static  VINFO *vinf=NULL,*vnf;
static  long tickleft,samplesthatfit,vc_memory=0;
static  int vc_softchn;
static  SLONGLONG idxsize,idxlpos,idxlend;
static  SLONG *vc_tickbuf=NULL;
static  UWORD vc_mode;

#ifdef _WIN32
// Weird bug in compiler
typedef void (*MikMod_callback_t)(unsigned char *data, size_t len);
#endif

/* Reverb control variables */

static  int RVc1, RVc2, RVc3, RVc4, RVc5, RVc6, RVc7, RVc8;
static  ULONG RVRindex;

/* For Mono or Left Channel */
static  SLONG *RVbufL1=NULL,*RVbufL2=NULL,*RVbufL3=NULL,*RVbufL4=NULL,
          *RVbufL5=NULL,*RVbufL6=NULL,*RVbufL7=NULL,*RVbufL8=NULL;

/* For Stereo only (Right Channel) */
static  SLONG *RVbufR1=NULL,*RVbufR2=NULL,*RVbufR3=NULL,*RVbufR4=NULL,
          *RVbufR5=NULL,*RVbufR6=NULL,*RVbufR7=NULL,*RVbufR8=NULL;

#ifdef NATIVE_64BIT_INT
#define NATIVE SLONGLONG
#else
#define NATIVE SLONG
#endif

/*========== 32 bit sample mixers - only for 32 bit platforms */
#ifndef NATIVE_64BIT_INT

static SLONG Mix32MonoNormal(const SWORD* const srce,SLONG* dest,SLONG index,SLONG increment,SLONG todo)
{
  SWORD sample=0;
  SLONG i,f;

  while(todo--) {
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)( (((SLONG)(srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONG)srce[i+1]*f)) >> FRACBITS_VC2));
    index+=increment;

    if(vnf->rampvol) {
      *dest++ += (long)(
        ( ( (SLONG)(vnf->oldlvol*vnf->rampvol) +
            (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol)) ) *
          (SLONG)sample ) >> CLICK_SHIFT_VC2 );
      vnf->rampvol--;
    } else
      if(vnf->click) {
      *dest++ += (long)(
        ( ( ((SLONG)vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONG)sample ) +
          (vnf->lastvalL*vnf->click) ) >> CLICK_SHIFT_VC2 );
      vnf->click--;
    } else
      *dest++ +=vnf->lvolsel*sample;
  }
  vnf->lastvalL=vnf->lvolsel * sample;

  return index;
}

static SLONG Mix32StereoNormal(const SWORD* const srce,SLONG* dest,SLONG index,SLONG increment,ULONG todo)
{
  SWORD sample=0;
  SLONG i,f;

  while(todo--) {
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)(((((SLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONG)srce[i+1] * f)) >> FRACBITS_VC2));
    index += increment;

    if(vnf->rampvol) {
      *dest++ += (long)(
        ( ( ((SLONG)vnf->oldlvol*vnf->rampvol) +
            (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol))
          ) * (SLONG)sample ) >> CLICK_SHIFT_VC2 );
      *dest++ += (long)(
        ( ( ((SLONG)vnf->oldrvol*vnf->rampvol) +
            (vnf->rvolsel*(CLICK_BUFFER_VC2-vnf->rampvol))
          ) * (SLONG)sample ) >> CLICK_SHIFT_VC2 );
      vnf->rampvol--;
    } else
      if(vnf->click) {
      *dest++ += (long)(
        ( ( (SLONG)(vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONG)sample ) + (vnf->lastvalL * vnf->click) )
          >> CLICK_SHIFT_VC2 );
      *dest++ += (long)(
        ( ( ((SLONG)vnf->rvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONG)sample ) + (vnf->lastvalR * vnf->click) )
          >> CLICK_SHIFT_VC2 );
      vnf->click--;
    } else {
      *dest++ +=vnf->lvolsel*sample;
      *dest++ +=vnf->rvolsel*sample;
    }
  }
  vnf->lastvalL=vnf->lvolsel*sample;
  vnf->lastvalR=vnf->rvolsel*sample;

  return index;
}

static SLONG Mix32StereoSurround(const SWORD* const srce,SLONG* dest,SLONG index,SLONG increment,ULONG todo)
{
  SWORD sample=0;
  long whoop;
  SLONG i, f;

  while(todo--) {
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)(((((SLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONG)srce[i+1]*f)) >> FRACBITS_VC2));
    index+=increment;

    if(vnf->rampvol) {
      whoop=(long)(
        ( ( (SLONG)(vnf->oldlvol*vnf->rampvol) +
            (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol)) ) *
          (SLONG)sample) >> CLICK_SHIFT_VC2 );
      *dest++ +=whoop;
      *dest++ -=whoop;
      vnf->rampvol--;
    } else
      if(vnf->click) {
      whoop = (long)(
        ( ( ((SLONG)vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONG)sample) +
          (vnf->lastvalL * vnf->click) ) >> CLICK_SHIFT_VC2 );
      *dest++ +=whoop;
      *dest++ -=whoop;
      vnf->click--;
    } else {
      *dest++ +=vnf->lvolsel*sample;
      *dest++ -=vnf->lvolsel*sample;
    }
  }
  vnf->lastvalL=vnf->lvolsel*sample;
  vnf->lastvalR=vnf->lvolsel*sample;

  return index;
}
#endif

/*========== 64 bit mixers */

static SLONGLONG MixMonoNormal(const SWORD* const srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,SLONG todo)
{
  SWORD sample=0;
  SLONGLONG i,f;

  while(todo--) {
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)((((SLONGLONG)(srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONGLONG)srce[i+1]*f)) >> FRACBITS_VC2));
    index+=increment;

    if(vnf->rampvol) {
      *dest++ += (long)(
        ( ( (SLONGLONG)(vnf->oldlvol*vnf->rampvol) +
            (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol)) ) *
          (SLONGLONG)sample ) >> CLICK_SHIFT_VC2 );
      vnf->rampvol--;
    } else
      if(vnf->click) {
      *dest++ += (long)(
        ( ( ((SLONGLONG)vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONGLONG)sample ) +
          (vnf->lastvalL*vnf->click) ) >> CLICK_SHIFT_VC2 );
      vnf->click--;
    } else
      *dest++ +=vnf->lvolsel*sample;
  }
  vnf->lastvalL=vnf->lvolsel * sample;

  return index;
}

// Slowest part...

#if defined HAVE_SSE2 || defined HAVE_ALTIVEC

static __inline SWORD GetSample(const SWORD* const srce, SLONGLONG index)
{
  SLONGLONG i=index>>FRACBITS_VC2;
  SLONGLONG f=index&FRACMASK_VC2;
  return (SWORD)(((((SLONGLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
               ((SLONGLONG)srce[i+1] * f)) >> FRACBITS_VC2));
}

static SLONGLONG MixSIMDStereoNormal(const SWORD* const srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,ULONG todo)
{
  SWORD vol[8] = {vnf->lvolsel, vnf->rvolsel};
  SWORD s[8];
  SWORD sample=0;
  SLONG remain = todo;

  // Dest can be misaligned ...
  while(!IS_ALIGNED_16(dest)) {
    sample=srce[(index += increment) >> FRACBITS_VC2];
    *dest++ += vol[0] * sample;
    *dest++ += vol[1] * sample;
    todo--;
  }

  // Srce is always aligned ...

#if defined HAVE_SSE2
  remain = todo&3;
  {
    __m128i v0 = _mm_set_epi16(0, vol[1],
                   0, vol[0],
                   0, vol[1],
                   0, vol[0]);
    for(todo>>=2;todo; todo--)
    {
      SWORD s0 = GetSample(srce, index += increment);
      SWORD s1 = GetSample(srce, index += increment);
      SWORD s2 = GetSample(srce, index += increment);
      SWORD s3 = GetSample(srce, index += increment);
      __m128i v1 = _mm_set_epi16(0, s1, 0, s1, 0, s0, 0, s0);
      __m128i v2 = _mm_set_epi16(0, s3, 0, s3, 0, s2, 0, s2);
      __m128i v3 = _mm_load_si128((__m128i*)(dest+0));
      __m128i v4 = _mm_load_si128((__m128i*)(dest+4));
      _mm_store_si128((__m128i*)(dest+0), _mm_add_epi32(v3, _mm_madd_epi16(v0, v1)));
      _mm_store_si128((__m128i*)(dest+4), _mm_add_epi32(v4, _mm_madd_epi16(v0, v2)));
      dest+=8;
    }
  }

#elif defined HAVE_ALTIVEC
  remain = todo&3;
    remain = todo&3;
  {
    vector signed short r0 = vec_ld(0, vol);
    vector signed short v0 = vec_perm(r0, r0, (vector unsigned char)(0, 1, // l
                                     0, 1, // l
                                     2, 3, // r
                                     2, 1, // r
                                     0, 1, // l
                                     0, 1, // l
                                     2, 3, // r
                                     2, 3 // r
                                     ));


    for(todo>>=2;todo; todo--)
    {
      // Load constants
      s[0] = GetSample(srce, index += increment);
      s[1] = GetSample(srce, index += increment);
      s[2] = GetSample(srce, index += increment);
      s[3] = GetSample(srce, index += increment);
      s[4] = 0;

      vector short int r1 = vec_ld(0, s);
      vector signed short v1 = vec_perm(r1, r1, (vector unsigned char)(0*2, 0*2+1, // s0
                                       4*2, 4*2+1, // 0
                                       0*2, 0*2+1, // s0
                                       4*2, 4*2+1, // 0
                                       1*2, 1*2+1, // s1
                                       4*2, 4*2+1, // 0
                                       1*2, 1*2+1, // s1
                                       4*2, 4*2+1  // 0
                                       ));

      vector signed short v2 = vec_perm(r1, r1, (vector unsigned char)(2*2, 2*2+1, // s2
                                       4*2, 4*2+1, // 0
                                       2*2, 2*2+1, // s2
                                       4*2, 4*2+1, // 0
                                       3*2, 3*2+1, // s3
                                       4*2, 4*2+1, // 0
                                       3*2, 3*2+1, // s3
                                       4*2, 4*2+1  // 0
                                       ));
      vector signed int v3 = vec_ld(0, dest);
      vector signed int v4 = vec_ld(0, dest + 4);
      vector signed int v5 = vec_mule(v0, v1);
      vector signed int v6 = vec_mule(v0, v2);

      vec_st(vec_add(v3, v5), 0, dest);
      vec_st(vec_add(v4, v6), 0, dest + 4);

      dest+=8;
    }
  }
  #endif // HAVE_ALTIVEC

  // Remaining bits ...
  while(remain--) {
    sample=GetSample(srce, index+= increment);

    *dest++ += vol[0] * sample;
    *dest++ += vol[1] * sample;
  }


  vnf->lastvalL=vnf->lvolsel*sample;
  vnf->lastvalR=vnf->rvolsel*sample;
  return index;
}

#endif


static SLONGLONG MixStereoNormal(const SWORD* const srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,ULONG todo)
{
  SWORD sample=0;
  SLONGLONG i,f;

  if (vnf->rampvol)
  while(todo) {
    todo--;
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)(((((SLONGLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONGLONG)srce[i+1] * f)) >> FRACBITS_VC2));
    index += increment;


    *dest++ += (long)(
      ( ( ((SLONGLONG)vnf->oldlvol*vnf->rampvol) +
          (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol))
      ) * (SLONGLONG)sample ) >> CLICK_SHIFT_VC2 );
    *dest++ += (long)(
      ( ( ((SLONGLONG)vnf->oldrvol*vnf->rampvol) +
          (vnf->rvolsel*(CLICK_BUFFER_VC2-vnf->rampvol))
      ) * (SLONGLONG)sample ) >> CLICK_SHIFT_VC2 );
    vnf->rampvol--;

    if (!vnf->rampvol)
      break;
  }

  if (vnf->click)
  while(todo) {
    todo--;
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)(((((SLONGLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONGLONG)srce[i+1] * f)) >> FRACBITS_VC2));
    index += increment;

    *dest++ += (long)(
        ( ( (SLONGLONG)(vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONGLONG)sample ) + (vnf->lastvalL * vnf->click) )
          >> CLICK_SHIFT_VC2 );

    *dest++ += (long)(
        ( ( ((SLONGLONG)vnf->rvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONGLONG)sample ) + (vnf->lastvalR * vnf->click) )
          >> CLICK_SHIFT_VC2 );
      vnf->click--;

    if (!vnf->click)
      break;
  }


  if (todo)
  {
#if defined HAVE_SSE2 || defined HAVE_ALTIVEC
    if (md_mode & DMODE_SIMDMIXER)
    {
      index = MixSIMDStereoNormal(srce, dest, index, increment, todo);
    }
    else
#endif
    {

      while(todo)
      {
        i=index>>FRACBITS_VC2,
        f=index&FRACMASK_VC2;
        sample=(SWORD)(((((SLONGLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
                ((SLONGLONG)srce[i+1] * f)) >> FRACBITS_VC2));
        index += increment;

        *dest++ +=vnf->lvolsel*sample;
        *dest++ +=vnf->rvolsel*sample;
        todo--;
      }
    }
  }
  return index;
}

static SLONGLONG MixStereoSurround(SWORD* srce,SLONG* dest,SLONGLONG index,SLONGLONG increment,ULONG todo)
{
  SWORD sample=0;
  long whoop;
  SLONGLONG i, f;

  while(todo--) {
    i=index>>FRACBITS_VC2,f=index&FRACMASK_VC2;
    sample=(SWORD)(((((SLONGLONG)srce[i]*(FRACMASK_VC2+1L-f)) +
            ((SLONGLONG)srce[i+1]*f)) >> FRACBITS_VC2));
    index+=increment;

    if(vnf->rampvol) {
      whoop=(long)(
        ( ( (SLONGLONG)(vnf->oldlvol*vnf->rampvol) +
            (vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->rampvol)) ) *
          (SLONGLONG)sample) >> CLICK_SHIFT_VC2 );
      *dest++ +=whoop;
      *dest++ -=whoop;
      vnf->rampvol--;
    } else
      if(vnf->click) {
      whoop = (long)(
        ( ( ((SLONGLONG)vnf->lvolsel*(CLICK_BUFFER_VC2-vnf->click)) *
            (SLONGLONG)sample) +
          (vnf->lastvalL * vnf->click) ) >> CLICK_SHIFT_VC2 );
      *dest++ +=whoop;
      *dest++ -=whoop;
      vnf->click--;
    } else {
      *dest++ +=vnf->lvolsel*sample;
      *dest++ -=vnf->lvolsel*sample;
    }
  }
  vnf->lastvalL=vnf->lvolsel*sample;
  vnf->lastvalR=vnf->lvolsel*sample;

  return index;
}

static  void(*Mix32toFP)(float* dste,const SLONG *srce,NATIVE count);
static  void(*Mix32to16)(SWORD* dste,const SLONG *srce,NATIVE count);
static  void(*Mix32to8)(SBYTE* dste,const SLONG *srce,NATIVE count);
static  void(*MixReverb)(SLONG *srce,NATIVE count);

/* Reverb macros */
#define COMPUTE_LOC(n) loc##n = RVRindex % RVc##n
#define COMPUTE_LECHO(n) RVbufL##n [loc##n ]=speedup+((ReverbPct*RVbufL##n [loc##n ])>>7)
#define COMPUTE_RECHO(n) RVbufR##n [loc##n ]=speedup+((ReverbPct*RVbufR##n [loc##n ])>>7)

static void MixReverb_Normal(SLONG *srce,NATIVE count)
{
  NATIVE speedup;
  int ReverbPct;
  unsigned int loc1,loc2,loc3,loc4,loc5,loc6,loc7,loc8;

  ReverbPct=58+(md_reverb*4);

  COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
  COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

  while(count--) {
    /* Compute the left channel echo buffers */
    speedup = *srce >> 3;

    COMPUTE_LECHO(1); COMPUTE_LECHO(2); COMPUTE_LECHO(3); COMPUTE_LECHO(4);
    COMPUTE_LECHO(5); COMPUTE_LECHO(6); COMPUTE_LECHO(7); COMPUTE_LECHO(8);

    /* Prepare to compute actual finalized data */
    RVRindex++;

    COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
    COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

    /* left channel */
    *srce++ +=RVbufL1[loc1]-RVbufL2[loc2]+RVbufL3[loc3]-RVbufL4[loc4]+
              RVbufL5[loc5]-RVbufL6[loc6]+RVbufL7[loc7]-RVbufL8[loc8];
  }
}

static void MixReverb_Stereo(SLONG *srce,NATIVE count)
{
  NATIVE speedup;
  int ReverbPct;
  unsigned int loc1,loc2,loc3,loc4,loc5,loc6,loc7,loc8;

  ReverbPct=58+(md_reverb*4);

  COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
  COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

  while(count--) {
    /* Compute the left channel echo buffers */
    speedup = *srce >> 3;

    COMPUTE_LECHO(1); COMPUTE_LECHO(2); COMPUTE_LECHO(3); COMPUTE_LECHO(4);
    COMPUTE_LECHO(5); COMPUTE_LECHO(6); COMPUTE_LECHO(7); COMPUTE_LECHO(8);

    /* Compute the right channel echo buffers */
    speedup = srce[1] >> 3;

    COMPUTE_RECHO(1); COMPUTE_RECHO(2); COMPUTE_RECHO(3); COMPUTE_RECHO(4);
    COMPUTE_RECHO(5); COMPUTE_RECHO(6); COMPUTE_RECHO(7); COMPUTE_RECHO(8);

    /* Prepare to compute actual finalized data */
    RVRindex++;

    COMPUTE_LOC(1); COMPUTE_LOC(2); COMPUTE_LOC(3); COMPUTE_LOC(4);
    COMPUTE_LOC(5); COMPUTE_LOC(6); COMPUTE_LOC(7); COMPUTE_LOC(8);

    /* left channel */
    *srce++ +=RVbufL1[loc1]-RVbufL2[loc2]+RVbufL3[loc3]-RVbufL4[loc4]+
              RVbufL5[loc5]-RVbufL6[loc6]+RVbufL7[loc7]-RVbufL8[loc8];

    /* right channel */
    *srce++ +=RVbufR1[loc1]-RVbufR2[loc2]+RVbufR3[loc3]-RVbufR4[loc4]+
              RVbufR5[loc5]-RVbufR6[loc6]+RVbufR7[loc7]-RVbufR8[loc8];
  }
}

static void (*MixLowPass)(SLONG* srce,NATIVE count);

static int nLeftNR, nRightNR;

static void MixLowPass_Stereo(SLONG* srce,NATIVE count)
{
  int n1 = nLeftNR, n2 = nRightNR;
  SLONG *pnr = srce;
  int nr=count;
  for (; nr; nr--)
  {
    int vnr = pnr[0] >> 1;
    pnr[0] = vnr + n1;
    n1 = vnr;
    vnr = pnr[1] >> 1;
    pnr[1] = vnr + n2;
    n2 = vnr;
    pnr += 2;
  }
  nLeftNR = n1;
  nRightNR = n2;
}

static void MixLowPass_Normal(SLONG* srce,NATIVE count)
{
  int n1 = nLeftNR;
  SLONG *pnr = srce;
  int nr=count;
  for (; nr; nr--)
  {
    int vnr = pnr[0] >> 1;
    pnr[0] = vnr + n1;
    n1 = vnr;
    pnr ++;
  }
  nLeftNR = n1;
}

/* Mixing macros */
#define EXTRACT_SAMPLE_VC2_FP_VC2(var,attenuation) var=*srce++*((1.0f / 32768.0f) / (MAXVOL_FACTOR*attenuation))
#define CHECK_SAMPLE_FP(var,bound) var=(var>bound)?bound:(var<-bound)?-bound:var

static void Mix32ToFP_Normal(float* dste,const SLONG *srce,NATIVE count)
{
  float x1,x2,tmpx;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx=0.0f;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2_FP_VC2(x1,1.0f); EXTRACT_SAMPLE_VC2_FP_VC2(x2,1.0f);

      CHECK_SAMPLE_FP(x1,1.0f); CHECK_SAMPLE_FP(x2,1.0f);

      tmpx+=x1+x2;
    }
    *dste++ =tmpx*(1.0f/SAMPLING_FACTOR);
  }
}

static void Mix32ToFP_Stereo(float* dste,const SLONG *srce,NATIVE count)
{
  float x1,x2,x3,x4,tmpx,tmpy;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx=tmpy=0.0f;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2_FP_VC2(x1,1.0f); EXTRACT_SAMPLE_VC2_FP_VC2(x2,1.0f);
      EXTRACT_SAMPLE_VC2_FP_VC2(x3,1.0f); EXTRACT_SAMPLE_VC2_FP_VC2(x4,1.0f);

      CHECK_SAMPLE_FP(x1,1.0f); CHECK_SAMPLE_FP(x2,1.0f);
      CHECK_SAMPLE_FP(x3,1.0f); CHECK_SAMPLE_FP(x4,1.0f);

      tmpx+=x1+x3;
      tmpy+=x2+x4;
    }
    *dste++ =tmpx*(1.0f/SAMPLING_FACTOR);
    *dste++ =tmpy*(1.0f/SAMPLING_FACTOR);
  }
}

/* Mixing macros */
#define EXTRACT_SAMPLE_VC2(var,attenuation) var=*srce++/(MAXVOL_FACTOR*attenuation)
#define CHECK_SAMPLE(var,bound) var=(var>=bound)?bound-1:(var<-bound)?-bound:var

static void Mix32To16_Normal(SWORD* dste,const SLONG *srce,NATIVE count)
{
  NATIVE x1,x2,tmpx;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx=0;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2(x1,1); EXTRACT_SAMPLE_VC2(x2,1);

      CHECK_SAMPLE(x1,32768); CHECK_SAMPLE(x2,32768);

      tmpx+=x1+x2;
    }
    *dste++ =(SWORD)(tmpx/SAMPLING_FACTOR);
  }
}


static void Mix32To16_Stereo(SWORD* dste,const SLONG *srce,NATIVE count)
{
  NATIVE x1,x2,x3,x4,tmpx,tmpy;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx=tmpy=0;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2(x1,1); EXTRACT_SAMPLE_VC2(x2,1);
      EXTRACT_SAMPLE_VC2(x3,1); EXTRACT_SAMPLE_VC2(x4,1);

      CHECK_SAMPLE(x1,32768); CHECK_SAMPLE(x2,32768);
      CHECK_SAMPLE(x3,32768); CHECK_SAMPLE(x4,32768);

      tmpx+=x1+x3;
      tmpy+=x2+x4;
    }
    *dste++ =(SWORD)(tmpx/SAMPLING_FACTOR);
    *dste++ =(SWORD)(tmpy/SAMPLING_FACTOR);
  }
}

static void Mix32To8_Normal(SBYTE* dste,const SLONG *srce,NATIVE count)
{
  NATIVE x1,x2,tmpx;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx = 0;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2(x1,256); EXTRACT_SAMPLE_VC2(x2,256);

      CHECK_SAMPLE(x1,128); CHECK_SAMPLE(x2,128);

      tmpx+=x1+x2;
    }
    *dste++ = (SBYTE)((tmpx/SAMPLING_FACTOR)+128);
  }
}

static void Mix32To8_Stereo(SBYTE* dste,const SLONG *srce,NATIVE count)
{
  NATIVE x1,x2,x3,x4,tmpx,tmpy;
  int i;

  for(count/=SAMPLING_FACTOR;count;count--) {
    tmpx=tmpy=0;

    for(i=SAMPLING_FACTOR/2;i;i--) {
      EXTRACT_SAMPLE_VC2(x1,256); EXTRACT_SAMPLE_VC2(x2,256);
      EXTRACT_SAMPLE_VC2(x3,256); EXTRACT_SAMPLE_VC2(x4,256);

      CHECK_SAMPLE(x1,128); CHECK_SAMPLE(x2,128);
      CHECK_SAMPLE(x3,128); CHECK_SAMPLE(x4,128);

      tmpx+=x1+x3;
      tmpy+=x2+x4;
    }
    *dste++ =(SBYTE)((tmpx/SAMPLING_FACTOR)+128);
    *dste++ =(SBYTE)((tmpy/SAMPLING_FACTOR)+128);
  }
}

#if defined HAVE_SSE2
#define SHIFT_MIX_TO_16 (BITSHIFT + 16 - 16)
// TEST: Ok
static void Mix32To16_Stereo_SIMD_4Tap(SWORD* dste, SLONG* srce, NATIVE count)
{
  int remain = count;

  // Check unaligned dste buffer. srce is always aligned.
  while(!IS_ALIGNED_16(dste))
  {
    Mix32To16_Stereo(dste, srce, SAMPLING_FACTOR);
    dste+=2;
    srce+=8;
    count--;
  }

  // dste and srce aligned. srce is always aligned.
  {
    remain = count & 15;
    // count / 2 for 1 sample

    for(count>>=4;count;count--)
    {
         // Load 32bit sample. 1st average
    __m128i v0 = _mm_add_epi32(
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+0)), SHIFT_MIX_TO_16),
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+4)), SHIFT_MIX_TO_16)
    );
    // v0: s0.l+s2.l | s0.r+s2.r | s1.l+s3.l | s1.r+s3.r


    // 2nd average (s0.l+s2.l+s1.l+s3.l / 4, s0.r+s2.r+s1.r+s3.r / 4). Upper 64bit is unused  (1 stereo sample)
        __m128i v1 = _mm_srai_epi32(_mm_add_epi32(v0, mm_hiqq(v0)), 2);
    // v1: s0.l+s2.l / 4 | s0.r+s2.r / 4 | s1.l+s3.l+s0.l+s2.l / 4 | s1.r+s3.r+s0.r+s2.r / 4


    __m128i v2 = _mm_add_epi32(
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+8)), SHIFT_MIX_TO_16),
      _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+12)), SHIFT_MIX_TO_16)
    );
    // v2: s4.l+s6.l | s4.r+s6.r | s5.l+s7.l | s5.r+s7.r

        __m128i v3 = _mm_srai_epi32(_mm_add_epi32(v2, mm_hiqq(v2)), 2) ;  //Upper 64bit is unused
    // v3: s4.l+s6.l /4 | s4.r+s6.r / 4| s5.l+s7.l+s4.l+s6.l / 4 | s5.r+s7.r+s4.r+s6.l / 4

      // pack two stereo samples in one
        __m128i v4 = _mm_unpacklo_epi64(v1, v3); //   v4 = avg(s0,s1,s2,s3) | avg(s4,s5,s6,s7)

    __m128i v6;

        // Load 32bit sample. 1st average (s0.l+s2.l, s0.r+s2.r, s1.l+s3.l, s1.r+s3.r)
    v0 = _mm_add_epi32(
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+16)), SHIFT_MIX_TO_16),
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+20)), SHIFT_MIX_TO_16)
    );  //128bit = 2 stereo samples

    // 2nd average (s0.l+s2.l+s1.l+s3.l / 4, s0.r+s2.r+s1.r+s3.r / 4). Upper 64bit is unused  (1 stereo sample)
        v1 = _mm_srai_epi32(_mm_add_epi32(v0, mm_hiqq(v0)), 2);

    v2 = _mm_add_epi32(
    _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+24)), SHIFT_MIX_TO_16),
      _mm_srai_epi32(_mm_loadu_si128((__m128i*)(srce+28)), SHIFT_MIX_TO_16)
    );

        v3 = _mm_srai_epi32(_mm_add_epi32(v2, mm_hiqq(v2)), 2);  //Upper 64bit is unused

        // pack two stereo samples in one
        v6 = _mm_unpacklo_epi64(v1, v3); //    v6 = avg(s8,s9,s10,s11) | avg(s12,s13,s14,s15)

    _mm_store_si128((__m128i*)dste, _mm_packs_epi32(v4, v6));  // 4 interpolated stereo sample 32bit to 4

    dste+=8;
    srce+=32; // 32 = 4 * 8
    }
  }

  if (remain)
  {
    Mix32To16_Stereo(dste, srce, remain);
  }
}

#elif defined HAVE_ALTIVEC
#define SHIFT_MIX_TO_16 vec_splat_u32(BITSHIFT + 16 - 16)
// TEST: Ok
static void Mix32To16_Stereo_SIMD_4Tap(SWORD* dste, SLONG* srce, NATIVE count)
{
  int remain = count;

  // Check unaligned dste buffer. srce is always aligned.
  while(!IS_ALIGNED_16(dste))
  {
    Mix32To16_Stereo(dste, srce, SAMPLING_FACTOR);
    dste+=2;
    srce+=8;
    count--;
  }

  // dste and srce aligned. srce is always aligned.
  {
    remain = count & 15;
    for(count>>=4;count;count--)
    {
      // Load 32bit sample. 1st average (s0.l+s2.l, s0.r+s2.r, s1.l+s3.l, s1.r+s3.r)
      vector signed int v0 = vec_add(
        vec_sra(vec_ld(0, srce+0), SHIFT_MIX_TO_16),  //128bit = 2 stereo samples
        vec_sra(vec_ld(0, srce+4), SHIFT_MIX_TO_16)
      );  //128bit = 2 stereo samples

      // 2nd average (s0.l+s2.l+s1.l+s3.l / 4, s0.r+s2.r+s1.r+s3.r / 4). Upper 64bit is unused  (1 stereo sample)
      vector signed int v1 = vec_sra(vec_add(v0, vec_hiqq(v0)), vec_splat_u32(2));

      vector signed int v2 = vec_add(
        vec_sra(vec_ld(0, srce+8), SHIFT_MIX_TO_16),
          vec_sra(vec_ld(0, srce+12), SHIFT_MIX_TO_16)
      );

      vector signed int v3 = vec_sra(vec_add(v2, vec_hiqq(v2)), vec_splat_u32(2));  //Upper 64bit is unused

      // pack two stereo samples in one
      vector signed int v6, v4 = vec_unpacklo(v1, v3); //   v4 = lo64(v1) | lo64(v3)

      // Load 32bit sample. 1st average (s0.l+s2.l, s0.r+s2.r, s1.l+s3.l, s1.r+s3.r)
      v0 = vec_add(
        vec_sra(vec_ld(0, srce+16), SHIFT_MIX_TO_16), //128bit = 2 stereo samples
        vec_sra(vec_ld(0, srce+20), SHIFT_MIX_TO_16)
      );  //128bit = 2 stereo samples

      // 2nd average (s0.l+s2.l+s1.l+s3.l / 4, s0.r+s2.r+s1.r+s3.r / 4). Upper 64bit is unused  (1 stereo sample)
      v1 = vec_sra(vec_add(v0, vec_hiqq(v0)), vec_splat_u32(2));

      v2 = vec_add(
        vec_sra(vec_ld(0, srce+24), SHIFT_MIX_TO_16),
        vec_sra(vec_ld(0, srce+28), SHIFT_MIX_TO_16))
      ;

      v3 = vec_sra(vec_add(v2, vec_hiqq(v2)), vec_splat_u32(2));  //Upper 64bit is unused

      // pack two stereo samples in one
      v6 = vec_unpacklo(v1, v3); //

      vec_st(vec_packs(v4, v6), 0, dste);  // 4 interpolated stereo sample 32bit to 4 interpolated stereo sample 16bit + saturation

      dste+=8;
      srce+=32; // 32 = 4 * 8
    }
  }

  if (remain)
  {
    Mix32To16_Stereo(dste, srce, remain);
  }
}

#endif



static void AddChannel(SLONG* ptr,NATIVE todo)
{
  SLONGLONG end,done;
  SWORD *s;

  if(!(s=Samples[vnf->handle])) {
    vnf->current = vnf->active  = 0;
    vnf->lastvalL = vnf->lastvalR = 0;
    return;
  }

  /* update the 'current' index so the sample loops, or stops playing if it
     reached the end of the sample */
  while(todo>0) {
    SLONGLONG endpos;

    if(vnf->flags & SF_REVERSE) {
      /* The sample is playing in reverse */
      if((vnf->flags&SF_LOOP)&&(vnf->current<idxlpos)) {
        /* the sample is looping and has reached the loopstart index */
        if(vnf->flags & SF_BIDI) {
          /* sample is doing bidirectional loops, so 'bounce' the
             current index against the idxlpos */
          vnf->current = idxlpos+(idxlpos-vnf->current);
          vnf->flags &= ~SF_REVERSE;
          vnf->increment = -vnf->increment;
        } else
          /* normal backwards looping, so set the current position to
             loopend index */
          vnf->current=idxlend-(idxlpos-vnf->current);
      } else {
        /* the sample is not looping, so check if it reached index 0 */
        if(vnf->current < 0) {
          /* playing index reached 0, so stop playing this sample */
          vnf->current = vnf->active  = 0;
          break;
        }
      }
    } else {
      /* The sample is playing forward */
      if((vnf->flags & SF_LOOP) &&
         (vnf->current >= idxlend)) {
        /* the sample is looping, check the loopend index */
        if(vnf->flags & SF_BIDI) {
          /* sample is doing bidirectional loops, so 'bounce' the
             current index against the idxlend */
          vnf->flags |= SF_REVERSE;
          vnf->increment = -vnf->increment;
          vnf->current = idxlend-(vnf->current-idxlend);
        } else
          /* normal backwards looping, so set the current position
             to loopend index */
          vnf->current=idxlpos+(vnf->current-idxlend);
      } else {
        /* sample is not looping, so check if it reached the last
           position */
        if(vnf->current >= idxsize) {
          /* yes, so stop playing this sample */
          vnf->current = vnf->active  = 0;
          break;
        }
      }
    }

    end=(vnf->flags&SF_REVERSE)?(vnf->flags&SF_LOOP)?idxlpos:0:
         (vnf->flags&SF_LOOP)?idxlend:idxsize;

    /* if the sample is not blocked... */
    if((end==vnf->current)||(!vnf->increment))
      done=0;
    else {
      done=MIN((end-vnf->current)/vnf->increment+1,todo);
      if(done<0) done=0;
    }

    if(!done) {
      vnf->active = 0;
      break;
    }

    endpos=vnf->current+done*vnf->increment;

    if(vnf->vol || vnf->rampvol) {
#ifndef NATIVE_64BIT_INT
      /* use the 32 bit mixers as often as we can (they're much faster) */
      if((vnf->current<0x7fffffff)&&(endpos<0x7fffffff)) {
        if(vc_mode & DMODE_STEREO) {
          if((vnf->pan==PAN_SURROUND)&&(vc_mode&DMODE_SURROUND))
            vnf->current=(SLONGLONG)Mix32StereoSurround
                           (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
          else
            vnf->current=Mix32StereoNormal
                           (s,ptr,(SLONG)vnf->current, (SLONG)vnf->increment,(SLONG)done);
        } else
          vnf->current=Mix32MonoNormal
                             (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
      } else
#endif
             {
        if(vc_mode & DMODE_STEREO) {
          if((vnf->pan==PAN_SURROUND)&&(vc_mode&DMODE_SURROUND))
            vnf->current=MixStereoSurround
                           (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
          else
            vnf->current=MixStereoNormal
                           (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
        } else
          vnf->current=MixMonoNormal
                             (s,ptr,(SLONG)vnf->current,(SLONG)vnf->increment,(SLONG)done);
      }
    } else  {
      vnf->lastvalL = vnf->lastvalR = 0;
      /* update sample position */
      vnf->current=endpos;
    }

    todo -= (SLONG)done;
    ptr += (SLONG)(vc_mode & DMODE_STEREO)?(done<<1):done;
  }
}

#define VC2_Exit NULL


void VC2_WriteSamples(SBYTE* buf,ULONG todo)
{
  int left,portion=0;
  SBYTE *buffer;
  int t,pan,vol;

  todo*=SAMPLING_FACTOR;

  while(todo) {
    if(!tickleft) {
      if(vc_mode & DMODE_SOFT_MUSIC) md_player();
      tickleft=(md_mixfreq*125L*SAMPLING_FACTOR)/(md_bpm*50L);
      tickleft&=~(SAMPLING_FACTOR-1);
    }
    left = MIN(tickleft, (int)todo);
    buffer    = buf;
    tickleft -= left;
    todo     -= left;
    buf += samples2bytes(left)/SAMPLING_FACTOR;

    while(left) {
      portion = MIN(left, samplesthatfit);
      memset(vc_tickbuf,0,portion<<((vc_mode&DMODE_STEREO)?3:2));
      for(t=0;t<vc_softchn;t++) {
        vnf = &vinf[t];

        if(vnf->kick) {
          vnf->current=((SLONGLONG)(vnf->start))<<FRACBITS_VC2;
          vnf->kick    = 0;
          vnf->active  = 1;
          vnf->click   = CLICK_BUFFER_VC2;
          vnf->rampvol = 0;
        }

        if(!vnf->frq) vnf->active = 0;

        if(vnf->active) {
          vnf->increment=((SLONGLONG)(vnf->frq)<<(FRACBITS_VC2-SAMPLING_SHIFT))
                         /md_mixfreq;
          if(vnf->flags&SF_REVERSE) vnf->increment=-vnf->increment;
          vol = vnf->vol;  pan = vnf->pan;

          vnf->oldlvol=vnf->lvolsel;vnf->oldrvol=vnf->rvolsel;
          if(vc_mode & DMODE_STEREO) {
            if(pan!=PAN_SURROUND) {
              vnf->lvolsel=(vol*(PAN_RIGHT-pan))>>8;
              vnf->rvolsel=(vol*pan)>>8;
            } else {
              vnf->lvolsel=vnf->rvolsel=(vol * 256L) / 480;
            }
          } else
            vnf->lvolsel=vol;

          idxsize=(vnf->size)?((SLONGLONG)(vnf->size)<<FRACBITS_VC2)-1:0;
          idxlend=(vnf->repend)?((SLONGLONG)(vnf->repend)<<FRACBITS_VC2)-1:0;
          idxlpos=(SLONGLONG)(vnf->reppos)<<FRACBITS_VC2;
          AddChannel(vc_tickbuf,portion);
        }
      }

      if(md_mode & DMODE_NOISEREDUCTION) {
        MixLowPass(vc_tickbuf, portion);
      }

      if(md_reverb) {
        if(md_reverb>15) md_reverb=15;
        MixReverb(vc_tickbuf,portion);
      }

      if (vc_callback) {
        vc_callback((unsigned char*)vc_tickbuf, portion);
      }

      if(vc_mode & DMODE_FLOAT)
        Mix32toFP((float*)buffer,vc_tickbuf,portion);
      else if(vc_mode & DMODE_16BITS)
        Mix32to16((SWORD*)buffer,vc_tickbuf,portion);
      else
        Mix32to8((SBYTE*)buffer,vc_tickbuf,portion);

      buffer += samples2bytes(portion) / SAMPLING_FACTOR;
      left   -= portion;
    }
  }
}

BOOL VC2_Init(void)
{
  VC_SetupPointers();

  if (!(md_mode&DMODE_HQMIXER))
    return VC1_Init();

  if(!(Samples=(SWORD**)MikMod_calloc(MAXSAMPLEHANDLES,sizeof(SWORD*)))) {
    _mm_errno = MMERR_INITIALIZING_MIXER;
    return 1;
  }
  if(!vc_tickbuf)
    if(!(vc_tickbuf=(SLONG*)MikMod_malloc((TICKLSIZE+32)*sizeof(SLONG)))) {
      _mm_errno = MMERR_INITIALIZING_MIXER;
      return 1;
    }

  if(md_mode & DMODE_STEREO) {
    Mix32toFP  = Mix32ToFP_Stereo;
#if ((defined HAVE_ALTIVEC || defined HAVE_SSE2) && (SAMPLING_FACTOR == 4))
    if (md_mode & DMODE_SIMDMIXER)
      Mix32to16  = Mix32To16_Stereo_SIMD_4Tap;
    else
#endif
      Mix32to16  = Mix32To16_Stereo;
    Mix32to8   = Mix32To8_Stereo;
    MixReverb  = MixReverb_Stereo;
    MixLowPass = MixLowPass_Stereo;
  } else {
    Mix32toFP  = Mix32ToFP_Normal;
    Mix32to16  = Mix32To16_Normal;
    Mix32to8   = Mix32To8_Normal;
    MixReverb  = MixReverb_Normal;
    MixLowPass = MixLowPass_Normal;
  }


  md_mode |= DMODE_INTERP;
  vc_mode = md_mode;
  return 0;
}

BOOL VC2_PlayStart(void)
{
  md_mode|=DMODE_INTERP;

  samplesthatfit = TICKLSIZE;
  if(vc_mode & DMODE_STEREO) samplesthatfit >>= 1;
  tickleft = 0;

  RVc1 = (5000L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc2 = (5078L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc3 = (5313L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc4 = (5703L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc5 = (6250L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc6 = (6953L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc7 = (7813L * md_mixfreq) / (REVERBERATION_VC2 * 10);
  RVc8 = (8828L * md_mixfreq) / (REVERBERATION_VC2 * 10);

  if(!(RVbufL1=(SLONG*)MikMod_calloc((RVc1+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL2=(SLONG*)MikMod_calloc((RVc2+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL3=(SLONG*)MikMod_calloc((RVc3+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL4=(SLONG*)MikMod_calloc((RVc4+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL5=(SLONG*)MikMod_calloc((RVc5+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL6=(SLONG*)MikMod_calloc((RVc6+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL7=(SLONG*)MikMod_calloc((RVc7+1),sizeof(SLONG)))) return 1;
  if(!(RVbufL8=(SLONG*)MikMod_calloc((RVc8+1),sizeof(SLONG)))) return 1;

  if(!(RVbufR1=(SLONG*)MikMod_calloc((RVc1+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR2=(SLONG*)MikMod_calloc((RVc2+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR3=(SLONG*)MikMod_calloc((RVc3+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR4=(SLONG*)MikMod_calloc((RVc4+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR5=(SLONG*)MikMod_calloc((RVc5+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR6=(SLONG*)MikMod_calloc((RVc6+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR7=(SLONG*)MikMod_calloc((RVc7+1),sizeof(SLONG)))) return 1;
  if(!(RVbufR8=(SLONG*)MikMod_calloc((RVc8+1),sizeof(SLONG)))) return 1;

  RVRindex = 0;
  return 0;
}

void VC2_PlayStop(void)
{
  if(RVbufL1) MikMod_free(RVbufL1);
  if(RVbufL2) MikMod_free(RVbufL2);
  if(RVbufL3) MikMod_free(RVbufL3);
  if(RVbufL4) MikMod_free(RVbufL4);
  if(RVbufL5) MikMod_free(RVbufL5);
  if(RVbufL6) MikMod_free(RVbufL6);
  if(RVbufL7) MikMod_free(RVbufL7);
  if(RVbufL8) MikMod_free(RVbufL8);
  if(RVbufR1) MikMod_free(RVbufR1);
  if(RVbufR2) MikMod_free(RVbufR2);
  if(RVbufR3) MikMod_free(RVbufR3);
  if(RVbufR4) MikMod_free(RVbufR4);
  if(RVbufR5) MikMod_free(RVbufR5);
  if(RVbufR6) MikMod_free(RVbufR6);
  if(RVbufR7) MikMod_free(RVbufR7);
  if(RVbufR8) MikMod_free(RVbufR8);

  RVbufL1=RVbufL2=RVbufL3=RVbufL4=RVbufL5=RVbufL6=RVbufL7=RVbufL8=NULL;
  RVbufR1=RVbufR2=RVbufR3=RVbufR4=RVbufR5=RVbufR6=RVbufR7=RVbufR8=NULL;
}

BOOL VC2_SetNumVoices(void)
{
  int t;

  md_mode|=DMODE_INTERP;

  if(!(vc_softchn=md_softchn)) return 0;

  if(vinf) MikMod_free(vinf);
  if(!(vinf=(VC2::VINFO*)MikMod_calloc(sizeof(VC2::VINFO),vc_softchn))) return 1;

  for(t=0;t<vc_softchn;t++) {
    vinf[t].frq=10000;
    vinf[t].pan=(t&1)?PAN_LEFT:PAN_RIGHT;
  }

  return 0;
}

}; // namespace VC2


/*
#define _IN_VIRTCH_

#define VC1_SilenceBytes      VC2_SilenceBytes
#define VC1_WriteSamples      VC2_WriteSamples
#define VC1_WriteBytes        VC2_WriteBytes
#define VC1_Exit              VC2_Exit
#define VC1_VoiceSetVolume    VC2_VoiceSetVolume
#define VC1_VoiceGetVolume    VC2_VoiceGetVolume
#define VC1_VoiceSetPanning   VC2_VoiceSetPanning
#define VC1_VoiceGetPanning   VC2_VoiceGetPanning
#define VC1_VoiceSetFrequency VC2_VoiceSetFrequency
#define VC1_VoiceGetFrequency VC2_VoiceGetFrequency
#define VC1_VoicePlay         VC2_VoicePlay
#define VC1_VoiceStop         VC2_VoiceStop
#define VC1_VoiceStopped      VC2_VoiceStopped
#define VC1_VoiceGetPosition  VC2_VoiceGetPosition
#define VC1_SampleUnload      VC2_SampleUnload
#define VC1_SampleLoad        VC2_SampleLoad
#define VC1_SampleSpace       VC2_SampleSpace
#define VC1_SampleLength      VC2_SampleLength
#define VC1_VoiceRealVolume   VC2_VoiceRealVolume

#undef _IN_VIRTCH_
*/


void VC_SetupPointers(void)
{
  /*
	if (md_mode&DMODE_HQMIXER) {
    VC_Init_ptr=VC2::VC2_Init;
		VC_Exit_ptr=VC1_Exit;
		VC_SetNumVoices_ptr=VC2::VC2_SetNumVoices;
    VC_SampleSpace_ptr=VC2_SampleSpace;
		VC_SampleLength_ptr=VC2_SampleLength;
		VC_PlayStart_ptr=VC2_PlayStart;
		VC_PlayStop_ptr=VC2_PlayStop;
		VC_SampleLoad_ptr=VC2_SampleLoad;
		VC_SampleUnload_ptr=VC2_SampleUnload;
		VC_WriteBytes_ptr=VC2_WriteBytes;
		VC_SilenceBytes_ptr=VC2_SilenceBytes;
		VC_VoiceSetVolume_ptr=VC2_VoiceSetVolume;
		VC_VoiceGetVolume_ptr=VC2_VoiceGetVolume;
		VC_VoiceSetFrequency_ptr=VC2_VoiceSetFrequency;
		VC_VoiceGetFrequency_ptr=VC2_VoiceGetFrequency;
		VC_VoiceSetPanning_ptr=VC2_VoiceSetPanning;
		VC_VoiceGetPanning_ptr=VC2_VoiceGetPanning;
		VC_VoicePlay_ptr=VC2_VoicePlay;
		VC_VoiceStop_ptr=VC2_VoiceStop;
		VC_VoiceStopped_ptr=VC2_VoiceStopped;
		VC_VoiceGetPosition_ptr=VC2_VoiceGetPosition;
		VC_VoiceRealVolume_ptr=VC2_VoiceRealVolume;
	} else */{
		VC_Init_ptr=VC1_Init;
		VC_Exit_ptr=VC1_Exit;
		VC_SetNumVoices_ptr=VC1_SetNumVoices;
		VC_SampleSpace_ptr=VC1_SampleSpace;
		VC_SampleLength_ptr=VC1_SampleLength;
		VC_PlayStart_ptr=VC1_PlayStart;
		VC_PlayStop_ptr=VC1_PlayStop;
		VC_SampleLoad_ptr=VC1_SampleLoad;
		VC_SampleUnload_ptr=VC1_SampleUnload;
		VC_WriteBytes_ptr=VC1_WriteBytes;
		VC_SilenceBytes_ptr=VC1_SilenceBytes;
		VC_VoiceSetVolume_ptr=VC1_VoiceSetVolume;
		VC_VoiceGetVolume_ptr=VC1_VoiceGetVolume;
		VC_VoiceSetFrequency_ptr=VC1_VoiceSetFrequency;
		VC_VoiceGetFrequency_ptr=VC1_VoiceGetFrequency;
		VC_VoiceSetPanning_ptr=VC1_VoiceSetPanning;
		VC_VoiceGetPanning_ptr=VC1_VoiceGetPanning;
		VC_VoicePlay_ptr=VC1_VoicePlay;
		VC_VoiceStop_ptr=VC1_VoiceStop;
		VC_VoiceStopped_ptr=VC1_VoiceStopped;
		VC_VoiceGetPosition_ptr=VC1_VoiceGetPosition;
		VC_VoiceRealVolume_ptr=VC1_VoiceRealVolume;
	}
}



MODULE* pCurrentMod = NULL;


} // namespace XMPlay



XMPlayer::XMPlayer() :
  m_Volume( 100 ),
  m_Looping( true )
{

  GR::Service::Environment::Instance().SetService( "Sound.ModPlayer", this );

}



XMPlayer::~XMPlayer()
{

  Unload();
  GR::Service::Environment::Instance().RemoveService( "Sound.ModPlayer" );

}



bool XMPlayer::Initialize( GR::IEnvironment& Environment )
{

  m_pEnvironment = &Environment;

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

  XMPlay::g_hwndMain = hWnd;

  //m_pSoundClass = pSound;

  // PFUI
  //XMPlay::pSoundClass = pSound;

  XMPlay::MikMod_RegisterAllDrivers();
  XMPlay::MikMod_RegisterAllLoaders();

  XMPlay::md_mode |= DMODE_SOFT_MUSIC;

  if ( XMPlay::MikMod_Init( "" ) )
  {
    //MessageBox( NULL, "Init failed!", "Oh no!", 0 );
		return false;
	}
  return true;

}



void XMPlayer::Unload()
{

  /*
  if ( ( m_pSoundClass )
  &&   ( XMPlay::dwSoundBufferHandle )
  &&   ( m_pSoundClass->IsPlaying( XMPlay::dwSoundBufferHandle ) ) )
  {
    Stop();
  }
  */
  if ( XMPlay::pCurrentMod )
  {
    XMPlay::Player_Free( XMPlay::pCurrentMod );
    XMPlay::pCurrentMod = NULL;
  }

}



bool XMPlayer::Release()
{

  if ( IsPlaying() )
  {
    Stop();
  }
  Unload();
  XMPlay::MikMod_Exit();

  return true;

}



bool XMPlayer::Play( bool bLoop )
{

  if ( XMPlay::pCurrentMod == NULL )
  {
    return false;
  }

  return StartThread();

}



int XMPlayer::Run()
{
  while ( ( m_Looping )
  &&      ( !HaveToShutDown() ) )
  {
	  XMPlay::Player_Start( XMPlay::pCurrentMod );
    XMPlay::Player_SetVolume( (SWORD)( m_Volume * 128 / 100 ) );

	  while ( ( XMPlay::Player_Active() )
    &&      ( !HaveToShutDown() ) )
    {
		  Sleep( 10 );
		  XMPlay::MikMod_Update();
	  }

	  //XMPlay::Player_Stop();
    // Hässlich, aber Puffer ausspielen lassen
    //Sleep( 1000 );
    if ( !HaveToShutDown() )
    {
      if ( XMPlay::pCurrentMod )
      {
        XMPlay::Player_Init_internal( XMPlay::pCurrentMod );
      }
    }
  }

	//Player_Free( pCurrentMod );

  return true;

}



bool XMPlayer::LoadMusic( const GR::Char* szFileName )
{
  if ( IsPlaying() )
  {
    Stop();
  }

  XMPlay::Player_Free( XMPlay::pCurrentMod );
  XMPlay::pCurrentMod = NULL;

  Release();
  //Initialize( m_pSoundClass );
  Initialize( *m_pEnvironment );
  SetVolume( m_Volume );

  XMPlay::pCurrentMod = XMPlay::Player_Load( szFileName, 64, 0 );
  return true;
}



bool XMPlayer::LoadMusic( IIOStream& ioIn )
{

  if ( IsPlaying() )
  {
    Stop();
  }

  XMPlay::Player_Free( XMPlay::pCurrentMod );
  XMPlay::pCurrentMod = NULL;

  Release();
  //Initialize( m_pSoundClass );
  Initialize( *m_pEnvironment );
  SetVolume( m_Volume );

  ByteBuffer     bbIn;

  bbIn.Resize( (size_t)ioIn.GetSize() );
  ioIn.ReadBlock( bbIn.Data(), bbIn.Size() );

  XMPlay::pCurrentMod = XMPlay::Player_LoadMem( (const char*)bbIn.Data(), (int)bbIn.Size(), 64, 0 );

  return true;

}



void XMPlayer::Stop()
{

  ShutDown();

  /*
  if ( XMPlay::g_dwMMTimerID )
  {
    timeKillEvent( XMPlay::g_dwMMTimerID );
    XMPlay::g_dwMMTimerID = 0;
  }
  */

  XMPlay::Player_Stop();

  Release();

}



int XMPlayer::Volume()
{

  return m_Volume;

}



bool XMPlayer::SetVolume( int iVolume )
{

  /*
  if ( m_pSoundClass )
  {
    m_pSoundClass->SetVolume( XMPlay::dwSoundBufferHandle, ucVolume );
  }
  */
  if ( iVolume < 0 )
  {
    iVolume = 0;
  }
  if ( iVolume > 100 )
  {
    iVolume = 100;
  }
  XMPlay::Player_SetVolume( iVolume * 128 / 100 );
  m_Volume = iVolume;

  return true;

}



void XMPlayer::OnServiceGotUnset( const GR::String& strService, GR::IService* pService )
{

  /*
  if ( strService == "Sound" )
  {
    Release();
    XMPlay::pSoundClass = NULL;
    m_pSoundClass = NULL;
  }
  */

}



void XMPlayer::OnServiceGotSet( const GR::String& strService, GR::IService* pService )
{

  /*
  if ( strService == "Sound" )
  {
    Initialize();
  }
  */

}



bool XMPlayer::Pause()
{

  if ( XMPlay::Player_Active() )
  {
    XMPlay::Player_TogglePause();
  }

  return true;

}



bool XMPlayer::Resume()
{

  if ( !XMPlay::Player_Active() )
  {
    XMPlay::Player_TogglePause();
  }

  return true;

}



bool XMPlayer::IsPlaying()
{

  return !!XMPlay::Player_Active();

}



bool XMPlayer::IsInitialized()
{

  // TODO!
  return true;

}