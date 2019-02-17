// Copyright 2019 Christoph Stiller
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef bool_t
#define bool_t uint64_t
#endif // !bool_t

#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "slapcodec2D.h"

#include "turbojpeg.h"

#include "apex_memmove/apex_memmove.h"
#include "apex_memmove/apex_memmove.c"

#include <intrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

//////////////////////////////////////////////////////////////////////////

#define slapAlloc(Type, count) (Type *)malloc(sizeof(Type) * (count))
#define slapRealloc(ptr, Type, count) (*ptr = (Type *)realloc(*ptr, sizeof(Type) * (count)))
#define slapFreePtr(ptr)  do { if (ptr && *ptr) { free(*ptr); *ptr = NULL; } } while (0)
#define slapSetZero(ptr, Type) memset(ptr, 0, sizeof(Type))
#define slapStrCpy(target, source) do { size_t size = strlen(source) + 1; target = slapAlloc(char, size); if (target) { memcpy(target, source, size); } } while (0)

#ifdef _DEBUG
#define slapLog(str, ...) printf(str, __VA_ARGS__);
#else
#define slapLog(std, ...)
#endif

#define SLAP_SUB_BUFFER_COUNT 3

#define SLAP_HEADER_BLOCK_SIZE 1024

#define SLAP_PRE_HEADER_SIZE 8
#define SLAP_PRE_HEADER_HEADER_SIZE_INDEX 0
#define SLAP_PRE_HEADER_FRAME_COUNT_INDEX 1
#define SLAP_PRE_HEADER_FRAME_SIZEX_INDEX 2
#define SLAP_PRE_HEADER_FRAME_SIZEY_INDEX 3
#define SLAP_PRE_HEADER_IFRAME_STEP_INDEX 4
#define SLAP_PRE_HEADER_CODEC_FLAGS_INDEX 5

#define SLAP_HEADER_PER_FRAME_FULL_FRAME_OFFSET 2
#define SLAP_HEADER_PER_FRAME_SIZE (SLAP_HEADER_PER_FRAME_FULL_FRAME_OFFSET + SLAP_SUB_BUFFER_COUNT * 2)

#define SLAP_HEADER_FRAME_OFFSET_INDEX 0
#define SLAP_HEADER_FRAME_DATA_SIZE_INDEX 1

#define SLAP_IFRAME_STEP 1

typedef union mode
{
  uint64_t flagsPack;

  struct flags
  {
    unsigned int encoder : 4;
  } flags;

} mode;

typedef struct slapEncoder
{
  size_t frameIndex;
  size_t iframeStep;
  size_t resX;
  size_t resY;
  uint8_t *pLastFrame;

  mode mode;

  int quality;
  int iframeQuality;
  void *pEncoderInternal[SLAP_SUB_BUFFER_COUNT];
  void *pDecoderInternal[SLAP_SUB_BUFFER_COUNT];
  void *pCompressedBuffers[SLAP_SUB_BUFFER_COUNT];
  size_t compressedSubBufferSizes[SLAP_SUB_BUFFER_COUNT];
} slapEncoder;

typedef struct slapFileWriter
{
  FILE *pMainFile;
  FILE *pHeaderFile;
  uint64_t headerPosition;
  uint64_t frameCount;
  slapEncoder *pEncoder;
  void *pData;
  uint64_t frameSizeOffsets[SLAP_HEADER_BLOCK_SIZE];
  size_t frameSizeOffsetIndex;
  char *filename;
} slapFileWriter;

typedef struct slapDecoder
{
  size_t frameIndex;
  size_t iframeStep;
  size_t resX;
  size_t resY;

  mode mode;

  void *pDecoders[SLAP_SUB_BUFFER_COUNT];
  uint8_t *pLastFrame;
} slapDecoder;

typedef struct slapFileReader
{
  FILE *pFile;
  void *pCurrentFrame;
  size_t currentFrameAllocatedSize;
  size_t currentFrameSize;

  void *pDecodedFrameYUV;
  void *pDecodedFrameBGRA;

  uint64_t preHeaderBlock[SLAP_PRE_HEADER_SIZE];
  uint64_t *pHeader;
  size_t headerOffset;
  size_t frameIndex;

  slapDecoder *pDecoder;
} slapFileReader;

slapEncoder * slapCreateEncoder(const size_t sizeX, const size_t sizeY, const uint64_t flags);
void slapDestroyEncoder(IN_OUT slapEncoder **ppEncoder);

slapResult slapFinalizeEncoder(IN slapEncoder *pEncoder);

// After slapEncoder_BeginFrame has finished, the subFrame can be compressed and written.
slapResult slapEncoder_BeginFrame(IN slapEncoder *pEncoder, IN void *pData);

// After slapEncoder_BeginSubFrame has finished, the frame can be written to disk.
slapResult slapEncoder_BeginSubFrame(IN slapEncoder *pEncoder, IN void *pData, OUT void **ppCompressedData, OUT size_t *pSize, const size_t subFrameIndex);
slapResult slapEncoder_EndSubFrame(IN slapEncoder *pEncoder, IN void *pData, const size_t subFrameIndex);

slapResult slapEncoder_EndFrame(IN slapEncoder *pEncoder, IN void *pData);

slapDecoder * slapCreateDecoder(const size_t sizeX, const size_t sizeY, const uint64_t flags);
void slapDestroyDecoder(IN_OUT slapDecoder **ppDecoder);

slapResult slapDecoder_DecodeSubFrame(IN slapDecoder *pDecoder, const size_t decoderIndex, IN void **ppCompressedData, IN size_t *pLength, IN_OUT void *pYUVData);
slapResult slapDecoder_FinalizeFrame(IN slapDecoder *pDecoder, IN void *pData, const size_t length, IN_OUT void *pYUVData);

