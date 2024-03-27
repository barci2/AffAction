/*******************************************************************************

  Copyright (c) Honda Research Institute Europe GmbH.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "TTSComponent.h"

#include <Rcs_macros.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>

#if defined (_MSC_VER)
#include <sapi.h>
#endif


namespace aff
{

TTSComponent::TTSComponent(EntityBase* parent) :
  ComponentBase(parent), threadRunning(false)
{
  subscribe("Start", &TTSComponent::onStart);
  subscribe("Stop", &TTSComponent::onStop);
  subscribe<std::string>("Speak", &TTSComponent::onSpeak);
  subscribe<>("EmergencyStop", &TTSComponent::onEmergencyStop);
}

TTSComponent::~TTSComponent()
{
}

void TTSComponent::onStart()
{
  if (this->threadRunning == true)
  {
    RLOG(1, "TTS thread already running - doing nothing");
    return;
  }

  this->threadRunning = true;
  ttsThread = std::thread(&TTSComponent::localThread, this);
}

void TTSComponent::onStop()
{
  if (this->threadRunning == false)
  {
    RLOG(1, "TTS thread not running - doing nothing");
    return;
  }

  this->threadRunning = false;
  ttsThread.join();
}

void TTSComponent::onSpeak(std::string text)
{
  RLOG(0, "Saying: %s", text.c_str());

  std::lock_guard<std::mutex> lock(mtx);
  this->textToSpeak = text;
}

void TTSComponent::onEmergencyStop()
{
  RLOG(0, "EmergencyStop");
  onSpeak("Oh no, emergency stop detected");
}

// If port is set to -1, this thread runs espeak on the local machine
#if defined (_MSC_VER)

void TTSComponent::localThread()
{
  // Initialize COM
  if (FAILED(::CoInitialize(NULL)))
  {
    RLOG_CPP(0, "Failed to initialize COM object");
    return;
  }

  // Create a SAPI voice object
  ISpVoice* pVoice = NULL;
  HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);

  if (!SUCCEEDED(hr))
  {
    RLOG_CPP(0, "Failed to create SAPI voice object : 0x % x" << hr);
    return;
  }

  while (this->threadRunning)
  {
    std::string text;
    {
      std::lock_guard<std::mutex> lock(mtx);
      text = this->textToSpeak;
      this->textToSpeak.clear();
    }

    if (!text.empty())
    {
      // The text to be converted to speech
      std::wstring widestr = std::wstring(text.begin(), text.end());
      const wchar_t* textToSpeak = widestr.c_str();

      // Speak the text
      pVoice->Speak(textToSpeak, 0, NULL);
      pVoice->WaitUntilDone(INFINITE);
    }

  }

  // Release the voice object
  pVoice->Release();

  // Uninitialize COM
  CoUninitialize();
}
#else

void TTSComponent::localThread()
{

  while (this->threadRunning)
  {
    std::string text;
    {
      std::lock_guard<std::mutex> lock(mtx);
      text = this->textToSpeak;
      this->textToSpeak.clear();
    }

    if (!text.empty())
    {
      //std::string consCmd = "spd-say " + std::string("\"") + text + std::string("\"");
      std::string consCmd = "espeak " + std::string("\"") + text + std::string("\"");
      int err = system(consCmd.c_str());

      if (err == -1)
      {
        RMSG("Couldn't call spd-say (%s)", consCmd.c_str());
      }
      else
      {
        RLOG(1, "Console command \"%s\"", consCmd.c_str());
      }
    }
  }

}
#endif

}   // namespace aff
