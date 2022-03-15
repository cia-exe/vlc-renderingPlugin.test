#pragma once

// Which platform we are on?
#if _MSC_VER
#define UNITY_WIN 1
#endif

// Which graphics device APIs we possibly support?
#if UNITY_WIN
#define SUPPORT_D3D11 1 // comment this out if you don't have D3D11 header/library files
#endif

#define DEBUGLOG 1

#include "Unity/IUnityInterface.h"
// --------------------------------------------------------------------------
// Include headers for the graphics APIs we support
#if SUPPORT_D3D11
#	include <d3d11.h>
#	include "Unity/IUnityGraphicsD3D11.h"
#endif
// --------------------------------------------------------------------------
#include <mutex>
#include <queue>

#define SAFE_RELEASE(x) {if(x) x->Release(); x=0;}	

namespace vlcVideoTexture
{
	class Autolock
	{
		std::mutex& p;
	public:
		Autolock(std::mutex& _p) : p(_p) { p.lock(); }
		~Autolock() { p.unlock(); }
	};


	class CDirectXResource
	{
	public:
		ID3D11Texture2D *pTex0, *pTex1, *pTex2;
		ID3D11ShaderResourceView *pTxSrv0, *pTxSrv1, *pTxSrv2;

		CDirectXResource();
		~CDirectXResource();
		void CreateTextures(int w, int h);
		void Release();

	protected:
		std::mutex mutex;
		void mCreateTextures(int w, int h);
		void mRelease();
	};


	class CFrameMemory // maxium planes == 3 
	{
		// rtti check for void* casting; need to add "mTypeInfo(typeid(*this))" in constructor.

	public:
		UINT pitches[3];
		UINT lines[3];
		UINT8* buffers[3];
		char chroma[4];

		//unsigned width;
		//unsigned height;
		//bool bNewData;
		//UINT8* tempBuffer;

		CFrameMemory();
		CFrameMemory(const CFrameMemory& o);
		~CFrameMemory();
		void allocBuffers();
		void release();
		bool CheckType();
		void lock();
		void unlock();

		//CFrameMemory(CFrameMemory&&) = default;
		//CFrameMemory(const CFrameMemory&) = default;
		//CFrameMemory& operator=(const CFrameMemory&) = default;

	protected:
		const std::type_info& mTypeInfo;
		std::mutex mutex;

		UINT multiple_of_32(UINT x);
		void freeBuffers();
		void init();
	};


	class CFrameQueue
	{
		// rtti check for void* casting; need to add "mTypeInfo(typeid(*this))" in constructor.

	public:
		CFrameQueue(CFrameMemory& prototype);
		~CFrameQueue();

		int GetWidth();
		int GetHeight();
		void SetWidth(int v);
		void SetHeight(int v);
		CFrameMemory* GetEmptyBuffer();
		void Push(CFrameMemory* p);
		CFrameMemory* Pop();
		void Recycle(CFrameMemory* p);
		bool CheckType();
		void LogStatus();

	protected:
		const std::type_info& mTypeInfo;
		std::mutex mutex;
		void Lock();
		void Unlock();

	private:
		CFrameMemory mPrototype;
		std::queue<CFrameMemory*> mRecycle;
		std::queue<CFrameMemory*> mQueue;
		std::queue<CFrameMemory*> mAll;
		CFrameQueue() = delete;
		int mWidth;
		int mHeight;
	};


}