slapResult slapFileReader_ReadNextFrame(IN slapFileReader *pFileReader);
slapResult slapFileReader_DecodeCurrentFrame(IN slapFileReader *pFileReader);

//////////////////////////////////////////////////////////////////////////

slapResult _slapCompressChannel(IN void *pData, IN_OUT void **ppCompressedData, IN_OUT size_t *pCompressedDataSize, const size_t width, const size_t height, const int quality, IN void *pCompressor);
slapResult _slapCompressYUV420(IN void *pData, IN_OUT void **ppCompressedData, IN_OUT size_t *pCompressedDataSize, const size_t width, const size_t height, const int quality, IN void *pCompressor);
slapResult _slapDecompressChannel(IN void *pData, IN_OUT void *pCompressedData, const size_t compressedDataSize, const size_t width, const size_t height, IN void *pDecompressor);
slapResult _slapDecompressYUV420(IN void *pData, IN_OUT void *pCompressedData, const size_t compressedDataSize, const size_t width, const size_t height, IN void *pDecompressor);
void _slapEncodeLastFrameDiff(IN_OUT void *pLastFrame, IN_OUT void *pData, const size_t resX, const size_t resY);
void _slapCopyToLastFrame(IN_OUT void *pData, OUT void *pLastFrame, const size_t resX, const size_t resY);
void _slapDecodeLastFrameDiff(IN_OUT void *pData, OUT void *pLastFrame, const size_t resX, const size_t resY);

typedef struct _slapFrameEncoderBlock
{
  size_t frameSize;
  void *pFrameData;
} _slapFrameEncoderBlock;

//////////////////////////////////////////////////////////////////////////

void slapMemcpy(OUT void *pDest, IN const void *pSrc, const size_t size)
{
  apex_memcpy(pDest, pSrc, size);
}

void slapMemmove(OUT void *pDest, IN_OUT void *pSrc, const size_t size)
{
  apex_memmove(pDest, pSrc, size);
}

slapResult slapWriteJpegFromYUV(const char *filename, IN const void *pData, const size_t resX, const size_t resY)
{
  slapResult result = slapSuccess;
  tjhandle jpegHandle = NULL;
  unsigned char *pBuffer = NULL;
  unsigned long bufferSize = 0;
  FILE *pFile = NULL;

  if (!filename || !pData)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  jpegHandle = tjInitCompress();

  if (!jpegHandle)
  {
    result = slapError_Compress_Internal;
    goto epilogue;
  }

  if (tjCompressFromYUV(jpegHandle, (unsigned char *)pData, (int)resX, 32, (int)resY, TJSAMP_420, &pBuffer, &bufferSize, 75, 0))
  {
    slapLog(tjGetErrorStr2(jpegHandle));
    result = slapError_Compress_Internal;
    goto epilogue;
  }

  pFile = fopen(filename, "wb");

  if (!pFile)
  {
    result = slapError_FileError;
    goto epilogue;
  }

  if (bufferSize != fwrite(pBuffer, 1, bufferSize, pFile))
  {
    result = slapError_FileError;
    goto epilogue;
  }

epilogue:
  if (jpegHandle)
    tjDestroy(jpegHandle);

  if (pBuffer)
    tjFree(pBuffer);

  if (pFile)
  {
    fclose(pFile);

    if (result != slapSuccess)
      remove(filename);
  }

  return result;
}

slapEncoder * slapCreateEncoder(const size_t sizeX, const size_t sizeY, const uint64_t flags)
{
  if (sizeX & 7 || sizeY & 7) // must be multiple of 8.
    return NULL;

  slapEncoder *pEncoder = slapAlloc(slapEncoder, 1);

  if (!pEncoder)
    goto epilogue;

  slapSetZero(pEncoder, slapEncoder);

  pEncoder->resX = sizeX;
  pEncoder->resY = sizeY;
  pEncoder->iframeStep = SLAP_IFRAME_STEP;
  pEncoder->mode.flagsPack = flags;
  pEncoder->quality = 75;
  pEncoder->iframeQuality = 75;

  pEncoder->pLastFrame = slapAlloc(uint8_t, sizeX * sizeY * 3 / 2);

  if (!pEncoder->pLastFrame)
    goto epilogue;

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    pEncoder->pEncoderInternal[i] = tjInitCompress();

    if (!pEncoder->pEncoderInternal[i])
      goto epilogue;

    pEncoder->pDecoderInternal[i] = tjInitDecompress();

    if (!pEncoder->pDecoderInternal[i])
      goto epilogue;
  }

  return pEncoder;

epilogue:

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    if (pEncoder->pEncoderInternal[i])
      if (pEncoder->pEncoderInternal[i])
        tjDestroy(pEncoder->pEncoderInternal[i]);

    if (pEncoder->pDecoderInternal[i])
      if (pEncoder->pDecoderInternal[i])
        tjDestroy(pEncoder->pDecoderInternal[i]);
  }

  if ((pEncoder)->pLastFrame)
    slapFreePtr(&(pEncoder)->pLastFrame);

  slapFreePtr(&pEncoder);

  return NULL;
}

void slapDestroyEncoder(IN_OUT slapEncoder **ppEncoder)
{
  if (ppEncoder && *ppEncoder)
  {
    for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
    {
      if ((*ppEncoder)->pEncoderInternal[i])
        if ((*ppEncoder)->pEncoderInternal[i])
          tjDestroy((*ppEncoder)->pEncoderInternal[i]);

      if ((*ppEncoder)->pDecoderInternal[i])
        if ((*ppEncoder)->pDecoderInternal[i])
          tjDestroy((*ppEncoder)->pDecoderInternal[i]);

      if ((*ppEncoder)->pCompressedBuffers[i])
        slapFreePtr(&(*ppEncoder)->pCompressedBuffers[i]);
    }

    if ((*ppEncoder)->pLastFrame)
      slapFreePtr(&(*ppEncoder)->pLastFrame);
  }

  slapFreePtr(ppEncoder);
}

