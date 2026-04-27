#include "AudioEngine.h"
#include <Audioclient.h>
#include <iostream>
#include <conio.h>
#include <wrl/client.h>`

using Microsoft::WRL::ComPtr;

void AudioEngine::Run(IMMDevice* pCaptureDevice, IMMDevice* pRenderDevice, IAudioProcessor* pProcessor, std::atomic<bool>* pIsRunning) {
    ComPtr<IAudioClient> pCaptureClient, pRenderClient;
    pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &pCaptureClient);
    pRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &pRenderClient);

    // 1. 마이크(1채널) 포맷 세팅 (안전한 계산 방식으로 복구)
    WAVEFORMATEX wfxCap = {};
    wfxCap.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfxCap.nChannels = 1;
    wfxCap.nSamplesPerSec = 48000;
    wfxCap.wBitsPerSample = 32;
    wfxCap.nBlockAlign = (wfxCap.nChannels * wfxCap.wBitsPerSample) / 8; // 4
    wfxCap.nAvgBytesPerSec = wfxCap.nSamplesPerSec * wfxCap.nBlockAlign; // 192,000

    // 2. 스피커(2채널) 포맷 세팅
    WAVEFORMATEX wfxRen = {};
    wfxRen.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfxRen.nChannels = 2;
    wfxRen.nSamplesPerSec = 48000;
    wfxRen.wBitsPerSample = 32;
    wfxRen.nBlockAlign = (wfxRen.nChannels * wfxRen.wBitsPerSample) / 8; // 8
    wfxRen.nAvgBytesPerSec = wfxRen.nSamplesPerSec * wfxRen.nBlockAlign; // 384,000

    // 3. 초기화 및 예외 처리 (방어 코드 추가)
    HRESULT hrCap = pCaptureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, &wfxCap, nullptr);
    if (FAILED(hrCap)) {
        std::wcerr << L"[에러] 마이크 초기화 실패! 에러 코드: " << std::hex << hrCap << std::endl;
        return; // 프로그램 강제 종료 방지
    }

    HRESULT hrRen = pRenderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, &wfxRen, nullptr);
    if (FAILED(hrRen)) {
        std::wcerr << L"[에러] 스피커 초기화 실패! 에러 코드: " << std::hex << hrRen << std::endl;
        return;
    }

    // 4. 서비스 획득 및 예외 처리
    ComPtr<IAudioCaptureClient> pCaptureService;
    ComPtr<IAudioRenderClient> pRenderService;

    if (FAILED(pCaptureClient->GetService(__uuidof(IAudioCaptureClient), &pCaptureService)) ||
        FAILED(pRenderClient->GetService(__uuidof(IAudioRenderClient), &pRenderService))) {
        std::wcerr << L"[에러] 오디오 서비스 획득 실패!" << std::endl;
        return;
    }

    // 5. 실시간 I/O 무한 루프
    pCaptureClient->Start();
    pRenderClient->Start();

    while (*pIsRunning) {
        Sleep(5);
        UINT32 packetLength = 0;
        pCaptureService->GetNextPacketSize(&packetLength);

        while (packetLength != 0) {
            BYTE* pCaptureData;
            BYTE* pRenderData;
            UINT32 numFrames;
            DWORD flags;

            pCaptureService->GetBuffer(&pCaptureData, &numFrames, &flags, nullptr, nullptr);
            pRenderService->GetBuffer(numFrames, &pRenderData);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                memset(pRenderData, 0, numFrames * wfxRen.nBlockAlign);
            }
            else {
                // 핵심: 하드코딩된 함수가 아닌, 주입받은 프로세서의 Process 호출
                if (pProcessor) {
                    pProcessor->Process(pCaptureData, pRenderData, numFrames);
                }
            }

            pRenderService->ReleaseBuffer(numFrames, 0);
            pCaptureService->ReleaseBuffer(numFrames);
            pCaptureService->GetNextPacketSize(&packetLength);
        }
    }

    pCaptureClient->Stop();
    pRenderClient->Stop();
}