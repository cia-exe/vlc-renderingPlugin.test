#pragma once

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

#undef  CFLAG_DEBUG
#define CFLAG_DEBUG

#ifdef WIN32
#define __WINDOWS__
#elif WIN64
#define __WINDOWS__
#endif


#ifdef __WINDOWS__
//#define __PRETTY_FUNCTION__ __FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCSIG__
//#include <tchar.h>
#endif

#define __X_FUNCTION__ __FUNCTION__ // for shorter log output

//#include <utils/Trace.h> 
//#undef ATRACE_TAG
//#define ATRACE_TAG ATRACE_TAG_GRAPHICS

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <float.h>
#include <string.h>
#include <limits.h>

//#include <unistd.h> //for usleep() in linux


//#include <exception>
//=================================================================
//void* operator new		( size_t s );
//void* operator new[]	( size_t s );
//void  operator delete	( void *p );
//void  operator delete[] ( void *p );

extern "C" void _TraceFormat(const char *fmt, ...);

//=================================================================

//#ifndef ARRAYSIZE // it works but I don't understand its concept.
//
//// copy from WinNT.h
//template <typename T, size_t N>
//char (*RtlpNumberOf(T (&)[N] ))[N];
//#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)))
//#define ARRAYSIZE(A)    RTL_NUMBER_OF_V2(A)
//
//#endif

//#ifdef ARRAYSIZE
//#undef ARRAYSIZE
//#endif

#define _ARRAY_SIZE(A)    (sizeof(A)/sizeof(*A))

// **************************************************************************
void ErrorExit(const char *function, const char *file, int line);
//void ErrorExit( const char *fmt, ... );

//#define ERROR_EXIT(msg) ErrorExit(__FUNCTION__,msg)
#define ERROR_EXIT() ErrorExit(__PRETTY_FUNCTION__ ,__FILE__,__LINE__)
#define CHECKPOINT() _TraceFormat("****(%p) check %s, line: %d\n",this, __X_FUNCTION__ ,__LINE__)
#define CHECKPOINT_C(id) _TraceFormat("****(%p) check %s, line: %d\n",id, __X_FUNCTION__ ,__LINE__)
#define WARNPOINT() _TraceFormat("!!! **** WARNING %s, line: %d\n",__X_FUNCTION__ ,__LINE__)
#define DEPRECATED() {/*_TraceFormat("**Deprecated %s\n",__PRETTY_FUNCTION__)*/}

#define SAFE_DELETE_PTR(p) {if(p){ /*_TraceFormat("%s:%d del(%p)\n",__X_FUNCTION__ ,__LINE__,p);*/ delete p;} p=0;}
#define SAFE_DELETE_ARR(p) {if(p) delete[] p; p=0;}
#define SAFE_FREE_PTR(p) {if(p) free(p); p=0;}

typedef unsigned long long ntime;
ntime GetCurrentTimeInNanoseconds();

class CheckScope
{
	//char m_strMessage[500]; // consume too many memory.
	const char* m_strFunction;
	int m_iLine;
	const void* m_pObj;

public:
	/*CheckScope(const char* message)
	{
		strcpy(m_strMessage,message);
		ShowEnterMessage();
	}*/

	CheckScope(void* pObj, const char* strFunction, int iLine)
	{
		m_pObj = pObj;
		m_strFunction = strFunction;
		m_iLine = iLine;
		_TraceFormat("++(%p) %s:%d\n", m_pObj, m_strFunction, m_iLine);
	}

	~CheckScope()
	{
		_TraceFormat("--(%p) %s:%d\n", m_pObj, m_strFunction, m_iLine);
	}
};

#define CHECKSCOPE() CheckScope ____checkScope(this, __X_FUNCTION__ ,__LINE__)
#define CHECKSCOPE_C(id) CheckScope ____checkScope((void*)id, __X_FUNCTION__ ,__LINE__)
//#define CHECKSCOPE_MSG(MESSAGE) CheckScope ____checkScope(MESSAGE)

class CheckScopeTime
{
	//char m_strMessage[500]; // consume too many memory.
	const char* m_strFunction;
	int m_iLine;
	ntime m__time;
	const void* m_pObj;

public:

	CheckScopeTime(void* pObj, const char* strFunction, int iLine)
	{
		m_pObj = pObj;
		m_strFunction = strFunction;
		m_iLine = iLine;
		//_TraceFormat("<-(%p) %s:%d\n",m_pObj, m_strFunction,m_iLine);
		m__time = GetCurrentTimeInNanoseconds();
	}

	~CheckScopeTime()
	{
		_TraceFormat("(%p) %s:%d, tm=%6.2f\n", m_pObj, m_strFunction, m_iLine, (GetCurrentTimeInNanoseconds() - m__time) / 1000000.0f);
	}
};

#define CHECKSCOPETIME() CheckScopeTime ____checkScopeTime(this, __X_FUNCTION__ ,__LINE__)
#define CHECKSCOPETIME_C(id) CheckScopeTime ____checkScopeTime(id, __X_FUNCTION__ ,__LINE__)


UINT64 get_thread_time(HANDLE thread);

class ThreadTimer
{
	HANDLE mThread;
	UINT64 mTime;
	const char*	mTag;

public:
	ThreadTimer(const char* tag) : mTag(tag), mTime(0) {}

	void start()
	{
		mThread = GetCurrentThread();
		mTime = get_thread_time(mThread);
	}

	void stop(const char* msg = "")
	{
		HANDLE t = GetCurrentThread();
		if (mThread != t)
			_TraceFormat("!!!! %s: different thread !!! %p vs %p", mTag, mThread, t);

		UINT64 tm = get_thread_time(mThread) - mTime;

		_TraceFormat("%s: %s (%d)", mTag, msg, tm);
	}

};


class AvgThreadTimer
{
	UINT mMsDuration;
public:
	AvgThreadTimer(UINT msDuration) : mMsDuration(msDuration)
	{}
};