slapResult slapFinalizeEncoder(IN slapEncoder *pEncoder)
{
  (void)pEncoder;

  return slapSuccess;
}

slapResult slapEncoder_BeginFrame(IN slapEncoder *pEncoder, IN void *pData)
{
  slapResult result = slapSuccess;

  if (!pEncoder || !pData)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  if (pEncoder->frameIndex % pEncoder->iframeStep != 0)
    _slapEncodeLastFrameDiff(pEncoder->pLastFrame, pData, pEncoder->resX, pEncoder->resY);
  else
    _slapCopyToLastFrame(pData, pEncoder->pLastFrame, pEncoder->resX, pEncoder->resY);

epilogue:
  return result;
}

slapResult slapEncoder_BeginSubFrame(IN slapEncoder *pEncoder, IN void *pData, OUT void **ppCompressedData, OUT size_t *pSize, const size_t subFrameIndex)
{
  slapResult result = slapSuccess;

  if (!pEncoder || !pData || !ppCompressedData || !pSize)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  const int quality = (pEncoder->frameIndex % pEncoder->iframeStep == 0) ? pEncoder->quality : pEncoder->iframeQuality;

  if (subFrameIndex == 0)
    result = _slapCompressChannel(((uint8_t *)pData), &pEncoder->pCompressedBuffers[subFrameIndex], &pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX, pEncoder->resY, quality, pEncoder->pEncoderInternal[subFrameIndex]);
  else if (subFrameIndex == 1)
    result = _slapCompressChannel(((uint8_t *)pData) + pEncoder->resX * pEncoder->resY, &pEncoder->pCompressedBuffers[subFrameIndex], &pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX >> 1, pEncoder->resY >> 1, quality, pEncoder->pEncoderInternal[subFrameIndex]);
  else if (subFrameIndex == 2)
    result = _slapCompressChannel(((uint8_t *)pData) + pEncoder->resX * pEncoder->resY * 5 / 4, &pEncoder->pCompressedBuffers[subFrameIndex], &pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX >> 1, pEncoder->resY >> 1, quality, pEncoder->pEncoderInternal[subFrameIndex]);

  if (result != slapSuccess)
    goto epilogue;

  *pSize = pEncoder->compressedSubBufferSizes[subFrameIndex];
  *ppCompressedData = pEncoder->pCompressedBuffers[subFrameIndex];

epilogue:
  return result;
}

slapResult slapEncoder_EndSubFrame(IN slapEncoder *pEncoder, IN void *pData, const size_t subFrameIndex)
{
  slapResult result = slapSuccess;

  uint8_t *pDestination = NULL;

  if (pEncoder->frameIndex % pEncoder->iframeStep != 0)
    pDestination = (uint8_t *)pData;
  else
    pDestination = (uint8_t *)pEncoder->pLastFrame;

  if (subFrameIndex == 0)
    result = _slapDecompressChannel(pDestination, pEncoder->pCompressedBuffers[subFrameIndex], pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX, pEncoder->resY, pEncoder->pDecoderInternal[subFrameIndex]);
  else if (subFrameIndex == 1)
    result = _slapDecompressChannel(pDestination + pEncoder->resX * pEncoder->resY, pEncoder->pCompressedBuffers[subFrameIndex], pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX >> 1, pEncoder->resY >> 1, pEncoder->pDecoderInternal[subFrameIndex]);
  else if (subFrameIndex == 2)
    result = _slapDecompressChannel(pDestination + pEncoder->resX * pEncoder->resY * 5 / 4, pEncoder->pCompressedBuffers[subFrameIndex], pEncoder->compressedSubBufferSizes[subFrameIndex], pEncoder->resX >> 1, pEncoder->resY >> 1, pEncoder->pDecoderInternal[subFrameIndex]);

  if (result != slapSuccess)
    goto epilogue;

epilogue:
  return result;
}

slapResult slapEncoder_EndFrame(IN slapEncoder *pEncoder, IN void *pData)
{
  slapResult result = slapSuccess;

  if (!pEncoder)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  if (pEncoder->frameIndex % pEncoder->iframeStep != 0)
    _slapDecodeLastFrameDiff(pData, pEncoder->pLastFrame, pEncoder->resX, pEncoder->resY);

  pEncoder->frameIndex++;

epilogue:
  return result;
}

slapResult _slapWriteToHeader(IN slapFileWriter *pFileWriter, const uint64_t data)
{
  slapResult result = slapSuccess;

  pFileWriter->frameSizeOffsets[pFileWriter->frameSizeOffsetIndex++] = data;
  pFileWriter->headerPosition++;

  if (pFileWriter->frameSizeOffsetIndex >= (uint64_t)SLAP_HEADER_BLOCK_SIZE)
  {
    if ((size_t)SLAP_HEADER_BLOCK_SIZE != fwrite(pFileWriter->frameSizeOffsets, sizeof(uint64_t), SLAP_HEADER_BLOCK_SIZE, pFileWriter->pHeaderFile))
    {
      result = slapError_FileError;
      goto epilogue;
    }

    pFileWriter->frameSizeOffsetIndex = 0;
  }

epilogue:
  return result;
}

