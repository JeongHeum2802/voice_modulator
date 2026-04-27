#include <iostream>
#include <locale>

#pragma comment(lib, "Mmdevapi.lib")

#include "AudioDeviceManager.h"
#include "AudioEngine.h"
#include "AudioProcessor.h"

int main() {
    std::wcout.imbue(std::locale("korean"));

    // 1. 시스템 초기화 (COM)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::wcout << L"[에러] COM 초기화 실패!" << std::endl;
        return -1;
    }

    std::wcout << L"--- WASAPI 음성 변조 프로그램 ---" << std::endl;

    // 2. 장치 관리자 생성 및 마이크/스피커 획득
    AudioDeviceManager deviceManager;
    if (deviceManager.Initialize()) {
        auto pCaptureDevice = deviceManager.GetDefaultCaptureDevice();
        //auto pRenderDevice = deviceManager.GetDefaultRenderDevice(); // (기본 스피커)
        auto pRenderDevice = deviceManager.GetRenderDeviceByName(L"CABLE Input");

        // [추가된 디버깅 코드] 장치를 잘 찾았는지 확인
        if (pCaptureDevice == nullptr) {
            std::wcout << L"[문제 발생] 기본 마이크(입력) 장치를 찾을 수 없습니다." << std::endl;
        }
        if (pRenderDevice == nullptr) {
            std::wcout << L"[문제 발생] 기본 스피커(출력) 장치를 찾을 수 없습니다." << std::endl;
        }

        if (pCaptureDevice && pRenderDevice) {
            std::wcout << L"[성공] 마이크와 스피커를 찾았습니다. 오디오 엔진을 가동합니다!" << std::endl;
            std::wcout << L"종료하려면 아무 키나 누르세요..." << std::endl;

            // 1. 이펙터들 생성
            NoiseGateProcessor noiseGate(-45.0f); // 1빠따: 잡음 컷
            PitchShiftProcessor pitchShift(-4.0f); // 2빠따: 피치 낮춤 (굵은 목소리)
            EchoProcessor echo(100.0f, 0.5f);      // 3빠따: 공간감 부여

            // 2. 체인 프로세서 생성 및 조립 (순서가 매우 중요!)
            EffectChainProcessor voiceChanger;
            voiceChanger.AddProcessor(&noiseGate);
            voiceChanger.AddProcessor(&pitchShift);
            voiceChanger.AddProcessor(&echo);

            // 3. 엔진에는 최종적으로 하나로 묶인 체인 프로세서만 넘겨줌
            AudioEngine engine;
            engine.Run(pCaptureDevice.Get(), pRenderDevice.Get(), &voiceChanger);
        }
    }
    else {
        std::wcout << L"[문제 발생] 오디오 장치 관리자 초기화에 실패했습니다." << std::endl;
    }

    // 5. 종료 처리
    CoUninitialize();
    std::wcout << L"\n프로그램을 종료합니다." << std::endl;

    // 콘솔 창이 닫히지 않게 대기
    system("pause");
    return 0;
}