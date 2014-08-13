/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: basedec.cpp 2013/10/12 mohousch Exp $

	Copyright (C) 2004 Zwen
	base decoder class
	Homepage: http://www.cyberphoria.org/

	Kommentar:

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <basedec.h>
#include <ffmpegdec.h>

#include <driver/netfile.h>

#include <driver/audioplay.h> // for ShoutcastCallback()

#include <global.h>
#include <neutrino.h>

/*zapit includes*/
#include <client/zapittools.h>

#include <system/debug.h>


unsigned int CBaseDec::mSamplerate = 0;

void ShoutcastCallback(void *arg)
{
	CAudioPlayer::getInstance()->sc_callback(arg);
}

bool CBaseDec::GetMetaDataBase(CAudiofile* const in, const bool nice)
{
	bool Status = true;

	FILE * fp = fopen( in->Filename.c_str(), "r" );
	if ( fp == NULL )
	{
		fprintf( stderr, "Error opening file %s for meta data reading.\n", in->Filename.c_str() );
		Status = false;
	}
	else
	{
		Status = CFfmpegDec::getInstance()->GetMetaData(fp, nice, &in->MetaData);
			
		if ( fclose( fp ) == EOF )
		{
			dprintf(DEBUG_NORMAL, "Could not close file %s.\n", in->Filename.c_str() );
		}
	}

	return Status;
}

void CBaseDec::Init()
{
	mSamplerate = 0;
}