slapFileWriter * slapCreateFileWriter(const char *filename, const size_t sizeX, const size_t sizeY, const uint64_t flags)
{
  slapFileWriter *pFileWriter = slapAlloc(slapFileWriter, 1);
  char filenameBuffer[0xFF];
  char headerFilenameBuffer[0xFF];

  if (!pFileWriter)
    goto epilogue;

  slapSetZero(pFileWriter, slapFileWriter);
  slapStrCpy(pFileWriter->filename, filename);

  if (!pFileWriter->filename)
    goto epilogue;

  pFileWriter->pEncoder = slapCreateEncoder(sizeX, sizeY, flags);

  if (!pFileWriter->pEncoder)
    goto epilogue;

  sprintf_s(filenameBuffer, 0xFF, "%s.video", filename);
  sprintf_s(headerFilenameBuffer, 0xFF, "%s.header", filename);

  pFileWriter->pMainFile = fopen(filenameBuffer, "wb");

  if (!pFileWriter->pMainFile)
    goto epilogue;

  pFileWriter->pHeaderFile = fopen(headerFilenameBuffer, "wb");

  if (!pFileWriter->pHeaderFile)
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, 0))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, 0))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, (uint64_t)pFileWriter->pEncoder->resX))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, (uint64_t)pFileWriter->pEncoder->resY))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, (uint64_t)pFileWriter->pEncoder->iframeStep))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, (uint64_t)pFileWriter->pEncoder->mode.flagsPack))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, 0))
    goto epilogue;

  if (slapSuccess != _slapWriteToHeader(pFileWriter, 0))
    goto epilogue;

  if (pFileWriter->headerPosition != SLAP_PRE_HEADER_SIZE)
    goto epilogue;

  return pFileWriter;

epilogue:

  if (pFileWriter)
  {
    slapDestroyEncoder(&pFileWriter->pEncoder);

    if (pFileWriter->pMainFile)
      fclose(pFileWriter->pMainFile);

    if (pFileWriter->pHeaderFile)
      fclose(pFileWriter->pHeaderFile);

    if (pFileWriter->filename)
      slapFreePtr(&pFileWriter->filename);
  }

  slapFreePtr(&pFileWriter);
  return NULL;
}

void slapDestroyFileWriter(IN_OUT slapFileWriter **ppFileWriter)
{
  if (ppFileWriter && *ppFileWriter)
  {
    slapDestroyEncoder(&(*ppFileWriter)->pEncoder);

    if ((*ppFileWriter)->pData)
      tjFree((*ppFileWriter)->pData);

    if ((*ppFileWriter)->filename)
      slapFreePtr(&(*ppFileWriter)->filename);
  }

  slapFreePtr(ppFileWriter);
}

slapResult slapFileWriter_SetIntraFrameStep(slapFileWriter *pFileWriter, const size_t step)
{
  if (!pFileWriter)
    return slapError_ArgumentNull;

  if (step == 0)
    return slapError_InvalidParameter;

  if (pFileWriter->pEncoder->frameIndex != 0)
    return slapError_StateInvalid;

  pFileWriter->pEncoder->iframeStep = step;

  return slapSuccess;
}

slapResult slapFinalizeFileWriter(IN slapFileWriter *pFileWriter)
{
  slapResult result = slapError_Generic;
  FILE *pFile = NULL;
  FILE *pReadFile = NULL;
  char filenameBuffer[0xFF];
  void *pData = NULL;
  size_t fileSize = 0; 
  const size_t maxBlockSize = 1024 * 1024 * 64;
  size_t remainingSize = 0;

  if (!pFileWriter)
    goto epilogue;

  if (pFileWriter->pEncoder)
    slapFinalizeEncoder(pFileWriter->pEncoder);

  if (pFileWriter->pHeaderFile)
  {
    if (pFileWriter->frameSizeOffsetIndex != 0)
      fwrite(pFileWriter->frameSizeOffsets, 1, sizeof(uint64_t) * pFileWriter->frameSizeOffsetIndex, pFileWriter->pHeaderFile);

    fflush(pFileWriter->pHeaderFile);
    fclose(pFileWriter->pHeaderFile);
    pFileWriter->pHeaderFile = NULL;
  }

  if (pFileWriter->pMainFile)
  {
    fflush(pFileWriter->pMainFile);
    fclose(pFileWriter->pMainFile);
    pFileWriter->pMainFile = NULL;
  }

  pFile = fopen(pFileWriter->filename, "wb");

  if (!pFile)
    goto epilogue;

  sprintf_s(filenameBuffer, 0xFF, "%s.header", pFileWriter->filename);
  pReadFile = fopen(filenameBuffer, "rb");

  if (!pReadFile)
    goto epilogue;

  pData = slapAlloc(unsigned char, pFileWriter->headerPosition * sizeof(uint64_t));

  if (!pData)
    goto epilogue;

  if (pFileWriter->headerPosition != (fread(pData, sizeof(uint64_t), pFileWriter->headerPosition, pReadFile)))
    goto epilogue;

  ((uint64_t *)pData)[SLAP_PRE_HEADER_HEADER_SIZE_INDEX] = pFileWriter->headerPosition - SLAP_PRE_HEADER_SIZE;
  ((uint64_t *)pData)[SLAP_PRE_HEADER_FRAME_COUNT_INDEX] = pFileWriter->frameCount;

  if (pFileWriter->headerPosition != fwrite(pData, sizeof(uint64_t), pFileWriter->headerPosition, pFile))
    goto epilogue;

  fclose(pReadFile);
  remove(filenameBuffer);

  sprintf_s(filenameBuffer, 0xFF, "%s.video", pFileWriter->filename);
  pReadFile = fopen(filenameBuffer, "rb");

  if (!pFile)
    goto epilogue;

  fseek(pReadFile, 0, SEEK_END);
  fileSize = ftell(pReadFile);
  fseek(pReadFile, 0, SEEK_SET);

  remainingSize = fileSize;

  slapRealloc(&pData, uint8_t, fileSize < maxBlockSize ? fileSize : maxBlockSize);

  while (remainingSize > maxBlockSize)
  {
    if (maxBlockSize != fread(pData, 1, maxBlockSize, pReadFile))
      goto epilogue;

    if (maxBlockSize != fwrite(pData, 1, maxBlockSize, pFile))
      goto epilogue;

    remainingSize -= maxBlockSize;
  }

  if (remainingSize != fread(pData, 1, remainingSize, pReadFile))
    goto epilogue;

  if (remainingSize != fwrite(pData, 1, remainingSize, pFile))
    goto epilogue;

  fclose(pReadFile);
  pReadFile = NULL;
  remove(filenameBuffer);

  result = slapSuccess;

epilogue:

  if (pFile)
    fclose(pFile);

  if (pReadFile)
    fclose(pReadFile);

  if (pData)
    slapFreePtr(&pData);

  return result;
}

