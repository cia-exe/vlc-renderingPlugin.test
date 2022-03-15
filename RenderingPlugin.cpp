#include "Precomp.h"
typedef SSIZE_T ssize_t;
#include "RenderingPlugin.h"
#include "Unity/IUnityGraphics.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <vlc.h>
#include <libvlc.h>


#pragma message("++ vlc_fourcc.h")
#pragma warning(disable:4244)
#pragma warning(disable:4996)
#define LIBVLC_USE_PTHREAD_CANCEL
#include <vlc_fourcc.h>
#pragma warning(default:4244)
#pragma warning(default:4996)
#pragma message("-- vlc_fourcc.h")

static bool bUseUnityTexture = true;
static bool s_bInvalidMedia = false;
static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static void* g_TexturePointer = NULL;
bool binitmp = false;
bool bPlayed = false;
libvlc_instance_t * inst = 0;
libvlc_media_player_t *mp = 0;

#if SUPPORT_D3D11
static ID3D11Device* g_D3D11Device = NULL;
#endif


//#undef CHECKSCOPE()
//#define CHECKSCOPE()
//#undef CHECKSCOPE_C(id)
//#define CHECKSCOPE_C(id)

// function declaration
ID3D11ShaderResourceView* createD3DTextureSRV(UINT w, UINT h, ID3D11Texture2D** ppTex);


namespace vlcVideoTexture
{

	std::mutex s_mutex;
	CDirectXResource s_dxRc;
	CFrameQueue* s_pContex = 0;
	ThreadTimer g_decodeTimer("decode time");

	//====================== for libvlc_video_set_format_callbacks =================

