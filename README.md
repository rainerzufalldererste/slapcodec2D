# slapcodec2D
A simple video codec. Derived from [slapcodec](https://github.com/rainerzufalldererste/slapcodec). Written in C for the purpose of easily playing back compressed (non-stereoscopic) videos on a single thread in high performance applications. Image compression based on JPEG ([libjpeg-turbo](https://libjpeg-turbo.org/)).

### Main Features
- Very simple API
- Very small code base
- Runs on a single thread
- Comes with a few simple examples (encoder, decoder, asynchronous decoder)
- Licensed under [MIT](https://opensource.org/licenses/MIT) (Apart from the encoder example which is licensed under [GPLv3](https://www.gnu.org/licenses/quick-guide-gplv3.html) because it includes [ffmpeg](https://www.ffmpeg.org/)
- SIMD for intra frame en/decoding (We don't recommend using intra frame coding at this point in time, because it can create very noticable artifacts.)
- Allows random access of frames if not using intra frame coding (`IntraFrameStep` = 1)

### Decoder Benchmark
 - decoder example can be found in `examples/decoder`
 - single threaded
 - no pre-loading
 - rendering through SDL2 to a window without scaling
 - reading the video file from disk
 - Running on an `Intel(R) Core(TM) i5-3470S CPU @ 2.90GHz`
 - Compiled with the `Visual Studio 2015` toolset in `Visual Studio 2017`.

Resolution | Synchronous Decoding Speed | Filesize / Frame Count | `IntraFrameStep` | Content | Quality / IFrameQuality
-- | -- | -- | -- | -- | --
424x240 | ~2120 Frames per Second | ~5 kB / Frame | 1 | video game (dark) | 20 / -
424x240 | ~1905 Frames per Second | ~7.5 kB / Frame | 1 | same video game (dark) | 50 / -
424x240 | ~1735 Frames per Second | ~11 kB / Frame | 1 | same video game (dark) | 75 / -
848x480 | ~506 Frames per Second | ~37 kB / Frame | 1 | video game (colorful) | 50 / -
848x480 | ~451 Frames per Second | ~55 kB / Frame | 1 | same video game (colorful) | 75 / -
960x720 | ~238 Frames per Second | ~25 kB / Frame | 1 | static video footage | 75 / -
1280x720 | ~250 Frames per Second | ~56 kB / Frame | 1 | animation | 50 / -
1280x720 | ~244 Frames per Second | ~78.5 kB / Frame | 1 | same animation | 75 / -
1440x1080 | ~148 Frames per Second | ~101.5 kB / Frame | 1 | very shaky video footage | 75 / -
1920x1080 | ~112 Frames per Second | ~110.5 kB / Frame | 1 | animation / video footage | 50 / -
1920x1080 | ~107 Frames per Second | ~160.5 kB / Frame | 1 | same animation / video footage | 75 / -
7680x3840 | ~8 Frames per Second | ~2.5 MB / Frame | 1 | static video footage | 50 / -
7680x3840 | ~7 Frames per Second | ~3.5 MB / Frame | 1 | same static video footage | 75 / -

### Setup
```
git clone https://github.com/rainerzufalldererste/slapcodec2D.git
cd slapcodec2D
git submodule update --init --recursive
create_project.bat
```
Choose your preferred compiler toolset
```
MSBuild /p:Configuration=Release /nologo /v:m
```