slapResult slapFileWriter_AddFrameYUV420(IN slapFileWriter *pFileWriter, IN void *pData)
{
  slapResult result = slapSuccess;
  size_t filePosition = 0;
  _slapFrameEncoderBlock subFrames[SLAP_SUB_BUFFER_COUNT];
  size_t totalFullFrameSize = 0;

  if (!pFileWriter || !pData)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  result = slapEncoder_BeginFrame(pFileWriter->pEncoder, pData);

  if (result != slapSuccess)
    goto epilogue;

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    result = slapEncoder_BeginSubFrame(pFileWriter->pEncoder, pData, &subFrames[i].pFrameData, &subFrames[i].frameSize, i);

    if (result != slapSuccess)
      goto epilogue;
  }

  filePosition = ftell(pFileWriter->pMainFile);

  if ((result = _slapWriteToHeader(pFileWriter, filePosition)) != slapSuccess)
    goto epilogue;

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
    totalFullFrameSize += subFrames[i].frameSize;

  if ((result = _slapWriteToHeader(pFileWriter, totalFullFrameSize)) != slapSuccess)
    goto epilogue;

  filePosition = 0;

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    if ((result = _slapWriteToHeader(pFileWriter, filePosition)) != slapSuccess)
      goto epilogue;

    if ((result = _slapWriteToHeader(pFileWriter, subFrames[i].frameSize)) != slapSuccess)
      goto epilogue;

    filePosition += subFrames[i].frameSize;

    if (subFrames[i].frameSize != fwrite(subFrames[i].pFrameData, 1, subFrames[i].frameSize, pFileWriter->pMainFile))
    {
      result = slapError_FileError;
      goto epilogue;
    }
  }

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    result = slapEncoder_EndSubFrame(pFileWriter->pEncoder, pData, i);

    if (result != slapSuccess)
      goto epilogue;
  }

  // finalize frame.
  result = slapEncoder_EndFrame(pFileWriter->pEncoder, pData);

  if (result != slapSuccess)
    goto epilogue;

  pFileWriter->frameCount++; 

epilogue:
  return result;
}

slapDecoder * slapCreateDecoder(const size_t sizeX, const size_t sizeY, const uint64_t flags)
{
  if (sizeX & 7 || sizeY & 7) // must be multiple of 8.
    return NULL;

  slapDecoder *pDecoder = slapAlloc(slapDecoder, 1);

  if (!pDecoder)
    goto epilogue;

  slapSetZero(pDecoder, slapDecoder);

  pDecoder->resX = sizeX;
  pDecoder->resY = sizeY;
  pDecoder->iframeStep = SLAP_IFRAME_STEP;
  pDecoder->mode.flagsPack = flags;

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    pDecoder->pDecoders[i] = tjInitDecompress();

    if (!pDecoder->pDecoders[i])
      goto epilogue;
  }

  pDecoder->pLastFrame = slapAlloc(uint8_t, sizeX * sizeY * 3 / 2);

  if (!pDecoder->pLastFrame)
    goto epilogue;

  return pDecoder;

epilogue:
  if (pDecoder->pDecoders)
  {
    for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
      if (pDecoder->pDecoders[i])
        tjDestroy(pDecoder->pDecoders[i]);
  }

  if (pDecoder->pLastFrame)
    slapFreePtr(&pDecoder->pLastFrame);

  slapFreePtr(&pDecoder);

  return NULL;
}

void slapDestroyDecoder(IN_OUT slapDecoder **ppDecoder)
{
  if (ppDecoder && *ppDecoder)
  {
    if ((*ppDecoder)->pDecoders)
    {
      for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
        if ((*ppDecoder)->pDecoders[i])
          tjDestroy((*ppDecoder)->pDecoders[i]);
    }

    if ((*ppDecoder)->pLastFrame)
      slapFreePtr(&(*ppDecoder)->pLastFrame);
  }

  slapFreePtr(ppDecoder);
}

slapResult slapDecoder_DecodeSubFrame(IN slapDecoder *pDecoder, const size_t decoderIndex, IN void **ppCompressedData, IN size_t *pLength, IN_OUT void *pYUVData)
{
  slapResult result = slapSuccess;

  uint8_t *pOutData = (uint8_t *)pYUVData;

  if (decoderIndex == 0)
    result = _slapDecompressChannel(pOutData, ppCompressedData[decoderIndex], pLength[decoderIndex], pDecoder->resX, pDecoder->resY, pDecoder->pDecoders[decoderIndex]);
  else if (decoderIndex == 1)
    result = _slapDecompressChannel(pOutData + pDecoder->resX * pDecoder->resY, ppCompressedData[decoderIndex], pLength[decoderIndex], pDecoder->resX >> 1, pDecoder->resY >> 1, pDecoder->pDecoders[decoderIndex]);
  else
    result = _slapDecompressChannel(pOutData + pDecoder->resX * pDecoder->resY * 5 / 4, ppCompressedData[decoderIndex], pLength[decoderIndex], pDecoder->resX >> 1, pDecoder->resY >> 1, pDecoder->pDecoders[decoderIndex]);

  if (result != slapSuccess)
    goto epilogue;

epilogue:
  return result;
}

