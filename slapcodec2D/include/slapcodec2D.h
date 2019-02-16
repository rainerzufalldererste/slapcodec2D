// Copyright 2019 Christoph Stiller
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef slapcodec_h__
#define slapcodec_h__

#include <stdint.h>

#ifndef IN
#define IN
#endif // !IN

#ifndef OUT
#define OUT
#endif // !OUT

#ifndef IN_OUT
#define IN_OUT IN OUT
#endif // !IN_OUT

#ifdef __cplusplus
extern "C" {
#endif

  void slapMemcpy(OUT void *pDest, IN const void *pSrc, const size_t size);
  void slapMemmove(OUT void *pDest, IN_OUT void *pSrc, const size_t size);

  typedef enum slapResult
  {
    slapSuccess,
    slapError_Generic,
    slapError_InvalidParameter,
    slapError_ArgumentNull,
    slapError_Compress_Internal,
    slapError_FileError,
    slapError_EndOfStream,
    slapError_MemoryAllocation,
    slapError_StateInvalid
  } slapResult;

  slapResult slapWriteJpegFromYUV(const char *filename, IN const void *pData, const size_t resX, const size_t resY);

  typedef struct slapFileWriter slapFileWriter;
  typedef struct slapFileReader slapFileReader;

  slapFileWriter * slapCreateFileWriter(const char *filename, const size_t sizeX, const size_t sizeY, const uint64_t flags);
  void slapDestroyFileWriter(IN_OUT slapFileWriter **ppFileWriter);

  // Setting the intra frame step to 1 will disable intra frame coding.
  // Default IntraFrameStep is 1. (No Intra Frames.)
  slapResult slapFileWriter_SetIntraFrameStep(slapFileWriter *pFileWriter, const size_t step);

  slapResult slapFileWriter_AddFrameYUV420(IN slapFileWriter *pFileWriter, IN void *pData);
  slapResult slapFinalizeFileWriter(IN slapFileWriter *pFileWriter);

  slapFileReader * slapCreateFileReader(const char *filename);
  void slapDestroyFileReader(IN_OUT slapFileReader **ppFileReader);

  slapResult slapFileReader_GetNextFrame(IN slapFileReader *pFileReader);
  slapResult slapFileReader_RestartVideoStream(IN slapFileReader *pFileReader);
  slapResult slapFileReader_TransformBufferToBGRA(IN slapFileReader *pFileReader);

  slapResult slapFileReader_GetResolution(IN slapFileReader *pFileReader, OUT size_t *pResolutionX, OUT size_t *pResolutionY);
  const void * slapFileReader_GetBufferYUV420(IN slapFileReader *pFileReader);
  const void * slapFileReader_GetBufferBGRA(IN slapFileReader *pFileReader);

#ifdef __cplusplus
}
#endif

#endif // slapcodec_h__