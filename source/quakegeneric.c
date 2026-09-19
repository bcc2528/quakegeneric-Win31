/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"

static quakeparms_t    parms;

void QG_Tick(double duration)
{
	Host_Frame(duration);
}

void QG_Free(void)
{
	if(parms.membase != NULL)
	{
		free(parms.membase);
	}
}

int QG_Create(int argc, char *argv[])
{
	int t;

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 8*1024*1024;

	if (COM_CheckParm ("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;

		if (t < com_argc)
			parms.memsize = Q_atoi (com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);
	if(parms.membase == NULL)
	{
		return 0;
	}
	parms.basedir = ".";

	printf ("Host_Init\n");
	Host_Init (&parms);

	return 1;
}
