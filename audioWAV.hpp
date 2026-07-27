#include <iostream>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>

#define SAFE_RELEASE(punk) \
    if ((punk) != nullptr) \
    {                      \
        (punk)->Release(); \
        (punk) = nullptr;  \
    }

class AudioRecorder
{
private:
	HRESULT hr;

	UINT32 frames = 0;
	BYTE* pData = nullptr;
	IMMDeviceEnumerator* pEnumerator = nullptr;
	IMMDevice* pDevice = nullptr;
	IAudioClient* pAudioClient = nullptr;
	WAVEFORMATEX* pwfx = nullptr;
	IAudioCaptureClient* pCapture = nullptr;
public:
	int audioInitToRecord()
	{
		hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

		if (FAILED(hr)) { std::cout << "HRESULT = 0x%08X\n" << hr; return 1; }

		pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
		pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
		pAudioClient->GetMixFormat(&pwfx);

		pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, nullptr);
		pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCapture);

	}

	

	BYTE* audioRecordReturn() // Do in cycle, can be sent by tcp client, call releasePData() after every use
	{
		UINT32 bytes = 0;
		DWORD flags = 0;
		UINT32 packetLenght{ 0 };

	packetSize:
		pCapture->GetNextPacketSize(&packetLenght);

		if (packetLenght == 0)
		{
			Sleep(5);
			goto packetSize;
		}
		HRESULT hr = pCapture->GetBuffer(&pData, &frames, &flags, nullptr, nullptr);

		bytes = frames * pwfx->nBlockAlign;

		return pData;
	}

	void releasePData() { pCapture->ReleaseBuffer(frames); }

	

	void shutdownAudio()
	{
		SAFE_RELEASE(pAudioClient);
		SAFE_RELEASE(pDevice);
		SAFE_RELEASE(pEnumerator);

		CoUninitialize();
	}
};

class AudioPlayer
{
private:
	HRESULT hr;
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioRenderClient* pRenderClient = NULL;
	WAVEFORMATEX* pwfx = NULL;
	UINT32 bufferFrames;
public:
	int audioInitToPlay()
	{
		HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hr)) return -1;

		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
		if (FAILED(hr)) { return -2; }

		pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);

		pAudioClient->GetMixFormat(&pwfx);

		hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pwfx, nullptr);
		if (FAILED(hr)) { return -3; }

		pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);

		pAudioClient->GetBufferSize(&bufferFrames);

		return 0;
	}

	void startPlay() //  use audioPlayer() after this
	{
		pAudioClient->Start();
	}

	int audioPlayer(BYTE PCMData[], size_t size)
	{ 
		bool fullPlayed = false;

		UINT32 padding;
		pAudioClient->GetCurrentPadding(&padding);
		UINT32 freeFrames = bufferFrames - padding;
		UINT32 freeBytes = freeFrames * pwfx->nBlockAlign;
		size_t bytesCopied = NULL;
		BYTE* pData = nullptr;

		hr = pRenderClient->GetBuffer(freeFrames, &pData);

		if (freeFrames == 0)
		{
			return 1;
		}

		if (size < freeBytes)
		{
			memcpy(pData, PCMData, size);
			pRenderClient->ReleaseBuffer(size / pwfx->nBlockAlign, NULL);
			return 0;
		}
			
		while (bytesCopied < size)
		{
			pAudioClient->GetCurrentPadding(&padding);
			UINT32 freeFrames = bufferFrames - padding;
			UINT32 freeBytes = freeFrames * pwfx->nBlockAlign;

			hr = pRenderClient->GetBuffer(freeFrames, &pData);
			std::cout << hr << std::endl;

			if (freeFrames == 0)
			{
				Sleep(5);
				continue;
			}

			if (size - bytesCopied < freeBytes)
			{
				memcpy(pData, PCMData + (size - bytesCopied), freeBytes);
				pRenderClient->ReleaseBuffer(freeFrames, NULL);
				break;
			}

			memcpy(pData, PCMData + bytesCopied, freeBytes); // тут исключение
			bytesCopied += freeBytes;
			pRenderClient->ReleaseBuffer(freeFrames, NULL);
		}
		return 0;
	}
};

struct WAVHeader
{
	char riff[4] = { 'R','I','F','F' };
	uint32_t fileSize;

	char wave[4] = { 'W','A','V','E' };

	char fmt[4] = { 'f','m','t',' ' };
	uint32_t fmtSize = 16;

	uint16_t audioFormat = 3;
	uint16_t channels;
	uint32_t sampleRate;
	uint32_t byteRate;
	uint16_t blockAlign;
	uint16_t bitsPerSample;

	char data[4] = { 'd','a','t','a' };
	uint32_t dataSize;
};
