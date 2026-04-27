#include <iostream>
#include <locale>
#include <atomic>
#include <thread>
#include <conio.h>

#pragma comment(lib, "Mmdevapi.lib")

#include "AudioDeviceManager.h"
#include "AudioEngine.h"
#include "AudioProcessor.h"

std::atomic<bool> g_bAudioRunning(true);

void AudioThreadFunction(IMMDevice* pCap, IMMDevice* pRen, IAudioProcessor* pProcessor) {
    // 스레드를 위한 COM 초기화
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // 기존의 _kbhit() 루프 대신 g_bAudioRunning 플래그를 확인하며 무한 루프
    AudioEngine engine;
    engine.Run(pCap, pRen, pProcessor, &g_bAudioRunning); // Run 함수 내부 수정 필요
}

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
            NoiseGateProcessor noiseGate(-45.0f); 
            PitchShiftProcessor pitchShift(-4.0f);
            EchoProcessor echo(100.0f, 0.5f);   

            // 2. 체인 프로세서 생성 및 조립 (순서가 매우 중요!)
            EffectChainProcessor voiceChanger;
            voiceChanger.AddProcessor(&noiseGate);
            voiceChanger.AddProcessor(&pitchShift);
            voiceChanger.AddProcessor(&echo);

            // 3. 오디오 엔진을 '별도의 스레드'로 독립시켜서 출발
            std::thread audioThread(AudioThreadFunction, pCaptureDevice.Get(), pRenderDevice.Get(), &voiceChanger);

            while (!_kbhit()) {
                Sleep(100); // 0.1초마다 키보드 입력이 있는지 확인
            }

            // 5. 사용자가 키를 누르면 종료 시퀀스 가동
            std::wcout << L"\n[안내] 종료 신호를 보냈습니다. 오디오 스레드 대기 중..." << std::endl;

            g_bAudioRunning = false; // 오디오 스레드의 무한 루프를 멈추게 함

            if (audioThread.joinable()) {
                audioThread.join(); // 오디오 스레드가 안전하게 정리하고 끝날 때까지 기다려줌
            }

            std::wcout << L"[안내] 오디오 스레드가 안전하게 종료되었습니다." << std::endl;
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