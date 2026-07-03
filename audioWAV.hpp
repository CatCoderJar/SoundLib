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

	int recordSoundInWAVFile(const char path, const char filename, bool whenStop) // Run as async function, put in argument link to bool variable
	{
		WAVHeader header;
		std::ofstream file{ std::ofstream(path + (filename + ".wav"), std::ios::binary)};

		header.audioFormat = 3;
		header.channels = pwfx->nChannels;
		header.sampleRate = pwfx->nSamplesPerSec;
		header.bitsPerSample = pwfx->wBitsPerSample;
		header.blockAlign = pwfx->nBlockAlign;
		header.byteRate = pwfx->nAvgBytesPerSec;

		header.dataSize = 0;
		header.fileSize = 36;

		file.write(reinterpret_cast<const char*>(&header), sizeof(header));

		pAudioClient->Start();

		while (whenStop)
		{ 
			UINT32 packetLenght{ 0 };
			pCapture->GetNextPacketSize(&packetLenght);

			if (packetLenght == 0)
			{
				Sleep(5);
				continue;
			}

			while (packetLenght > 0)
			{
				BYTE* pData = nullptr;
				UINT32 frames = 0;
				UINT32 bytes = 0;
				DWORD flags = 0;

				HRESULT hr = pCapture->GetBuffer(&pData, &frames, &flags, nullptr, nullptr);

				if (FAILED(hr))
				{
					continue;
				}

				bytes = frames * pwfx->nBlockAlign;
				
				file.write(reinterpret_cast<const char*>(pData), bytes);

				header.dataSize += bytes;
				pCapture->ReleaseBuffer(frames);
			}

			file.seekp(0);

			file.write(reinterpret_cast<const char*>(&header), sizeof(header));
		}

		return 0;
	}

	void initializeHeaderBeforeReturnRecord(WAVHeader headerInit)  // put as a link, initialization for header before audioRecordReturn
	{
		headerInit.audioFormat = 1;
		headerInit.channels = pwfx->nChannels;
		headerInit.sampleRate = pwfx->nSamplesPerSec;
		headerInit.bitsPerSample = pwfx->wBitsPerSample;
		headerInit.blockAlign = pwfx->nBlockAlign;
		headerInit.byteRate = pwfx->nAvgBytesPerSec;

		headerInit.dataSize = 0;
		headerInit.fileSize = 36;
	}

	void recordStart() // use this before audioRecordReturn() 
	{
		pAudioClient->Start();
	}

	BYTE* audioRecordReturn(WAVHeader headerArg) // Do in cycle, can be sent by tcp client, put WAVHeader as &(link) in second argument, then make file.seekp(0) and file.write(reinterpret_cast<const char*>(yourHeader), sizeof(yourHeader);
	{
		BYTE* pData = nullptr;
		UINT32 frames = 0;
		UINT32 bytes = 0;
		DWORD flags = 0;

		do
		{
			HRESULT hr = pCapture->GetBuffer(&pData, &frames, &flags, nullptr, nullptr);

			bytes = frames * pwfx->nBlockAlign;

			headerArg.dataSize += bytes;
			pCapture->ReleaseBuffer(frames);
		} while (FAILED(hr));
		return pData;
	}

	BYTE* audioRecordReturn() // Do in cycle, can be sent by tcp client, put WAVHeader as &(link) in second argument, then make file.seekp(0) and file.write(reinterpret_cast<const char*>(yourHeader), sizeof(yourHeader);
	{
		BYTE* pData = nullptr;
		UINT32 frames = 0;
		UINT32 bytes = 0;
		DWORD flags = 0;

		do
		{
			HRESULT hr = pCapture->GetBuffer(&pData, &frames, &flags, nullptr, nullptr);

			bytes = frames * pwfx->nBlockAlign;

			pCapture->ReleaseBuffer(frames);
		} while (FAILED(hr));
		return pData;
	}

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
		pwfx->wFormatTag = 3;
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

	int audioPlayer(BYTE* PCMData, size_t size)
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
			pRenderClient->ReleaseBuffer(size, NULL);
			return 0;
		}

		while (bytesCopied < size)
		{
			pAudioClient->GetCurrentPadding(&padding);
			UINT32 freeFrames = bufferFrames - padding;
			UINT32 freeBytes = freeFrames * pwfx->nBlockAlign;

			if (freeFrames == 0)
			{
				Sleep(5);
				continue;
			}

			memcpy(pData, PCMData + bytesCopied, freeBytes);
			bytesCopied += freeBytes;
			pRenderClient->ReleaseBuffer(size, NULL);
		}
		return 0;
	}
	void stopPlay()
	{
		pAudioClient->Stop();
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
