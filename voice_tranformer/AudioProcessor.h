#pragma once
#include <windows.h>
#include <cmath>
#include <vector>
#include <SoundTouch.h>

using namespace soundtouch;

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

#include <cmath>
#include <algorithm> // std::min, std::max 사용

class NoiseGateProcessor : public IAudioProcessor {
private:
    float m_thresholdLinear;
    float m_currentGain; // 현재 적용 중인 부드러운 볼륨 (0.0 ~ 1.0)

    // 볼륨을 올리고 내리는 속도 (1샘플당 변화량)
    float m_attackStep;
    float m_releaseStep;

public:
    // 생성자: 데시벨, 어택 타임(ms), 릴리즈 타임(ms), 샘플 레이트
    NoiseGateProcessor(float thresholdDb = -35.0f, float attackMs = 10.0f, float releaseMs = 150.0f, int sampleRate = 48000) {
        // 1. 임계치 계산
        m_thresholdLinear = std::pow(10.0f, thresholdDb / 20.0f);
        m_currentGain = 0.0f; // 처음엔 무음 상태로 시작

        // 2. 어택/릴리즈 스텝 계산 (최적화)
        // 지정된 시간(ms) 동안 볼륨이 0에서 1로 도달하기 위해, 1프레임당 더해야 할 값
        float attackSamples = (attackMs / 1000.0f) * sampleRate;
        m_attackStep = 1.0f / attackSamples;

        float releaseSamples = (releaseMs / 1000.0f) * sampleRate;
        m_releaseStep = 1.0f / releaseSamples;
    }

    void Process(BYTE* pInMono, BYTE* pOutStereo, UINT32 numFrames) override {
        float* inBuffer = reinterpret_cast<float*>(pInMono);
        float* outBuffer = reinterpret_cast<float*>(pOutStereo);

        for (UINT32 i = 0; i < numFrames; ++i) {
            float sample = inBuffer[i];
            float absSample = std::abs(sample);

            // 1. 목표 볼륨(Target Gain) 설정
            // 소리가 기준치보다 크면 볼륨을 1.0(원음)으로, 작으면 0.0(무음)으로 목표를 잡음
            float targetGain = (absSample > m_thresholdLinear) ? 1.0f : 0.0f;

            // 2. 게인 스무딩 (Gain Smoothing)
            if (targetGain > m_currentGain) {
                // 문을 여는 중 (Fade-in: Attack)
                m_currentGain += m_attackStep;
                if (m_currentGain > 1.0f) m_currentGain = 1.0f;
            }
            else if (targetGain < m_currentGain) {
                // 문을 닫는 중 (Fade-out: Release)
                m_currentGain -= m_releaseStep;
                if (m_currentGain < 0.0f) m_currentGain = 0.0f;
            }

            // 3. 부드러워진 볼륨(Gain)을 실제 샘플에 곱함
            sample *= m_currentGain;

            // 4. 출력
            outBuffer[i * 2] = sample;
            outBuffer[i * 2 + 1] = sample;
        }
    }
};

class EchoProcessor : public IAudioProcessor {
private:
    std::vector<float> m_delayBuffer; // 과거의 소리를 기억할 링 버퍼
    int m_writeIndex;                 // 현재 버퍼의 어디에 기록하고 있는지 가리키는 포인터
    float m_feedback;                 // 메아리가 줄어드는 비율 (0.0 ~ 1.0)

public:
    // 생성자: 지연 시간(ms)과 피드백(잔향의 길이) 설정
    EchoProcessor(float delayMs = 300.0f, float feedback = 0.5f, int sampleRate = 48000) {
        // 1. 필요한 버퍼 크기 계산 (최적화 원칙 1: 메모리 미리 할당)
        // 1초(1000ms)에 48000개의 샘플이므로, 300ms면 14400개의 공간이 필요합니다.
        int bufferSize = static_cast<int>((delayMs / 1000.0f) * sampleRate);

        // 2. 버퍼를 0(무음)으로 가득 채워 초기화
        m_delayBuffer.resize(bufferSize, 0.0f);

        m_writeIndex = 0;
        m_feedback = feedback;
    }

