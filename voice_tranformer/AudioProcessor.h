#pragma once
#include <windows.h>

// 추후 다양한 변조 기능(DSP, AI)을 얹기 위한 인터페이스
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;

    // 입력 버퍼를 받아 출력 버퍼로 변환/변조하여 넘겨주는 순수 가상 함수
    virtual void Process(BYTE* pInput, BYTE* pOutput, UINT32 numFrames) = 0;
};

// 현재 우리가 사용하는 '32-bit Mono -> Stereo 변환기'
class FormatConverterProcessor : public IAudioProcessor {
public:
    void Process(BYTE* pInMono, BYTE* pOutStereo, UINT32 numFrames) override {
        float* inBuffer = reinterpret_cast<float*>(pInMono);
        float* outBuffer = reinterpret_cast<float*>(pOutStereo);

        for (UINT32 i = 0; i < numFrames; ++i) {
            float sample = inBuffer[i];
            outBuffer[i * 2] = sample;
            outBuffer[i * 2 + 1] = sample;
        }
    }
};