/*	MikMod sound library
	(c) 1998, 1999, 2000, 2001, 2002 Miodrag Vallat and others - see file
	AUTHORS for complete list.

	This library is free software; you can redistribute it and/or modify
	it under the terms of the GNU Library General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.
 
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Library General Public License for more details.
 
	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.
*/

/*==============================================================================

  $Id: load_mtm.c,v 1.2 2005/03/30 19:10:02 realtech Exp $

  MTM module loader

==============================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>

//#include "mikmod_internals.h"

#ifdef SUNOS
extern int fprintf(FILE *, const char *, ...);
#endif

/*========== Module structure */

typedef struct MTMHEADER {
	UBYTE id[3];			/* MTM file marker */
	UBYTE version;			/* upper major, lower nibble minor version number */
	CHAR  songname[20];		/* ASCIIZ songname */
	UWORD numtracks;		/* number of tracks saved */
	UBYTE lastpattern;		/* last pattern number saved */
	UBYTE lastorder;		/* last order number to play (songlength-1) */
	UWORD commentsize;		/* length of comment field */
	UBYTE numsamples;		/* number of samples saved  */
	UBYTE attribute;		/* attribute byte (unused) */
	UBYTE beatspertrack;
	UBYTE numchannels;		/* number of channels used  */
	UBYTE panpos[32];		/* voice pan positions */
} MTMHEADER;

typedef struct MTMSAMPLE {
	CHAR  samplename[22];
	ULONG length;
	ULONG reppos;
	ULONG repend;
	UBYTE finetune;
	UBYTE volume;
	UBYTE attribute;
} MTMSAMPLE;

typedef struct MTMNOTE {
	UBYTE a,b,c;
} MTMNOTE;

/*========== Loader variables */

static MTMHEADER *mtm_mh = NULL;
static MTMNOTE *mtmtrk = NULL;
static UWORD mtm_pat[32];

static CHAR MTM_Version[] = "MTM";

/*========== Loader code */

BOOL MTM_Test(void)
{
	UBYTE id[3];

	if(!_mm_read_UBYTES(id,3,modreader)) return 0;
	if(!memcmp(id,"MTM",3)) return 1;
	return 0;
}

BOOL MTM_Init(void)
{
	if(!(mtmtrk=(MTMNOTE*)MikMod_calloc(64,sizeof(MTMNOTE)))) return 0;
	if(!(mtm_mh=(MTMHEADER*)MikMod_malloc(sizeof(MTMHEADER)))) return 0;

	return 1;
}

void MTM_Cleanup(void)
{
	MikMod_free(mtmtrk);
	MikMod_free(mtm_mh);
}

static UBYTE* MTM_Convert(void)
{
	int t;
	UBYTE a,b,inst,note,eff,dat;

	UniReset();
	for(t=0;t<64;t++) {
		a=mtmtrk[t].a;
		b=mtmtrk[t].b;
		inst=((a&0x3)<<4)|(b>>4);
		note=a>>2;
		eff=b&0xf;
		dat=mtmtrk[t].c;

		if(inst) UniInstrument(inst-1);
		if(note) UniNote(note+2*OCTAVE);

		/* MTM bug workaround : when the effect is volslide, slide-up *always*
		   overrides slide-down. */
		if(eff==0xa && (dat&0xf0)) dat&=0xf0;

		/* Convert pattern jump from Dec to Hex */
		if(eff==0xd)
			dat=(((dat&0xf0)>>4)*10)+(dat&0xf);
		UniPTEffect(eff,dat);
		UniNewline();
	}
	return UniDup();
}