    void Process(BYTE* pInMono, BYTE* pOutStereo, UINT32 numFrames) override {
        float* inBuffer = reinterpret_cast<float*>(pInMono);
        float* outBuffer = reinterpret_cast<float*>(pOutStereo);
        int bufferSize = m_delayBuffer.size();

        for (UINT32 i = 0; i < numFrames; ++i) {
            float currentSample = inBuffer[i]; // 현재 마이크에 들어온 소리

            // 1. 과거의 소리 읽어오기
            float delayedSample = m_delayBuffer[m_writeIndex];

            // 2. 현재 소리와 과거의 소리를 섞기 (피드백 비율 곱하기)
            float mixedSample = currentSample + (delayedSample * m_feedback);

            // 3. 섞인 소리를 다시 버퍼에 저장 (메아리가 꼬리를 물고 계속 이어지게 만듦)
            m_delayBuffer[m_writeIndex] = mixedSample;

            // 4. 쓰기 위치 한 칸 이동! 만약 배열 끝에 도달하면 '%' 연산자로 0으로 되돌아감
            m_writeIndex = (m_writeIndex + 1) % bufferSize;

            // 5. 출력 (클리핑 방지)
            // 소리 두 개가 합쳐졌으므로 볼륨이 1.0을 넘어가 찢어질 수 있습니다. 
            // 전체 볼륨을 살짝(0.8) 줄여서 스피커로 내보냅니다.
            outBuffer[i * 2] = mixedSample * 0.8f;
            outBuffer[i * 2 + 1] = mixedSample * 0.8f;
        }
    }
};


class PitchShiftProcessor : public IAudioProcessor {
private:
    SoundTouch m_soundTouch;
    std::vector<float> m_tempMonoBuffer; // 변환된 Mono 데이터를 받을 임시 버퍼

public:
    // 생성자: pitchSemiTones(반음 단위 조절), 샘플 레이트
    // 예: +4.0f (높고 얇은 목소리, 헬륨가스 느낌)
    // 예: -4.0f (낮고 굵은 목소리, 익명 인터뷰 느낌)
    PitchShiftProcessor(float pitchSemiTones = 0.0f, int sampleRate = 48000) {
        // 1. SoundTouch 초기화
        m_soundTouch.setSampleRate(sampleRate);
        m_soundTouch.setChannels(1); // 마이크 입력이 Mono이므로 1채널

        // 2. 피치 설정
        m_soundTouch.setPitchSemiTones(pitchSemiTones);

        // 3. 실시간 처리를 위한 최적화 설정 (지연 시간 및 CPU 부하 감소)
        m_soundTouch.setSetting(SETTING_USE_QUICKSEEK, 1);
        m_soundTouch.setSetting(SETTING_USE_AA_FILTER, 1);
    }

    void Process(BYTE* pInMono, BYTE* pOutStereo, UINT32 numFrames) override {
        float* inBuffer = reinterpret_cast<float*>(pInMono);
        float* outBuffer = reinterpret_cast<float*>(pOutStereo);

        // 1. 임시 버퍼 크기 확보 (필요시 메모리 재할당)
        if (m_tempMonoBuffer.size() < numFrames) {
            m_tempMonoBuffer.resize(numFrames);
        }

        // 2. SoundTouch에 원본 데이터(Mono) 밀어넣기
        m_soundTouch.putSamples(inBuffer, numFrames);

        // 3. 변환된 데이터 꺼내오기
        // 최대 numFrames만큼 꺼내오고, 실제로 꺼내온 개수를 반환받음
        UINT32 samplesReceived = m_soundTouch.receiveSamples(m_tempMonoBuffer.data(), numFrames);

        // 4. 받아온 데이터를 Stereo로 복사하여 출력 버퍼에 쓰기
        for (UINT32 i = 0; i < samplesReceived; ++i) {
            float sample = m_tempMonoBuffer[i];

            // 피치 변환 과정에서 볼륨이 커져 찢어지는(클리핑) 현상 방지를 위해 0.9 곱하기
            outBuffer[i * 2] = sample * 0.9f;
            outBuffer[i * 2 + 1] = sample * 0.9f;
        }

        // 5. 알고리즘 특성상 발생한 I/O 지연(Latency) 처리
        // 넣은 만큼 즉시 나오지 않을 수 있으므로, 모자란 부분은 0.0f(무음)으로 채워 노이즈 방지
        for (UINT32 i = samplesReceived; i < numFrames; ++i) {
            outBuffer[i * 2] = 0.0f;
            outBuffer[i * 2 + 1] = 0.0f;
        }
    }

    // 런타임에 실시간으로 피치를 변경하고 싶을 때 호출하는 함수
    void SetPitch(float pitchSemiTones) {
        m_soundTouch.setPitchSemiTones(pitchSemiTones);
    }
};