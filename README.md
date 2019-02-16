# slapcodec2D
A simple YUV420 JPEG ([libjpeg-turbo](https://libjpeg-turbo.org/)) based derivate from [slapcodec](https://github.com/rainerzufalldererste/slapcodec) written in C for the purpose of easily playing back compressed (non-stereoscopic) videos on a single thread in high performance applications.

### Main Features
- Very simple API
- SIMD for intra frame en/decoding
- Runs on a single thread
- Licensed under [MIT](https://opensource.org/licenses/MIT) (Apart from the encoder example which is licensed under [GPLv3](https://www.gnu.org/licenses/quick-guide-gplv3.html) because it includes [ffmpeg](https://www.ffmpeg.org/)

### Decoder Benchmark
 - decoder example can be found in `examples/decoder`
 - single threaded
 - no pre-loading
 - rendering through SDL2 to a window without scaling
 - reading the video file from disk
 - Running on an `Intel(R) Core(TM) i5-3470S CPU @ 2.90GHz`
 - Compiled with the `Visual Studio 2015` toolset in `Visual Studio 2017`.

Resolution | Synchronous Decoding Speed | Filesize / Frame Count | `IntraFrameStep` | Content
-- | -- | -- | -- | --
424x240 | ~1735 Frames per Second | ~11 kB / Frame | - | video game (dark)
848x480 | ~451 Frames per Second | ~55 kB / Frame | - | video game (colorful)
960x720 | ~238 Frames per Second | ~25 kB / Frame | - | static video footage
1280x720 | ~231 Frames per Second | ~78.5 kB / Frame | - | animation
1440x1080 | ~145 Frames per Second | ~101.5 kB / Frame | 3 | very shaky video footage
1920x1080 | ~107 Frames per Second | ~160.5 kB / Frame | - | animation / video footage
7680x3840 | ~7 Frames per Second | ~3.5 MB / Frame | - | static video footage

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