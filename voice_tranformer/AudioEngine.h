#pragma once
#include <mmdeviceapi.h>
#include "AudioProcessor.h" // 프로세서 인터페이스 필요

class AudioEngine {
public:
    // 엔진 시작 시 장치와 처리기를 넘겨받습니다.
    void Run(IMMDevice* pCaptureDevice, IMMDevice* pRenderDevice, IAudioProcessor* pProcessor);
};