slapResult slapDecoder_FinalizeFrame(IN slapDecoder *pDecoder, IN void *pData, const size_t length, IN_OUT void *pYUVData)
{
  slapResult result = slapSuccess;

  if (!pDecoder || !pData || !length || !pYUVData)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  if (pDecoder->frameIndex % pDecoder->iframeStep != 0)
    _slapDecodeLastFrameDiff(pYUVData, pDecoder->pLastFrame, pDecoder->resX, pDecoder->resY);
  else
    _slapCopyToLastFrame(pYUVData, pDecoder->pLastFrame, pDecoder->resX, pDecoder->resY);

  pDecoder->frameIndex++;

epilogue:
  return result;
}

slapFileReader * slapCreateFileReader(const char *filename)
{
  slapFileReader *pFileReader = slapAlloc(slapFileReader, 1);
  size_t frameSize = 0;

  if (!pFileReader)
    goto epilogue;

  slapSetZero(pFileReader, slapFileReader);

  pFileReader->pFile = fopen(filename, "rb");

  if (!pFileReader->pFile)
    goto epilogue;

  if (SLAP_PRE_HEADER_SIZE != fread(pFileReader->preHeaderBlock, sizeof(uint64_t), SLAP_PRE_HEADER_SIZE, pFileReader->pFile))
    goto epilogue;

  pFileReader->pHeader = slapAlloc(uint64_t, pFileReader->preHeaderBlock[SLAP_PRE_HEADER_HEADER_SIZE_INDEX]);

  if (!pFileReader->pHeader)
    goto epilogue;

  if (pFileReader->preHeaderBlock[SLAP_PRE_HEADER_HEADER_SIZE_INDEX] != fread(pFileReader->pHeader, sizeof(uint64_t), pFileReader->preHeaderBlock[SLAP_PRE_HEADER_HEADER_SIZE_INDEX], pFileReader->pFile))
    goto epilogue;

  pFileReader->headerOffset = ftell(pFileReader->pFile);

  pFileReader->pDecoder = slapCreateDecoder(pFileReader->preHeaderBlock[SLAP_PRE_HEADER_FRAME_SIZEX_INDEX], pFileReader->preHeaderBlock[SLAP_PRE_HEADER_FRAME_SIZEY_INDEX], pFileReader->preHeaderBlock[SLAP_PRE_HEADER_CODEC_FLAGS_INDEX]);
  pFileReader->pDecoder->iframeStep = pFileReader->preHeaderBlock[SLAP_PRE_HEADER_IFRAME_STEP_INDEX];

  if (!pFileReader->pDecoder)
    goto epilogue;

  frameSize = pFileReader->pDecoder->resX * pFileReader->pDecoder->resX * 3 / 2;

  pFileReader->pDecodedFrameYUV = slapAlloc(uint8_t, frameSize);

  if (!pFileReader->pDecodedFrameYUV)
    goto epilogue;

  return pFileReader;

epilogue:

  slapFreePtr(&(pFileReader)->pHeader);
  slapFreePtr(&(pFileReader)->pCurrentFrame);
  slapFreePtr(&(pFileReader)->pDecodedFrameYUV);

  if (pFileReader->pFile)
    fclose(pFileReader->pFile);

  if (pFileReader->pDecoder)
    slapDestroyDecoder(&pFileReader->pDecoder);

  slapFreePtr(&pFileReader);

  return NULL;
}

void slapDestroyFileReader(IN_OUT slapFileReader **ppFileReader)
{
  if (ppFileReader && *ppFileReader)
  {
    slapFreePtr(&(*ppFileReader)->pHeader);
    slapFreePtr(&(*ppFileReader)->pCurrentFrame);
    slapFreePtr(&(*ppFileReader)->pDecodedFrameYUV);
    slapFreePtr(&(*ppFileReader)->pDecodedFrameBGRA);
    slapDestroyDecoder(&(*ppFileReader)->pDecoder);
    fclose((*ppFileReader)->pFile);
  }

  slapFreePtr(ppFileReader);
}

slapResult slapFileReader_GetResolution(IN slapFileReader *pFileReader, OUT size_t *pResolutionX, OUT size_t *pResolutionY)
{
  if (!pFileReader || !pResolutionX || !pResolutionY)
    return slapError_ArgumentNull;

  *pResolutionX = pFileReader->pDecoder->resX;
  *pResolutionY = pFileReader->pDecoder->resY;

  return slapSuccess;
}

size_t slapFileReader_GetFrameCount(IN slapFileReader *pFileReader)
{
  if (!pFileReader)
    return 0;

  return pFileReader->preHeaderBlock[SLAP_PRE_HEADER_FRAME_COUNT_INDEX];
}

size_t slapFileReader_GetIntraFrameStep(IN slapFileReader *pFileReader)
{
  if (!pFileReader)
    return (size_t)-1;

  return pFileReader->pDecoder->iframeStep;
}

slapResult slapFileReader_SetFrameIndex(IN slapFileReader *pFileReader, const size_t frameIndex)
{
  if (!pFileReader)
    return slapError_ArgumentNull;

  if (slapFileReader_GetFrameCount(pFileReader) <= frameIndex)
    return slapError_EndOfStream;

  pFileReader->pDecoder->frameIndex = pFileReader->frameIndex = frameIndex - (frameIndex % slapFileReader_GetIntraFrameStep(pFileReader));
  
  return slapSuccess;
}

size_t slapFileReader_GetFrameIndex(IN slapFileReader *pFileReader)
{
  if (!pFileReader)
    return (size_t)-1;

  return pFileReader->frameIndex;
}

