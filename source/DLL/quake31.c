#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#define NUM_BUFFERS     4

/* ÉOÉçÅ[ÉoÉãïœêî */
HWAVEOUT    ghWaveOut = NULL;
WAVEHDR     gWaveHdr[NUM_BUFFERS];
HGLOBAL     ghBuffer[NUM_BUFFERS] = {NULL, NULL};
LPSTR       gpBuffer[NUM_BUFFERS] = {NULL, NULL};

UINT	buffer_size;
UINT	gCurBuf;

int FAR PASCAL LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine)
{
	if (wHeapSize != 0)
	{
        	UnlockData(0);
	}
	return 1;
}

int FAR PASCAL WEP(int nSystemExit)
{
	return 1;
}


DWORD FAR PASCAL __export GetTime(void)
{
	return timeGetTime();
}

DWORD FAR PASCAL __export SoundInit16(UINT size)
{
	int i, j;
	UINT ret;
	LPSTR Buf;
	PCMWAVEFORMAT pcmFormat;

	pcmFormat.wf.wFormatTag     = WAVE_FORMAT_PCM;
	pcmFormat.wf.nChannels      = 2;
	pcmFormat.wf.nSamplesPerSec = 11025L;
	pcmFormat.wf.nAvgBytesPerSec= 44100L;
	pcmFormat.wf.nBlockAlign    = 4;
	pcmFormat.wBitsPerSample    = 16;

	ret = waveOutOpen(&ghWaveOut, WAVE_MAPPER, (LPWAVEFORMAT)&pcmFormat,
                        NULL, 0L, CALLBACK_NULL);

	if (ret != MMSYSERR_NOERROR)
	{
		return 0;
	}

    	for (i = 0; i < NUM_BUFFERS; i++)
	{

		ghBuffer[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, size);
		gpBuffer[i] = (LPSTR)GlobalLock(ghBuffer[i]);

		GlobalPageLock(ghBuffer[i]);

		_fmemset(&gWaveHdr[i], 0, sizeof(WAVEHDR));
		gWaveHdr[i].lpData = gpBuffer[i];
		gWaveHdr[i].dwBufferLength = size;

		Buf = gpBuffer[i];

		for (j = 0; j < size; j++)
		{
			Buf[j] = 0;
		}

		waveOutPrepareHeader(ghWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
		waveOutWrite(ghWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
	}

	buffer_size = size;
	gCurBuf = 0;

	return 1;
}

DWORD FAR PASCAL __export SoundInit(UINT size)
{
	int i, j;
	UINT ret;
	LPSTR Buf;
	PCMWAVEFORMAT pcmFormat;

	pcmFormat.wf.wFormatTag     = WAVE_FORMAT_PCM;
	pcmFormat.wf.nChannels      = 1;
	pcmFormat.wf.nSamplesPerSec = 11025L;
	pcmFormat.wf.nAvgBytesPerSec= 11025L;
	pcmFormat.wf.nBlockAlign    = 1;
	pcmFormat.wBitsPerSample    = 8;

	ret = waveOutOpen(&ghWaveOut, WAVE_MAPPER, (LPWAVEFORMAT)&pcmFormat,
                        NULL, 0L, CALLBACK_NULL);

	if (ret != MMSYSERR_NOERROR)
	{
		return 0;
	}

    	for (i = 0; i < NUM_BUFFERS; i++)
	{

		ghBuffer[i] = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, size);
		gpBuffer[i] = (LPSTR)GlobalLock(ghBuffer[i]);

		GlobalPageLock(ghBuffer[i]);

		_fmemset(&gWaveHdr[i], 0, sizeof(WAVEHDR));
		gWaveHdr[i].lpData = gpBuffer[i];
		gWaveHdr[i].dwBufferLength = size;

		Buf = gpBuffer[i];

		for (j = 0; j < size; j++)
		{
			Buf[j] = 128;
		}

		waveOutPrepareHeader(ghWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
		waveOutWrite(ghWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
	}

	buffer_size = size;
	gCurBuf = 0;

	return 1;
}

DWORD FAR PASCAL __export SoundQuit(void)
{
	int i;

	if (ghWaveOut)
	{
	        waveOutReset(ghWaveOut);

		for (i = 0; i < NUM_BUFFERS; i++)
		{
			if (gWaveHdr[i].dwFlags & WHDR_PREPARED)
			{
	                	waveOutUnprepareHeader(ghWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
			}
			if (ghBuffer[i])
			{
				GlobalPageUnlock(ghBuffer[i]);
				GlobalUnlock(ghBuffer[i]);
				GlobalFree(ghBuffer[i]);
				ghBuffer[i] = NULL;
			}
		}
		waveOutClose(ghWaveOut);
		ghWaveOut = NULL;
	}

	return 0;
}

DWORD FAR PASCAL __export SoundPosition(void)
{
	int i;
	MMTIME mmtime;

	if(ghWaveOut == NULL)
	{
		return 0;
	}

	mmtime.wType = TIME_BYTES;

	waveOutGetPosition(ghWaveOut, &mmtime, sizeof(MMTIME));

	return mmtime.u.cb;
}

DWORD FAR PASCAL __export SoundWrite(LPSTR pBuffer)
{
	int i;
	LPSTR Buf;
	UINT num;

	if(ghWaveOut == NULL)
	{
		return 0;
	}

	num = gCurBuf;

	if (!(gWaveHdr[num].dwFlags & WHDR_DONE))
	{
			return 0;
	}

	Buf = gpBuffer[num];

	memcpy(Buf, pBuffer, buffer_size);

 	waveOutWrite(ghWaveOut, &gWaveHdr[num], sizeof(WAVEHDR));

	gCurBuf++;
	if(gCurBuf >= NUM_BUFFERS)
	{
		gCurBuf = 0;
	}

	return 1;
}
