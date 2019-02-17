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
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef _DEBUG
#define EXIT __debugbreak()
#else
#define EXIT goto epilogue
#endif

int main(int argc, char **pArgv)
{
  if (argc != 2)
  {
    printf("Usage %s <InputFile>\n", pArgv[0]);
    return 0;
  }

  slapResult result = slapSuccess;
  SDL_Window *pWindow = NULL;
  SDL_Surface *pSurface = NULL;
  uint32_t *pPixels = NULL;
  slapFileReader *pFileReader = NULL;
  size_t resolutionX, resolutionY;
  size_t frameIndex = 0;
  bool running = true;
  clock_t time;

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

  time = clock();

  while (running)
  {
    frameIndex++;
    result = slapFileReader_GetNextFrame(pFileReader);

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

    slapMemcpy(pPixels, slapFileReader_GetBufferBGRA(pFileReader), resolutionX * resolutionY * sizeof(uint32_t));

    if (0 != SDL_UpdateWindowSurface(pWindow))
      EXIT;

    SDL_Event event;

    while (SDL_PollEvent(&event))
      if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
        running = false;
  }

epilogue:

  if (pSurface)
  {
    SDL_FreeSurface(pSurface);
    pSurface = NULL;
  }

  if (pWindow)
  {
    SDL_DestroyWindow(pWindow);
    pWindow = NULL;
  }

  SDL_Quit();

  if (pFileReader)
    slapDestroyFileReader(&pFileReader);

  return 0;
}