	static unsigned video_format_cb32(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
	{
		CHECKSCOPE_C(0);

		//Furthermore, we recommend that pitches and lines be multiple of 32
		//to not break assumption that might be made by various optimizations
		// in the video decoders, video filters and / or video converters.

		assert(*width);
		assert(*height);
		unsigned w = *width;
		unsigned h = *height;

		//CFrameMemory &ctx = **(CFrameMemory**)opaque;
		CFrameMemory ctx;
		CFrameQueue *&q = **(CFrameQueue***)opaque;

		//chroma[0] = 'R'; chroma[1] = 'V'; chroma[2] = '2'; chroma[3] = '4';
		chroma[0] = 'R'; chroma[1] = 'V'; chroma[2] = '3'; chroma[3] = '2';
		//chroma[0] = 'R'; chroma[1] = 'G'; chroma[2] = 'B'; chroma[3] = 'A';

		ctx.chroma[0] = chroma[0];
		ctx.chroma[1] = chroma[1];
		ctx.chroma[2] = chroma[2];
		ctx.chroma[3] = chroma[3];
		//ctx.width = w;
		//ctx.height = h;

		ctx.pitches[0] = w * 4;
		ctx.pitches[1] = 0;
		ctx.pitches[2] = 0;

		ctx.lines[0] = h;
		ctx.lines[1] = 0;
		ctx.lines[2] = 0;

		SAFE_DELETE_PTR(q);
		q = new CFrameQueue(ctx);

		// set parameters
		pitches[0] = ctx.pitches[0];
		lines[0] = ctx.lines[0];

		return 1;
	}


	static unsigned video_format_cb24(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
	{
		//Furthermore, we recommend that pitches and lines be multiple of 32
		//to not break assumption that might be made by various optimizations
		// in the video decoders, video filters and / or video converters.

		assert(*width);
		assert(*height);
		unsigned w = *width;
		unsigned h = *height;

		//CFrameMemory &ctx = **(CFrameMemory**)opaque;
		CFrameMemory ctx;
		CFrameQueue *&q = **(CFrameQueue***)opaque;

		chroma[0] = 'R'; chroma[1] = 'V'; chroma[2] = '2'; chroma[3] = '4';
		//chroma[0] = 'R'; chroma[1] = 'V'; chroma[2] = '3'; chroma[3] = '2';
		//chroma[0] = 'R'; chroma[1] = 'G'; chroma[2] = 'B'; chroma[3] = 'A';


		ctx.chroma[0] = chroma[0];
		ctx.chroma[1] = chroma[1];
		ctx.chroma[2] = chroma[2];
		ctx.chroma[3] = chroma[3];
		//ctx.width = w;
		//ctx.height = h;


		ctx.pitches[0] = w * 3;
		ctx.pitches[1] = 0;
		ctx.pitches[2] = 0;

		ctx.lines[0] = h;
		ctx.lines[1] = 0;
		ctx.lines[2] = 0;

		SAFE_DELETE_PTR(q);
		q = new CFrameQueue(ctx);

		// set parameters
		pitches[0] = ctx.pitches[0];
		lines[0] = ctx.lines[0];

		return 1;
	}

	static unsigned video_format_cb420(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
	{
		CHECKSCOPE_C(*opaque);

		{  // only for j420 || i420
			char c0 = tolower(chroma[0]);
			if ((c0 != 'j' && c0 != 'i') || chroma[1] != '4' || chroma[2] != '2' || chroma[3] != '0')
				return 0;
		}

		/*ChromaType.J420:
		BitsPerPixel = 12;
		Planes = 3;
		PlaneSizes[0] = Width * Height;
		PlaneSizes[1] = PlaneSizes[2] = Width * Height / 4;
		Pitches = new int[3]{ Width, Width / 2, Width / 2 };
		Lines = new int[3]{ Height, Height / 2, Height / 2 };
		ImageSize = PlaneSizes[0] + PlaneSizes[1] + PlaneSizes[2];*/

		assert(*width);
		assert(*height);
		unsigned w = *width;
		unsigned h = *height;

		//CFrameMemory &ctx = **(CFrameMemory**)opaque;		
		CFrameMemory ctx;
		CFrameQueue *&q = **(CFrameQueue***)opaque;

		ctx.chroma[0] = chroma[0];
		ctx.chroma[1] = chroma[1];
		ctx.chroma[2] = chroma[2];
		ctx.chroma[3] = chroma[3];
		//ctx.width = w;
		//ctx.height = h;


		ctx.pitches[0] = w;
		ctx.pitches[1] = w / 2;
		ctx.pitches[2] = w / 2;

		ctx.lines[0] = h;
		ctx.lines[1] = h / 2;
		ctx.lines[2] = h / 2;

		SAFE_DELETE_PTR(q);
		q = new CFrameQueue(ctx);

		// set parameters
		pitches[0] = ctx.pitches[0];
		pitches[1] = ctx.pitches[1];
		pitches[2] = ctx.pitches[2];

		lines[0] = ctx.lines[0];
		lines[1] = ctx.lines[1];
		lines[2] = ctx.lines[2];

		return 3;
	}


	bool CheckValidFourCC(int i_codec)
	{
		_TraceFormat("codec fourcc = 0x%X\n", i_codec);

		//return true;

		if (i_codec == VLC_CODEC_VP8) return true; //VP80 = 0x30385056
		if (i_codec == VLC_CODEC_VORBIS) return true; //vorb = 0x62726f76		

		_TraceFormat("!!!!! unsupported codec\n");
		return false;
	}

	static unsigned video_format_cb(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
	{
		CHECKSCOPE_C(0);

		{	// only allow playing VP80

			libvlc_media_t *pMd = libvlc_media_player_get_media(mp);

			libvlc_media_track_info_t *pInf;
			int iStreams = libvlc_media_get_tracks_info(pMd, &pInf); //return the number of Elementary Streams
			libvlc_media_release(pMd);

			if (iStreams <= 0)
			{
				_TraceFormat("!!!!! get_tracks_info fail! iStreams=%d\n", iStreams);
				s_bInvalidMedia = true;
				return 0;
			}

			for (int i = 0; i < iStreams; i++)
				if (!CheckValidFourCC(pInf[i].i_codec))
				{
					s_bInvalidMedia = true;
					return 0;
				}
		}

		unsigned int ret = bUseUnityTexture ?
			video_format_cb32(opaque, chroma, width, height, pitches, lines) : // allocates buffers by orignal data.
			video_format_cb420(opaque, chroma, width, height, pitches, lines);

		if (ret <= 0)
		{
			s_bInvalidMedia = true;
			return 0;
		}

		CFrameQueue *&q = **(CFrameQueue***)opaque;

		if (1) {
			// VLC bug workaround !
			// decode a 3840x1920 video, we got *width=3840 & *height=1922 !		// 
			// I tested the decode duration between lock_buffer_cb() & unlock_buffer_cb() with "Bicubic (good quality)" scale-mode & RV32.
			// results:
			// *height = 1922; //(or no change)  25ms
			// *height = 1921;  25ms
			// *height = 1920;  11ms
			// *height = 1919;  25ms
			// it could be a vlc bug...		
			// BUT it is no different for J420 ??

			unsigned int vw, vh;
			libvlc_video_get_size(mp, 0, &vw, &vh);

			if (vw*vh)
			{
				*width = vw;
				*height = vh;
			}
			else
			{
				_TraceFormat("!!! warning! , libvlc_video_get_size() fail! use the size form video_format_cb() [%d x %d]", *width, *height);
				if (*height == 1922) *height = 1920; //many 1920 videos will get 1922.
			}


			q->SetWidth(*width);
			q->SetHeight(*height);
		}

		Sleep(66); //let render & script threads create texture resource.
		return ret;
	}


	static void video_cleanup_cb(void *opaque)
	{
		CHECKSCOPE_C(opaque);

		//CFrameMemory &ctx = *(CFrameMemory*)opaque;
		//ctx.release();
	}

	static void* lock(void *opaque, void **planes)
	{
		//CHECKSCOPE_C(opaque);

		CFrameQueue* q = *(CFrameQueue**)opaque;
		assert(q->CheckType());
		CFrameMemory* ctx = q->GetEmptyBuffer();

		//if (!ctx)
		//{	// this way will crash;
		//	planes[0] = 0;
		//	planes[1] = 0;
		//	planes[2] = 0;
		//	return 0;
		//}

		planes[0] = (void*)ctx->buffers[0];
		planes[1] = (void*)ctx->buffers[1];
		planes[2] = (void*)ctx->buffers[2];

		//ctx.lock();

		bPlayed = true;

		//g_decodeTimer.start();
		//StartCounter();

		return ctx;
	}

	static void unlock(void *opaque, void *picture, void *const *planes)
	{
		//CHECKSCOPE_C(opaque);		
	}

	//When the video frame needs to be shown, as determined by the media playback clock, the display callback is invoked.
	static void display(void *opaque, void *picture)
	{
		//CHECKSCOPE_C(opaque);

		CFrameQueue* q = *(CFrameQueue**)opaque;
		assert(q->CheckType());
		CFrameMemory* ctx = (CFrameMemory*)picture;
		q->Push(ctx);
	}

	void setup_yuvTexture()
	{
		CHECKSCOPE_C(0);

		libvlc_video_set_format_callbacks(mp, video_format_cb, video_cleanup_cb);
		libvlc_video_set_callbacks(mp, lock, unlock, display, &s_pContex);

		//[VLC] codec lock time = 12ms/J420; 18ms/rv32; 19ms/rv24 (i7 cpu)
	}

	void setup_unityTexture()
	{
		CHECKSCOPE_C(0);

		libvlc_video_set_format_callbacks(mp, video_format_cb, video_cleanup_cb);
		libvlc_video_set_callbacks(mp, lock, unlock, display, &s_pContex);
	}


	CDirectXResource::CDirectXResource() : pTex0(0), pTex1(0), pTex2(0), pTxSrv0(0), pTxSrv1(0), pTxSrv2(0) //,mTid(0)
	{
		Autolock a(mutex);
		CHECKSCOPE();
	}

	CDirectXResource::~CDirectXResource()
	{
		Autolock a(mutex);
		CHECKSCOPE();
		mRelease();
	}

	void CDirectXResource::CreateTextures(int w, int h)
	{
		Autolock a(mutex);
		mCreateTextures(w, h);
	}

	void CDirectXResource::Release()
	{
		Autolock a(mutex);
		mRelease();
	}

	void CDirectXResource::mRelease()
	{
		CHECKSCOPE();

		SAFE_RELEASE(pTxSrv0);
		SAFE_RELEASE(pTxSrv1);
		SAFE_RELEASE(pTxSrv2);

		SAFE_RELEASE(pTex0);
		SAFE_RELEASE(pTex1);
		SAFE_RELEASE(pTex2);
	}

	void CDirectXResource::mCreateTextures(int w, int h)
	{
		CHECKSCOPE();
		//mTid = GetCurrentThreadId();

		mRelease();
		pTxSrv0 = createD3DTextureSRV(w, h, &pTex0);
		pTxSrv1 = createD3DTextureSRV(w / 2, h / 2, &pTex1);
		pTxSrv2 = createD3DTextureSRV(w / 2, h / 2, &pTex2);
	}

	inline bool CFrameMemory::CheckType() { return mTypeInfo == typeid(*this); /*it == typeid(CFrameQueue);*/ }

	inline UINT CFrameMemory::multiple_of_32(UINT x)
	{
		return (x + 31) & (~0x1f);
	}

	void CFrameMemory::freeBuffers()
	{
		//CHECKSCOPE();

		//bNewData = false;
		SAFE_FREE_PTR(buffers[0]);
		SAFE_FREE_PTR(buffers[1]);
		SAFE_FREE_PTR(buffers[2]);
		//SAFE_FREE_PTR(tempBuffer);			
	}

	void CFrameMemory::init()
	{
		//CHECKSCOPE();

		memset(pitches, 0, sizeof(pitches));
		memset(lines, 0, sizeof(lines));
		memset(chroma, 0, sizeof(chroma));

		//width = 0;
		//height = 0;
		//bNewData = false;
	}

	void CFrameMemory::lock()
	{
		//CHECKSCOPE();
		mutex.lock();
	}

	void CFrameMemory::unlock()
	{
		//CHECKSCOPE();
		mutex.unlock();
	}

	CFrameMemory::CFrameMemory() : mTypeInfo(typeid(*this))
	{
		CHECKSCOPE();

		memset(buffers, 0, sizeof(buffers));
		//tempBuffer = 0;
		init();
	}

	CFrameMemory::CFrameMemory(const CFrameMemory & o) : mTypeInfo(typeid(*this))
	{
		CHECKSCOPE();

		memset(buffers, 0, sizeof(buffers));
		init();

		memcpy(pitches, o.pitches, sizeof(pitches));
		memcpy(lines, o.lines, sizeof(lines));
		memcpy(chroma, o.chroma, sizeof(chroma));
		//width=o.width;
		//height=o.height;			
	}

	CFrameMemory::~CFrameMemory()
	{
		CHECKSCOPE();

		lock();
		freeBuffers();
		unlock();
	}

	void CFrameMemory::release()
	{
		CHECKSCOPE();

		lock();
		freeBuffers();
		init();
		unlock();
	}

	void CFrameMemory::allocBuffers() // alloc planes buffers by lines & pitches, pitches & lines might be changed
	{
		CHECKSCOPE();

		lock();

		freeBuffers();

		for (int i = 0; i < 3; i++)
		{
			//Furthermore, we recommend that pitches and lines be multiple of 32
			//to not break assumption that might be made by various optimizations
			// in the video decoders, video filters and / or video converters.

			lines[i] = multiple_of_32(lines[i]);
			pitches[i] = multiple_of_32(pitches[i]);

			UINT sz = lines[i] * pitches[i];
			if (sz)
				buffers[i] = (UINT8*)malloc(sz);
		}

		//tempBuffer = (UINT8*)malloc(width * height * 4);

		unlock();
	}

	inline bool CFrameQueue::CheckType() { return mTypeInfo == typeid(*this); /*it == typeid(CFrameQueue);*/ }

	inline void CFrameQueue::Lock() { mutex.lock(); }

	inline void CFrameQueue::Unlock() { mutex.unlock(); }

	int CFrameQueue::GetWidth() {
		Autolock a(mutex);
		return mWidth;
	}

	int CFrameQueue::GetHeight()
	{
		Autolock a(mutex);
		return mHeight;
	}

	void CFrameQueue::SetWidth(int v)
	{
		Autolock a(mutex);
		assert(v > 0);
		mWidth = v;
	}

	void CFrameQueue::SetHeight(int v)
	{
		Autolock a(mutex);
		assert(v > 0);
		mHeight = v;
	}

	CFrameQueue::CFrameQueue(CFrameMemory & prototype) : mPrototype(prototype), mTypeInfo(typeid(*this)), mWidth(0), mHeight(0) {}

	//CFrameQueue(CFrameMemory& prototype) : mPrototype(prototype) {}

	void CFrameQueue::LogStatus()
	{
		Autolock a(mutex);
		_TraceFormat("(in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size(), __X_FUNCTION__);
	}

	CFrameQueue::~CFrameQueue()
	{
		Autolock a(mutex);
		_TraceFormat("(in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size(), __X_FUNCTION__);

		while (!mAll.empty())
		{
			CFrameMemory* p = mAll.front();
			mAll.pop();
			delete p;
		}
	}

	CFrameMemory * CFrameQueue::GetEmptyBuffer()
	{
		Autolock a(mutex);

		if (mRecycle.empty())
		{
			if (mAll.size() > 3)
			{
				//_TraceFormat("!!!buffer full!!! (in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size(), __X_FUNCTION__); return 0;
				if (mQueue.empty())
				{
					_TraceFormat("!!!!!unknown Error! mQueue should not be empty!! (in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size(), __X_FUNCTION__);
					return 0;
				}

				// return buffer from mQueue;
				CFrameMemory* p = mQueue.front();
				mQueue.pop();
				return p;
			}

			CFrameMemory* p = new CFrameMemory(mPrototype);
			p->allocBuffers();
			mAll.push(p);
			return p;
		}
		else
		{
			CFrameMemory* p = mRecycle.front();
			mRecycle.pop();
			return p;
		}
	}

	void CFrameQueue::Push(CFrameMemory * p)
	{
		Autolock a(mutex);
		//_TraceFormat("(in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size() ,__X_FUNCTION__);

		if (p)
			mQueue.push(p);
	}

	CFrameMemory * CFrameQueue::Pop()
	{
		Autolock a(mutex);

		if (mQueue.empty()) return 0;

		//_TraceFormat("(in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size(), __X_FUNCTION__);
		CFrameMemory* p = mQueue.front();
		mQueue.pop();
		return p;
	}

	void CFrameQueue::Recycle(CFrameMemory * p)
	{
		Autolock a(mutex);
		//_TraceFormat("(in-queue : recycle : all) (%d : %d : %d) (%s)\n", mQueue.size(), mRecycle.size(), mAll.size() ,__X_FUNCTION__);

		if (p)
			mRecycle.push(p);
	}

} // namespace

using namespace vlcVideoTexture;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API DebugWin(const char* message)
{
	_TraceFormat("c# : %s\n", message);
	//DebugLog(message);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* texturePtr)
{
	CHECKSCOPE_C(0);

	// A script calls this at initialization time; just remember the texture pointer here.
	// Will update texture pixels each frame from the plugin rendering event (texture update
	// needs to happen on the rendering thread).
	g_TexturePointer = texturePtr;
}

// GraphicsDeviceEvent
// Actual setup/teardown functions defined below
#if SUPPORT_D3D11
static void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType);
#endif
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	CHECKSCOPE_C(0);

	UnityGfxRenderer currentDeviceType = s_DeviceType;
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		_TraceFormat("OnGraphicsDeviceEvent(Initialize).\n");
		s_DeviceType = s_Graphics->GetRenderer();
		currentDeviceType = s_DeviceType;
		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		_TraceFormat("OnGraphicsDeviceEvent(Shutdown).\n");
		s_DeviceType = kUnityGfxRendererNull;
		g_TexturePointer = NULL;
		break;
	}
	case kUnityGfxDeviceEventBeforeReset:
	{
		_TraceFormat("OnGraphicsDeviceEvent(BeforeReset).\n");
		break;
	}
	case kUnityGfxDeviceEventAfterReset:
	{
		_TraceFormat("OnGraphicsDeviceEvent(AfterReset).\n");
		break;
	}
	};
#if SUPPORT_D3D11
	if (currentDeviceType == kUnityGfxRendererD3D11)
		DoEventGraphicsDeviceD3D11(eventType);
#endif
}

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	CHECKPOINT_C(0);

	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	CHECKPOINT_C(0);
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

double GetPerformanceTime()
{
	LARGE_INTEGER li;

	static double s_freq = 0;
	if (s_freq == 0) // assume the freq will not be changed.
	{
		QueryPerformanceFrequency(&li);
		s_freq = li.QuadPart / 1000.0;
	}

	QueryPerformanceCounter(&li);
	return (li.QuadPart) / s_freq;
}

// +++++++++++++++++ Performance log ++++++++++++++++++
double PCFreq = 0.0;
__int64 CounterStart = 0;
bool bCounterStarted = false;
void StartCounter()
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		_TraceFormat("QueryPerformanceFrequency failed!\n");
	PCFreq = double(li.QuadPart) / 1000.0;
	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}
double GetCounter()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}

void StartCounter_CPU()
{
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		_TraceFormat("QueryPerformanceFrequency failed!\n");
	PCFreq = double(li.QuadPart) / 1000.0;
	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}
double GetCounter_CPU()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}
//---------------Performance log----------------

void setup()
{
	if (bUseUnityTexture) vlcVideoTexture::setup_unityTexture();
	else vlcVideoTexture::setup_yuvTexture();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitPlayer(const char* path, bool bUseUnityTexture)
{
	Autolock a(s_mutex);

	//path = "http://10.116.77.155:1935/vod/mp4:dance.mp4/playlist.m3u8";

	if (0) // for test
	{
		static int idxTest = 0;

		switch (2)
			//switch (idxTest++%4)
		{
		case 0:
			path = "http://52.77.210.23:1935/vod/mp4:BigMouth360.mp4/playlist.m3u8";
			break;
		case 1:
			path = "http://10.116.77.155:1935/vod/mp4:dance.mp4/playlist.m3u8";
			break;
		case 2:
			path = "d:\\test2.mp4";
			break;
		case 3:
			path = "c:\\360Videos\\test2.mp4";
			break;
		default:
			break;
		}
	}


	_TraceFormat("%s : (%s) (%d)\n", __X_FUNCTION__, path, bUseUnityTexture);

	::s_bInvalidMedia = false;
	::bUseUnityTexture = bUseUnityTexture;

	if (binitmp == true)
	{
		_TraceFormat("!!!InitPlayer: binitmp == true");
		return;
	}

	binitmp = true;

	std::string videoPath = path;

	libvlc_media_t *m;
	/* Load the VLC engine */
#if 0
	const char *const vlc_args[] = {
		"-I",
		"dummy",
		"--ignore-config",
		"--extraintf=logger",
		"--verbose=2",
		"--high-priority",
		"--avcodec-hw=any",
	};
#elif 0
	const char *const vlc_args[] = {
		"-I",
		"dummy",
		"--high-priority",
		"--avcodec-hw=any",
	};

#else 
	const char *const vlc_args[] = {
		"--swscale-mode=4" //Nearest neighbour
		//,"--swscale-mode=2" //Bicubic
		,"--no-autoscale"
		//,"--extraintf=logger"
		,"--verbose=2"
	};
#endif

	int vlc_argc = sizeof(vlc_args) / sizeof(vlc_args[0]);
	inst = libvlc_new(vlc_argc, vlc_args);
	//inst = libvlc_new(0, NULL);
	if (inst == NULL) {
		_TraceFormat("Fail to create vlc instance!");
	}
	if (videoPath.empty())
		m = libvlc_media_new_path(inst, "HenryShort.mp4");
	else {
		if (videoPath.find("http") == 0)
			m = libvlc_media_new_location(inst, videoPath.c_str());
		else
			m = libvlc_media_new_path(inst, videoPath.c_str());
	}

	if (m == 0)
	{
		_TraceFormat("!!!! create libvlc_media fail! (%s)\n", path);
		return;
	}

	/* Create a media player playing environement */
	mp = libvlc_media_player_new_from_media(m);

	{
		libvlc_media_track_info_t *pInf;
		int iStreams = libvlc_media_get_tracks_info(m, &pInf); //return the number of Elementary Streams
		for (int i = 0; i < iStreams; i++)
			_TraceFormat("media fourcc = 0x%X\n", pInf[i].i_codec);
	}

	/* No need to keep the media now */
	libvlc_media_release(m);

	//libvlc_video_set_format(mp, "RV24", width, height, width * BytePerPixel);
	::setup();

	bPlayed = false;
	int r = libvlc_media_player_play(mp);
	if (r) _TraceFormat("!!!!! %s: libvlc_media_player_play() returns error %xX !!!!!\n", __X_FUNCTION__, r);

	//Sleep(200);	
}

//Playback operation
//....
//Video time
extern "C" long long UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetVideoLength() {
	if (mp) {
		return libvlc_media_player_get_length(mp);
	}
	return -1;
}
extern "C" long long UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetVideoCurrentTime() {
	if (mp) {
		return libvlc_media_player_get_time(mp);
	}
	return -1;
}
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetVideoCurrentTime(long long set_time) {
	if (mp) {
		libvlc_media_player_set_time(mp, set_time);
	}
}

extern "C" UINT UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetVideoWidth()
{
	if (!bUseUnityTexture)
	{
		if (s_dxRc.pTxSrv0 == 0 || s_dxRc.pTxSrv1 == 0 || s_dxRc.pTxSrv2 == 0)
			return 0;
	}

	int x = 0;
	if (s_pContex)
	{
		x = s_pContex->GetWidth();
		_TraceFormat("%s ret %d\n", __X_FUNCTION__, x);
	}

	return x;
}


extern "C" UINT UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetVideoHeight()
{
	if (!bUseUnityTexture)
	{
		if (s_dxRc.pTxSrv0 == 0 || s_dxRc.pTxSrv1 == 0 || s_dxRc.pTxSrv2 == 0)
			return 0;
	}

	int x = 0;
	if (s_pContex)
	{
		x = s_pContex->GetHeight();
		_TraceFormat("%s ret %d\n", __X_FUNCTION__, x);
	}

	return x;
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleasePlayer()
{
	Autolock a(s_mutex);

	CHECKSCOPE_C(0);
	if (binitmp == true)
	{
		if (mp)
		{
			bool bReleasPlayer = false;
			{
				libvlc_state_t st;
				for (int i = 0; i < 10; i++)
				{
					st = libvlc_media_player_get_state(mp);
					if (st != libvlc_Opening && st != libvlc_Buffering)
					{
						bReleasPlayer = true;
						break;
					}

					Sleep(333);
				}
				int wp = libvlc_media_player_will_play(mp);
				int playing = libvlc_media_player_is_playing(mp);
				_TraceFormat("will_play = %d, playing = %d, state = %d\n", wp, playing, st);
			}

			if (bReleasPlayer)
			{
				CHECKPOINT_C(0);
				libvlc_media_player_stop(mp); // It will Crash!!! if playing streamming video from network is finished.
				CHECKPOINT_C(0);
				libvlc_media_player_release(mp); // It will Crash!!! if playing streamming video from network is finished.
			}
			else _TraceFormat("!!! Skip releasing player !!!\n");
		}
		CHECKPOINT_C(0);
		if (inst) libvlc_release(inst);
		CHECKPOINT_C(0);
		SAFE_DELETE_PTR(s_pContex);
		CHECKPOINT_C(0);
		s_dxRc.Release();

		binitmp = false;
		mp = 0;
		inst = 0;
	}
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API IsVideoStarted()
{
	return bPlayed;
}


// for yuv420
static void UpdateTexture()
{
	//CHECKSCOPE_C(0);
	//CHECKSCOPETIME_C(0);

	if (!s_pContex) return;
	CFrameMemory* ctx = s_pContex->Pop();
	if (!ctx) return;

	CDirectXResource& dxr = s_dxRc;
	if (dxr.pTex0 == 0)
	{
		s_pContex->Recycle(ctx);
		return;
	}

#if SUPPORT_D3D11
	// D3D11 case
	if (s_DeviceType == kUnityGfxRendererD3D11)
	{
		//CHECKSCOPE_C(0);
		//if(s_pContex) s_pContex->LogStatus();

		ID3D11DeviceContext* dc = NULL;
		g_D3D11Device->GetImmediateContext(&dc);

		//ctx.lock();
		dc->UpdateSubresource(dxr.pTex0, 0, NULL, ctx->buffers[0], ctx->pitches[0], 0);
		dc->UpdateSubresource(dxr.pTex1, 0, NULL, ctx->buffers[1], ctx->pitches[1], 0);
		dc->UpdateSubresource(dxr.pTex2, 0, NULL, ctx->buffers[2], ctx->pitches[2], 0);
		//ctx.unlock();		
		dc->Release();
	}
	else
	{
		_TraceFormat("!!!!! %s: unsupported s_DeviceType = %xX !!!!!\n", __X_FUNCTION__, s_DeviceType);
	}


#endif
	s_pContex->Recycle(ctx);
}

// for Unity texture RV32
static void UpdateUnityTexture()
{
	//CHECKSCOPE_C(0);
	//CHECKSCOPETIME_C(0);

	if (!s_pContex) return;
	CFrameMemory* ctx = s_pContex->Pop();
	if (!ctx) return;
	if (g_TexturePointer == 0) return;

	//ctx.bNewData = false;

	//if(playerctx.width);

#if SUPPORT_D3D11
	// D3D11 case	
	if (s_DeviceType == kUnityGfxRendererD3D11)
	{
		//CHECKSCOPE_C(0);
		//if(s_pContex) s_pContex->LogStatus();

		ID3D11DeviceContext* dc = NULL;
		g_D3D11Device->GetImmediateContext(&dc);

		ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)g_TexturePointer;
		D3D11_TEXTURE2D_DESC desc;
		d3dtex->GetDesc(&desc);

		//UINT w = ctx.pitches[0];
		//UINT h = ctx.lines[0];

		assert(desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
		//assert(desc.Width == ctx.pitches[0]);
		//assert(desc.Height == h);

		//ctx.lock();
		dc->UpdateSubresource(d3dtex, 0, NULL, ctx->buffers[0], ctx->pitches[0], 0);
		//ctx.unlock();				
		dc->Release();
		s_pContex->Recycle(ctx);
	}
#endif	
}


#define CHECKD3DDEVICE()
//void CHECKD3DDEVICE()
//{
//	IUnityGraphicsD3D11* d3d11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
//	ID3D11Device* D3D11Device = d3d11->GetDevice();
//	if (g_D3D11Device != D3D11Device)
//	{
//		_TraceFormat("!!!!! XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX different D3D11Device %p and %p \n", g_D3D11Device, D3D11Device);
//	}
//}

ID3D11ShaderResourceView* createD3DTextureSRV(UINT w, UINT h, ID3D11Texture2D** ppTex)
{
	_TraceFormat("%s: %d x %d (ppTex=%p) (d3ddevice state=%xx )\n", __X_FUNCTION__, w, h, ppTex, g_D3D11Device->GetDeviceRemovedReason());
	//CHECKSCOPE_C(0);

	if (g_D3D11Device == 0)
	{
		WARNPOINT();
		return 0;
	}

	//Sleep(1000);

	HRESULT hr;

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;// D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	ID3D11Texture2D* tex = nullptr;
	{
		CHECKSCOPE_C(0);
		CHECKD3DDEVICE();
		hr = g_D3D11Device->CreateTexture2D(&desc, 0, &tex);
	}
	if (FAILED(hr))
	{
		_TraceFormat("!!!!! CreateTexture2D error(%s:%d) (hr = %x / %x)\n", __X_FUNCTION__, __LINE__, hr, g_D3D11Device->GetDeviceRemovedReason());
		if (tex)	tex->Release();
		*ppTex = 0;
		return 0;
	}

	ID3D11ShaderResourceView *pTextureView;

	{
		CHECKSCOPE_C(0);
		CHECKD3DDEVICE();
		hr = g_D3D11Device->CreateShaderResourceView(tex, 0, &pTextureView);
	}
	if (FAILED(hr))
	{
		//DXGI_ERROR_DEVICE_HUNG
		_TraceFormat("!!!!! CreateShaderResourceView error(%s:%d) (hr = %x / %x)\n", __X_FUNCTION__, __LINE__, hr, g_D3D11Device->GetDeviceRemovedReason());
		if (pTextureView)	pTextureView->Release();
		if (tex)	tex->Release();
		*ppTex = 0;
		return 0;
	}

	*ppTex = tex;
	return pTextureView;

}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API nativeCreateTexture(void** tex0, void** tex1, void** tex2)
{
	CHECKSCOPE_C(0);
	Autolock a(s_mutex);

	CDirectXResource& dxr = s_dxRc;
	*tex0 = dxr.pTxSrv0;
	*tex1 = dxr.pTxSrv1;
	*tex2 = dxr.pTxSrv2;
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.
static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	Autolock a(s_mutex);
	//CHECKSCOPE_C(eventID);

	if (s_pContex == 0) return;

	{
		static int s_count = 0;
		static double s_start = 0;

		s_count++;
		double now = GetPerformanceTime();
		double duration = now - s_start;
		if (duration > 4000.0)
		{
			_TraceFormat("fps = %4.2f dur=%f \n", s_count*1000.0 / duration, duration);
			s_pContex->LogStatus();
			s_start = now;
			s_count = 0;
		}
	}

	// Unknown graphics device type? Do nothing.
	if (s_DeviceType == kUnityGfxRendererNull) return;

	if (!bUseUnityTexture)
	{
		CDirectXResource& dxr = s_dxRc;
		if (dxr.pTxSrv0 == NULL)
		{
			int w = s_pContex->GetWidth();
			int h = s_pContex->GetHeight();
			if (w > 0 && h > 0)
				dxr.CreateTextures(s_pContex->GetWidth(), s_pContex->GetHeight());
		}
	}

	if (eventID == 0) return; //bTextureCreated


	if (bUseUnityTexture) UpdateUnityTexture();
	else UpdateTexture();
}
// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API nativeCallPluginAtEndOfFrames() {

	if (s_bInvalidMedia)
	{
		_TraceFormat("!!!!! InvalidMedia! release player \n");
		ReleasePlayer();
		s_bInvalidMedia = false;
	}


}

#if SUPPORT_D3D11
static void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType)
{
	CHECKSCOPE_C(0);

	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		IUnityGraphicsD3D11* d3d11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
		g_D3D11Device = d3d11->GetDevice();

		if (g_D3D11Device)
		{
			D3D11_FEATURE_DATA_THREADING x;
			HRESULT hr = g_D3D11Device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &x, sizeof(x));
			_TraceFormat("D3D11_FEATURE_THREADING(%d), DriverConcurrentCreates=%d, DriverCommandLists=%d\n", hr, x.DriverConcurrentCreates, x.DriverCommandLists);

		}
		else
		{
			_TraceFormat("!!!!!d3d11->GetDevice() return 0!\n");
		}
	}
	else if (eventType == kUnityGfxDeviceEventShutdown)
	{
		//Release resource
		s_dxRc.Release();
	}
}
#endif // #if SUPPORT_D3D11