BOOL MTM_Load(BOOL curious)
{
	int t,u;
	MTMSAMPLE s;
	SAMPLE *q;

	/* try to read module header  */
	_mm_read_UBYTES(mtm_mh->id,3,modreader);
	mtm_mh->version     =_mm_read_UBYTE(modreader);
	_mm_read_string(mtm_mh->songname,20,modreader);
	mtm_mh->numtracks   =_mm_read_I_UWORD(modreader);
	mtm_mh->lastpattern =_mm_read_UBYTE(modreader);
	mtm_mh->lastorder   =_mm_read_UBYTE(modreader);
	mtm_mh->commentsize =_mm_read_I_UWORD(modreader);
	mtm_mh->numsamples  =_mm_read_UBYTE(modreader);
	mtm_mh->attribute   =_mm_read_UBYTE(modreader);
	mtm_mh->beatspertrack=_mm_read_UBYTE(modreader);
	mtm_mh->numchannels =_mm_read_UBYTE(modreader);
	_mm_read_UBYTES(mtm_mh->panpos,32,modreader);

	if(_mm_eof(modreader)) {
		_mm_errno = MMERR_LOADING_HEADER;
		return 0;
	}

	/* set module variables */
	of.initspeed = 6;
	of.inittempo = 125;
	of.modtype   = _strdup(MTM_Version);
	of.numchn    = mtm_mh->numchannels;
	of.numtrk    = mtm_mh->numtracks+1;           /* get number of channels */
	of.songname  = DupStr(mtm_mh->songname,20,1); /* make a cstr of songname */
	of.numpos    = mtm_mh->lastorder+1;           /* copy the songlength */
	of.numpat    = mtm_mh->lastpattern+1;
	of.reppos    = 0;
	of.flags    |= UF_PANNING;
	for(t=0;t<32;t++) of.panning[t]=mtm_mh->panpos[t]<< 4;
	of.numins=of.numsmp=mtm_mh->numsamples;

	if(!AllocSamples()) return 0;
	q=of.samples;
	for(t=0;t<of.numins;t++) {
		/* try to read sample info */
		_mm_read_string(s.samplename,22,modreader);
		s.length    =_mm_read_I_ULONG(modreader);
		s.reppos    =_mm_read_I_ULONG(modreader);
		s.repend    =_mm_read_I_ULONG(modreader);
		s.finetune  =_mm_read_UBYTE(modreader);
		s.volume    =_mm_read_UBYTE(modreader);
		s.attribute =_mm_read_UBYTE(modreader);

		if(_mm_eof(modreader)) {
			_mm_errno = MMERR_LOADING_SAMPLEINFO; 
			return 0;
		}

		q->samplename = DupStr(s.samplename,22,1);
		q->seekpos   = 0;
		q->speed     = finetune[s.finetune];
		q->length    = s.length;
		q->loopstart = s.reppos;
		q->loopend   = s.repend;
		q->volume    = s.volume;
		if((s.repend-s.reppos)>2) q->flags |= SF_LOOP;

		if(s.attribute&1) {
			/* If the sample is 16-bits, convert the length and replen
			   byte-values into sample-values */
			q->flags|=SF_16BITS;
			q->length>>=1;
			q->loopstart>>=1;
			q->loopend>>=1;
		}
		q++;
	}

	if(!AllocPositions(of.numpos)) return 0;
	for(t=0;t<of.numpos;t++)
		of.positions[t]=_mm_read_UBYTE(modreader);
	for(;t<128;t++) _mm_read_UBYTE(modreader);
	if(_mm_eof(modreader)) {
		_mm_errno = MMERR_LOADING_HEADER;
		return 0;
	}

	if(!AllocTracks()) return 0;
	if(!AllocPatterns()) return 0;

	of.tracks[0]=MTM_Convert();		/* track 0 is empty */
	for(t=1;t<of.numtrk;t++) {
		int s;

		for(s=0;s<64;s++) {
			mtmtrk[s].a=_mm_read_UBYTE(modreader);
			mtmtrk[s].b=_mm_read_UBYTE(modreader);
			mtmtrk[s].c=_mm_read_UBYTE(modreader);
		}

		if(_mm_eof(modreader)) {
			_mm_errno = MMERR_LOADING_TRACK;
			return 0;
		}

		if(!(of.tracks[t]=MTM_Convert())) return 0;
	}

	for(t=0;t<of.numpat;t++) {
		_mm_read_I_UWORDS(mtm_pat,32,modreader);
		for(u=0;u<of.numchn;u++)
			of.patterns[((long)t*of.numchn)+u]=mtm_pat[u];
	}

	/* read comment field */
	if(mtm_mh->commentsize)
		if(!ReadLinedComment(mtm_mh->commentsize, 40)) return 0;

	return 1;
}

CHAR *MTM_LoadTitle(void)
{
	CHAR s[20];

	_mm_fseek(modreader,4,SEEK_SET);
	if(!_mm_read_UBYTES(s,20,modreader)) return NULL;

	return(DupStr(s,20,1));
}

/*========== Loader information */

MLOADER load_mtm={
	NULL,
	"MTM",
	"MTM (MultiTracker Module editor)",
	MTM_Init,
	MTM_Test,
	MTM_Load,
	MTM_Cleanup,
	MTM_LoadTitle
};

/* ex:set ts=4: */
