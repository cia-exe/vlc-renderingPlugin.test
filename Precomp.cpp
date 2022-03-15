#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

#include "Precomp.h"

#include <stdio.h>
#include <stdarg.h>


// **************************************************************************
void _TraceFormat(const char *format, ...)
{
	//CHECKSCOPEMEM_C(0);
	char buf[512];
	DWORD  tid = GetCurrentThreadId();
	int l = sprintf(buf, "[%x]", tid);

	va_list ap;
	va_start(ap, format);
	//_vsnprintf_s(buf, _TRUNCATE, format, ap);
	vsnprintf(buf + l, sizeof(buf) - l, format, ap);
	OutputDebugStringA(buf);
	va_end(ap);
}


double get_wall_time() {
	LARGE_INTEGER time, freq;
	if (!QueryPerformanceFrequency(&freq)) {
		//  Handle error
		return 0;
	}
	if (!QueryPerformanceCounter(&time)) {
		//  Handle error
		return 0;
	}
	return (double)time.QuadPart / freq.QuadPart;
}
double get_cpu_time() {
	FILETIME a, b, c, d;
	if (GetProcessTimes(GetCurrentProcess(), &a, &b, &c, &d) != 0) {
		//  Returns total user time.
		//  Can be tweaked to include kernel times as well.
		return
			(double)(d.dwLowDateTime |
				((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
	}
	else {
		//  Handle error
		return 0;
	}
}


ntime GetCurrentTimeInNanoseconds()
{
	return get_wall_time() * 1000000000;
	//return get_cpu_time() * 1000000000;
}

#if(0)

#pragma message("overload operator new")

void* operator new		(size_t s)
{
	//if(rand()%99==0)
	//{
	//	_TraceFormat("**** simulate memory corruption!!! (%d)\n",s);
	//	s-=rand()%32;
	//	if(s<=0) s=1;
	//}
	//_TraceFormat( "::operator new (%d)\n", s );
	return AllocMem(s);
}
void* operator new[](size_t s)
{
	//if(rand()%99==0)
	//{
	//	_TraceFormat("**** simulate memory corruption!!! (%d)\n",s);
	//	s-=rand()%32;
	//	if(s<=0) s=1;
	//}

	//_TraceFormat( "::operator new[] (%d)\n", s );
	return AllocMem(s);
}
void  operator delete	(void *p)
{
	//_TraceFormat( "::operator delete (0x%x)\n", (int)p );
	FreeMem(p);
}
void  operator delete[](void *p)
{
	//_TraceFormat( "::operator delete[] (0x%x)\n", (int)p );
	FreeMem(p);
}

#endif



#if 0
UINT64 get_thread_time(HANDLE thread) {
	FILETIME a, b, c, d;
	if (GetThreadTimes(thread, &a, &b, &c, &d) != 0) {

		SYSTEMTIME sTime;
		FileTimeToSystemTime(&d, &sTime);

		return ((UINT64)sTime.wMinute * 60 * 1000) + sTime.wSecond * 1000 + sTime.wMilliseconds;
	}
	else {
		WARNPOINT();
		return 0;
	}
}
#endif // 0


UINT64 get_thread_time(HANDLE thread) {
	FILETIME a, b, c, d;
	if (GetThreadTimes(thread, &a, &b, &c, &d) != 0) {
		return (d.dwLowDateTime | ((UINT64)d.dwHighDateTime << 32)) / 10000;
	}
	else {
		WARNPOINT();
		return 0;
	}
}
