#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdarg>
#endif

#include <cstdio>
#include <cassert>
#include <cstring>
#include <stdlib.h> // mbstowcs_s

#include "utils/logging.h"

#include "Core.h"


void debugs(logging::eLogCategory lc, char* s)
{
#if defined(PLATFORM_WINDOWS) && defined(_DEBUG)
	OutputDebugStringA(s);
#endif

	WCHAR buf[255];
	size_t n;
	mbstowcs_s(&n,buf,255,s,254);
	switch (lc)
	{
	case logging::LC_DEBUG:
	case logging::LC_INFO:
		GLog->Log(buf);
#ifdef _DEBUG //In debug mode, print output to console
		puts(s);
#endif
		break;
	case logging::LC_WARNING:
		GWarn->Log(buf);
		puts(s);
		break;
	case logging::LC_ERROR:
		GError->Log(buf);
		puts(s);
		break;
	}
}

namespace logging {

static const int MAX_LOG_LINE = 1024;

void logmsg(eLogCategory lc, const char* file, int line, const char* fmt, ...)
{
	assert(lc < NUM_LC_LOGCATEGORY && lc>=0 && fmt);

	static const char* 	msgs[NUM_LC_LOGCATEGORY] = { "DEBUG", "INFO", "WARNING", "ERROR" };
	char				text[MAX_LOG_LINE] = {0};
	va_list				ap;				

	if (!strlen(fmt)) return;

// to support jump to error
#ifdef PLATFORM_WINDOWS
	sprintf(text, "%s(%d): %s: ", file, line, msgs[lc]);
#else
	sprintf(text, "%s:%d: %s: ", file, line, msgs[lc]);
#endif
	size_t hdr_len = strlen(text);

	va_start(ap, fmt);	
	vsnprintf(text+strlen(text), MAX_LOG_LINE - hdr_len-2, fmt, ap); // 1 for "\0"

	va_end(ap);

	size_t len = strlen(text);
	text[len] = '\n';

	debugs(lc, text);
}


}