//must sync with script side
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API nativeVideoPlayerSendControl(int eControl)
{
	if (!mp)
	{
		_TraceFormat("%s: libvlc_media_player = 0! (ctrl=%d)\n", __X_FUNCTION__, eControl);
		return false;
	}


	enum EPlayControl
	{
		Play,
		Stop,
		TogglePause,
		Forward,
		Backward,
		SpeedUp,
		SpeedDown,
		SpeedNormal
	};

	int r = 0;

	switch (eControl)
	{
	case Play:
	{
		r = libvlc_media_player_play(mp);
		break;
	}

	case Stop:
	{
		libvlc_media_player_stop(mp);
		break;
	}

	case TogglePause:
	{
		libvlc_media_player_pause(mp);
		break;
	}

	case Forward:
	{
		libvlc_time_t t = libvlc_media_player_get_time(mp); //in ms
		_TraceFormat("play time = %3.2fs\n", t / 1000.0f);
		if (t >= 0)
			libvlc_media_player_set_time(mp, t + 300);
		break;
	}

	case Backward:
	{
		libvlc_time_t t = libvlc_media_player_get_time(mp); //in ms
		_TraceFormat("play time = %3.2fs\n", t / 1000.0f);
		if (t > 0)
			libvlc_media_player_set_time(mp, t - 300);
		break;
	}

	case SpeedUp:
	{
		float rate = libvlc_media_player_get_rate(mp) + 0.3f;
		r = libvlc_media_player_set_rate(mp, rate);
		_TraceFormat("play speed rate = %3.2f\n", rate);
		break;
	}

	case SpeedDown:
	{
		float rate = libvlc_media_player_get_rate(mp) - 0.3f;
		r = libvlc_media_player_set_rate(mp, rate);
		_TraceFormat("play speed rate = %3.2f\n", rate);
		break;
	}

	case SpeedNormal:
	{
		r = libvlc_media_player_set_rate(mp, 1.0f);
		break;
	}
	}

	if (r) _TraceFormat("!!!!! %s: libvlc_media_player_XXX() returns error %xX , ctrl=%d!!!!!\n", __X_FUNCTION__, r, eControl);
	return !r;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API nativeVideoPlayerGetState()
{
	if (!mp) return 0;
	return libvlc_media_player_get_state(mp);
}
