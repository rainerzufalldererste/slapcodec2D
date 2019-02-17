// Copyright 2019 Christoph Stiller
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "slapcodec2D.h"

#include "SDL.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <mutex>
#include <thread>

#ifdef _DEBUG
#define EXIT __debugbreak()
#else
#define EXIT goto epilogue
#endif

enum DecodingState : uint32_t
{
  DS_None,
  DS_FrameReady = 1 << 0,
  DS_CanDecodeNextFrame = 1 << 1,
};

void asyncUpdate(std::mutex *pMutex, std::condition_variable *pConditionVariable, volatile uint32_t *pState, slapFileReader *pFileReader, volatile bool *pRunning);

//////////////////////////////////////////////////////////////////////////

// This is a modified version of the decoder example that pre-loads frames on a separate thread.
// Especially for single- (or-not-very-multi-) threaded applications with expensive update cycles this can be quite useful because it removes the cost of decoding the video from the main thread.
// The more work you do apart from decoding the video, the greater the performance benefit will be.
int main(int argc, char **pArgv)
{
  if (argc != 2)
  {
    printf("Usage %s <InputFile>\n", pArgv[0]);
    return 0;
  }

  slapResult result = slapSuccess;
  SDL_Window *pWindow = nullptr;
  SDL_Surface *pSurface = nullptr;
  uint32_t *pPixels = nullptr;
  slapFileReader *pFileReader = nullptr;
  size_t resolutionX, resolutionY;
  volatile bool running = true;
  volatile uint32_t state = DS_CanDecodeNextFrame;
  std::mutex mutex;
  std::thread updateThread;
  std::condition_variable conditionVariable;
  bool updateFrame;

  pFileReader = slapCreateFileReader(pArgv[1]);
  
  if (!pFileReader)
    EXIT;

  printf("Opened Video '%s' (%" PRIu64 " Frames, IntraFrameStep: %" PRIu64 ")\n", pArgv[1], (uint64_t)slapFileReader_GetFrameCount(pFileReader), (uint64_t)slapFileReader_GetIntraFrameStep(pFileReader));

  if (0 != SDL_Init(SDL_INIT_VIDEO))
    goto epilogue;

  if (slapSuccess != (result = slapFileReader_GetResolution(pFileReader, &resolutionX, &resolutionY)))
    EXIT;

  pWindow = SDL_CreateWindow("Decoder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)resolutionX, (int)resolutionY, SDL_WINDOW_SHOWN);

  if (!pWindow)
    EXIT;

  pSurface = SDL_GetWindowSurface(pWindow);

  if (!pSurface)
    EXIT;

  pPixels = (uint32_t *)pSurface->pixels;

  updateThread = std::thread(&asyncUpdate, &mutex, &conditionVariable, &state, pFileReader, &running);

  while (running)
  {
    updateFrame = false;

    {
      mutex.lock();

      if (state & DS_FrameReady)
      {
        state &= ~DS_FrameReady;
        updateFrame = true;
      }

      mutex.unlock();
    }

    if (updateFrame)
    {
      slapMemcpy(pPixels, slapFileReader_GetBufferBGRA(pFileReader), resolutionX * resolutionY * sizeof(uint32_t));

      mutex.lock();

      state |= DS_CanDecodeNextFrame;

      mutex.unlock();

      conditionVariable.notify_one();
    }

    if (0 != SDL_UpdateWindowSurface(pWindow))
      EXIT;

    SDL_Event event;

    while (SDL_PollEvent(&event))
      if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
        running = false;

    std::unique_lock<std::mutex> lock(mutex);
    conditionVariable.wait_for(lock, std::chrono::milliseconds(1));
  }

epilogue:
  running = false;
  updateThread.join();

  if (pSurface)
  {
    SDL_FreeSurface(pSurface);
    pSurface = nullptr;
  }

  if (pWindow)
  {
    SDL_DestroyWindow(pWindow);
    pWindow = nullptr;
  }

  SDL_Quit();

  if (pFileReader)
    slapDestroyFileReader(&pFileReader);

  return 0;
}

//////////////////////////////////////////////////////////////////////////

void asyncUpdate(std::mutex *pMutex, std::condition_variable *pConditionVariable, volatile uint32_t *pState, slapFileReader *pFileReader, volatile bool *pRunning)
{
  size_t frameIndex = 0;
  clock_t time = clock();
  bool decodeNextFrame;

  while (*pRunning)
  {
    decodeNextFrame = false;

    {
      pMutex->lock();

      if (*pState & DS_CanDecodeNextFrame)
      {
        *pState &= ~DS_CanDecodeNextFrame;
        decodeNextFrame = true;
      }

      pMutex->unlock();
    }

    if (decodeNextFrame)
    {
      frameIndex++;
      slapResult result = slapFileReader_GetNextFrame(pFileReader);

      if (result != slapSuccess)
      {
        if (result == slapError_EndOfStream)
        {
          time = clock() - time;

          printf("Decoded %" PRIu64 " Frames in %" PRIu64 " ms. (%f frames per second)\n", frameIndex, (uint64_t)time, frameIndex / (time * 0.001));

          time = clock();
          frameIndex = 1;

          if (slapSuccess != (result = slapFileReader_RestartVideoStream(pFileReader)))
            EXIT;

          if (slapSuccess != (result = slapFileReader_GetNextFrame(pFileReader)))
            EXIT;
        }
        else
        {
          EXIT;
        }
      }

      if (slapSuccess != (result = slapFileReader_TransformBufferToBGRA(pFileReader)))
        EXIT;

      {
        pMutex->lock();

        *pState |= DS_FrameReady;

        pMutex->unlock();

        pConditionVariable->notify_one();
      }
    }

    std::unique_lock<std::mutex> lock(*pMutex);
    pConditionVariable->wait_for(lock, std::chrono::milliseconds(1));
  }

epilogue:
  *pRunning = false;
}