slapResult slapFileReader_ReadNextFrame(IN slapFileReader *pFileReader)
{
  slapResult result = slapSuccess;
  uint64_t position;

  if (!pFileReader)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  if (pFileReader->frameIndex >= pFileReader->preHeaderBlock[SLAP_PRE_HEADER_FRAME_COUNT_INDEX])
  {
    result = slapError_EndOfStream;
    goto epilogue;
  }

  position = pFileReader->pHeader[SLAP_HEADER_PER_FRAME_SIZE * pFileReader->frameIndex + SLAP_HEADER_FRAME_OFFSET_INDEX] + pFileReader->headerOffset;
  pFileReader->currentFrameSize = pFileReader->pHeader[SLAP_HEADER_PER_FRAME_SIZE * pFileReader->frameIndex + SLAP_HEADER_FRAME_DATA_SIZE_INDEX];

  if (pFileReader->currentFrameAllocatedSize < pFileReader->currentFrameSize)
  {
    slapRealloc(&pFileReader->pCurrentFrame, uint8_t, pFileReader->currentFrameSize);
    pFileReader->currentFrameAllocatedSize = pFileReader->currentFrameSize;

    if (!pFileReader->pCurrentFrame)
    {
      pFileReader->currentFrameAllocatedSize = 0;
      result = slapError_MemoryAllocation;
      goto epilogue;
    }
  }

  if (fseek(pFileReader->pFile, (long)position, SEEK_SET))
  {
    result = slapError_FileError;
    goto epilogue;
  }

  if (pFileReader->currentFrameSize != fread(pFileReader->pCurrentFrame, 1, pFileReader->currentFrameSize, pFileReader->pFile))
  {
    result = slapError_FileError;
    goto epilogue;
  }

  pFileReader->frameIndex++;

epilogue:
  return result;
}

slapResult slapFileReader_DecodeCurrentFrame(IN slapFileReader *pFileReader)
{
  slapResult result = slapSuccess;
  void *dataAddrs[SLAP_SUB_BUFFER_COUNT];
  size_t dataSizes[SLAP_SUB_BUFFER_COUNT];

  if (!pFileReader)
  {
    result = slapError_ArgumentNull;
    goto epilogue;
  }

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    dataAddrs[i] = ((uint8_t *)pFileReader->pCurrentFrame) + pFileReader->pHeader[SLAP_HEADER_PER_FRAME_SIZE * (pFileReader->frameIndex - 1) + SLAP_HEADER_PER_FRAME_FULL_FRAME_OFFSET + i * 2 + SLAP_HEADER_FRAME_OFFSET_INDEX];
    dataSizes[i] = pFileReader->pHeader[SLAP_HEADER_PER_FRAME_SIZE * (pFileReader->frameIndex - 1) + SLAP_HEADER_PER_FRAME_FULL_FRAME_OFFSET + i * 2 + SLAP_HEADER_FRAME_DATA_SIZE_INDEX];
  }

  for (size_t i = 0; i < SLAP_SUB_BUFFER_COUNT; i++)
  {
    result = slapDecoder_DecodeSubFrame(pFileReader->pDecoder, i, dataAddrs, dataSizes, pFileReader->pDecodedFrameYUV);

    if (result != slapSuccess)
      goto epilogue;
  }

  result = slapDecoder_FinalizeFrame(pFileReader->pDecoder, pFileReader->pCurrentFrame, pFileReader->currentFrameSize, pFileReader->pDecodedFrameYUV);

  if (result != slapSuccess)
    goto epilogue;

epilogue:
  return result;
}

slapResult slapFileReader_GetNextFrame(IN slapFileReader * pFileReader)
{
  slapResult result = slapFileReader_ReadNextFrame(pFileReader);

  if (result != slapSuccess)
    return result;

  return slapFileReader_DecodeCurrentFrame(pFileReader);
}

slapResult slapFileReader_RestartVideoStream(IN slapFileReader * pFileReader)
{
  if (pFileReader == NULL)
    return slapError_ArgumentNull;

  pFileReader->frameIndex = 0;
  pFileReader->pDecoder->frameIndex = 0;

  return slapSuccess;
}

slapResult slapFileReader_TransformBufferToBGRA(IN slapFileReader *pFileReader)
{
  if (pFileReader == NULL)
    return slapError_ArgumentNull;

  slapResult result = slapSuccess;

  if (!pFileReader->pDecodedFrameBGRA)
  {
    pFileReader->pDecodedFrameBGRA = slapAlloc(uint32_t, pFileReader->pDecoder->resX * pFileReader->pDecoder->resY);

    if (!pFileReader->pDecodedFrameBGRA)
    {
      result = slapError_MemoryAllocation;
      goto epilogue;
    }
  }

  const int error = tjDecodeYUV(pFileReader->pDecoder->pDecoders[0], (unsigned char *)pFileReader->pDecodedFrameYUV, 1, TJSAMP_420, (unsigned char *)pFileReader->pDecodedFrameBGRA, (int)pFileReader->pDecoder->resX, (int)pFileReader->pDecoder->resX * sizeof(uint32_t), (int)pFileReader->pDecoder->resY, TJPF_BGRA, 0);

  if (error != 0)
  {
    result = slapError_Compress_Internal;
    goto epilogue;
  }

epilogue:
  return result;
}

const void * slapFileReader_GetBufferYUV420(IN slapFileReader *pFileReader)
{
  if (pFileReader == NULL)
    return NULL;

  return pFileReader->pDecodedFrameYUV;
}


const void * slapFileReader_GetBufferBGRA(IN slapFileReader *pFileReader)
{
  if (pFileReader == NULL)
    return NULL;

  return pFileReader->pDecodedFrameBGRA;
}

