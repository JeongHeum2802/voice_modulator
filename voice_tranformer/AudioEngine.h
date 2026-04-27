#pragma once
#include <mmdeviceapi.h>
#include <atomic> // 추가: 원자적 변수 사용을 위해 필요
#include "AudioProcessor.h"

class AudioEngine {
public:
    // 4번째 인자로 std::atomic<bool>의 포인터를 받도록 추가합니다.
    void Run(IMMDevice* pCaptureDevice, IMMDevice* pRenderDevice, IAudioProcessor* pProcessor, std::atomic<bool>* pIsRunning);
};