//////////////////////////////////////////////////////////////////////////
// Core En- & Decoding Functions
//////////////////////////////////////////////////////////////////////////

slapResult _slapCompressChannel(IN void *pData, IN_OUT void **ppCompressedData, IN_OUT size_t *pCompressedDataSize, const size_t width, const size_t height, const int quality, IN void *pCompressor)
{
  unsigned long length = (unsigned long)*pCompressedDataSize;

  if (tjCompress2(pCompressor, (unsigned char *)pData, (int)width, (int)width, (int)height, TJPF_GRAY, (unsigned char **)ppCompressedData, &length, TJSAMP_GRAY, quality, TJFLAG_FASTDCT))
  {
    slapLog(tjGetErrorStr2(pCompressor));
    return slapError_Compress_Internal;
  }

  *pCompressedDataSize = length;

  return slapSuccess;
}

slapResult _slapCompressYUV420(IN void *pData, IN_OUT void **ppCompressedData, IN_OUT size_t *pCompressedDataSize, const size_t width, const size_t height, const int quality, IN void *pCompressor)
{
  unsigned long length = (unsigned long)*pCompressedDataSize;

  if (tjCompressFromYUV(pCompressor, (unsigned char *)pData, (int)width, 32, (int)height, TJSAMP_420, (unsigned char **)ppCompressedData, &length, quality, TJFLAG_FASTDCT))
  {
    slapLog(tjGetErrorStr2(pCompressor));
    return slapError_Compress_Internal;
  }

  *pCompressedDataSize = length;

  return slapSuccess;
}

slapResult _slapDecompressChannel(IN void *pData, IN_OUT void *pCompressedData, const size_t compressedDataSize, const size_t width, const size_t height, IN void *pDecompressor)
{
  if (tjDecompress2(pDecompressor, (unsigned char *)pCompressedData, (unsigned long)compressedDataSize, (unsigned char *)pData, (int)width, (int)width, (int)height, TJPF_GRAY, TJFLAG_FASTDCT))
  {
    slapLog(tjGetErrorStr2(pDecompressor));
    return slapError_Compress_Internal;
  }

  return slapSuccess;
}

slapResult _slapDecompressYUV420(IN void *pData, IN_OUT void *pCompressedData, const size_t compressedDataSize, const size_t width, const size_t height, IN void *pDecompressor)
{
  if (tjDecompressToYUV2(pDecompressor, (unsigned char *)pCompressedData, (unsigned long)compressedDataSize, (unsigned char *)pData, (int)width, 32, (int)height, TJFLAG_FASTDCT))
  {
    slapLog(tjGetErrorStr2(pDecompressor));
    return slapError_Compress_Internal;
  }

  return slapSuccess;
}

void _slapEncodeLastFrameDiff(IN_OUT void *pLastFrame, IN_OUT void *pData, const size_t resX, const size_t resY)
{
  uint8_t *pMainFrameY = (uint8_t *)pData;
  __m128i *pLastFrameYUV = (__m128i *)pLastFrame;

  __m128i *pCB0 = (__m128i *)pMainFrameY;
  __m128i *pLF0 = (__m128i *)pLastFrameYUV;

  __m128i half = _mm_set1_epi8(127);

  const size_t stepSize = 1;
  size_t itX = resX / (sizeof(__m128i) * stepSize);
  size_t itY = resY;

  for (size_t i = 0; i < 3; i++)
  {
    for (size_t y = 0; y < itY; y++)
    {
      for (size_t x = 0; x < itX; x++)
      {
        __m128i cb0 = _mm_load_si128(pCB0);
        __m128i lf0 = _mm_load_si128(pLF0);

        // last frame diff
        cb0 = _mm_add_epi8(_mm_sub_epi8(lf0, cb0), half);

        _mm_store_si128(pCB0, cb0);

        pCB0 += stepSize;
        pLF0 += stepSize;
      }
    }

    if (i == 0)
    {
      itX >>= 1;
      itY >>= 1;
    }

    pCB0 += (stepSize - 1);
    pLF0 += (stepSize - 1);
  }
}

void _slapCopyToLastFrame(IN_OUT void *pData, OUT void *pLastFrame, const size_t resX, const size_t resY)
{
  slapMemcpy(pLastFrame, pData, resX * resY * 3 / 2);
}

void _slapDecodeLastFrameDiff(IN_OUT void *pData, OUT void *pLastFrame, const size_t resX, const size_t resY)
{
  size_t max = (resY * resX) >> 4;

  __m128i *pCB0 = (__m128i *)pData;
  __m128i *pLF0 = (__m128i *)pLastFrame;

  __m128i halfY = _mm_set1_epi8(129);
  __m128i halfUV = _mm_set1_epi8(130);

  for (size_t i = 0; i < max; i++)
  {
    __m128i cb0 = _mm_load_si128(pCB0);
    __m128i lf0 = _mm_load_si128(pLF0);

    lf0 = _mm_sub_epi8(lf0, _mm_add_epi8(cb0, halfY));
    _mm_store_si128(pCB0, lf0);
    _mm_store_si128(pLF0, lf0);

    pCB0++;
    pLF0++;
  }

  max >>= 2;

  for (size_t cchannel = 0; cchannel < 2; cchannel++)
  {
    for (size_t i = 0; i < max; i++)
    {
      __m128i cb0 = _mm_load_si128(pCB0);
      __m128i lf0 = _mm_load_si128(pLF0);

      lf0 = _mm_sub_epi8(lf0, _mm_add_epi8(cb0, halfUV));
      _mm_store_si128(pCB0, lf0);
      _mm_store_si128(pLF0, lf0);

      pCB0++;
      pLF0++;
    }
  }
}
