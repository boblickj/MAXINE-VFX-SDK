/*###############################################################################
#
# Copyright (c) 2020 NVIDIA Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
of # this software and associated documentation files (the "Software"), to deal
in # the Software without restriction, including without limitation the rights
to # use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of # the Software, and to permit persons to whom the Software is
furnished to do so, # subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS # FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR # COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER # IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN # CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
#
###############################################################################*/
#include "nvCVOpenCV.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

#define strcasecmp _stricmp
#include <Windows.h>
#include <shellapi.h>

#define BAIL_IF_ERR(err) \
  do {                   \
    if (0 != (err)) {    \
      goto bail;         \
    }                    \
  } while (0)
#define BAIL_IF_NULL(x, err, code) \
  do {                             \
    if ((void *)(x) == NULL) {     \
      err = code;                  \
      goto bail;                   \
    }                              \
  } while (0)

struct FlagInfo {
  bool verbose;
  bool webcam;
  float strength;
  int mode;
  int resolution;
  std::string codec;
  std::string camRes;
};

// Set this when using OTA Updates
// This path is used by nvVideoEffectsProxy.cpp to load the SDK dll
// when using  OTA Updates
char *g_nvVFXSDKPath = NULL;

static bool HasSuffix(const char *str, const char *suf) {
  size_t strSize = strlen(str), sufSize = strlen(suf);
  if (strSize < sufSize) return false;
  return (0 == strcasecmp(suf, str + strSize - sufSize));
}

static bool HasOneOfTheseSuffixes(const char *str, ...) {
  bool matches = false;
  const char *suf;
  va_list ap;
  va_start(ap, str);
  while (nullptr != (suf = va_arg(ap, const char *))) {
    if (HasSuffix(str, suf)) {
      matches = true;
      break;
    }
  }
  va_end(ap);
  return matches;
}

static bool IsImageFile(const char *str) {
  return HasOneOfTheseSuffixes(str, ".bmp", ".jpg", ".jpeg", ".png", nullptr);
}

static bool IsLossyImageFile(const char *str) {
  return HasOneOfTheseSuffixes(str, ".jpg", ".jpeg", nullptr);
}

static const char *DurationString(double sc) {
  static char buf[16];
  int hr, mn;
  hr = (int)(sc / 3600.);
  sc -= hr * 3600.;
  mn = (int)(sc / 60.);
  sc -= mn * 60.;
  snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", hr, mn, sc);
  return buf;
}

struct VideoInfo {
  int codec;
  int width;
  int height;
  double frameRate;
  long long frameCount;
};

static void GetVideoInfo(cv::VideoCapture &reader, const char *fileName,
                         VideoInfo *vinfo, const FlagInfo &finfo) {
  vinfo->codec = (int)reader.get(cv::CAP_PROP_FOURCC);
  vinfo->width = (int)reader.get(cv::CAP_PROP_FRAME_WIDTH);
  vinfo->height = (int)reader.get(cv::CAP_PROP_FRAME_HEIGHT);
  vinfo->frameRate = (double)reader.get(cv::CAP_PROP_FPS);
  vinfo->frameCount = (long long)reader.get(cv::CAP_PROP_FRAME_COUNT);
  if (finfo.verbose)
    printf(
        "       file \"%s\"\n"
        "      codec %.4s\n"
        "      width %4d\n"
        "     height %4d\n"
        " frame rate %.3f\n"
        "frame count %4lld\n"
        "   duration %s\n",
        fileName, (char *)&vinfo->codec, vinfo->width, vinfo->height,
        vinfo->frameRate, vinfo->frameCount,
        DurationString(vinfo->frameCount / vinfo->frameRate));
}

static int StringToFourcc(const std::string &str) {
  union chint {
    int i;
    char c[4];
  };
  chint x = {0};
  for (int n = (str.size() < 4) ? (int)str.size() : 4; n--;) x.c[n] = str[n];
  return x.i;
}

typedef void (*progressCallback)(float percentComplete);

struct FXApp {
  enum Err {
    errQuit = +1,  // Application errors
    errFlag = +2,
    errRead = +3,
    errWrite = +4,
    errNone = NVCV_SUCCESS,  // Video Effects SDK errors
    errGeneral = NVCV_ERR_GENERAL,
    errUnimplemented = NVCV_ERR_UNIMPLEMENTED,
    errMemory = NVCV_ERR_MEMORY,
    errEffect = NVCV_ERR_EFFECT,
    errSelector = NVCV_ERR_SELECTOR,
    errBuffer = NVCV_ERR_BUFFER,
    errParameter = NVCV_ERR_PARAMETER,
    errMismatch = NVCV_ERR_MISMATCH,
    errPixelFormat = NVCV_ERR_PIXELFORMAT,
    errModel = NVCV_ERR_MODEL,
    errLibrary = NVCV_ERR_LIBRARY,
    errInitialization = NVCV_ERR_INITIALIZATION,
    errFileNotFound = NVCV_ERR_FILE,
    errFeatureNotFound = NVCV_ERR_FEATURENOTFOUND,
    errMissingInput = NVCV_ERR_MISSINGINPUT,
    errResolution = NVCV_ERR_RESOLUTION,
    errUnsupportedGPU = NVCV_ERR_UNSUPPORTEDGPU,
    errWrongGPU = NVCV_ERR_WRONGGPU,
    errUnsupportedDriver = NVCV_ERR_UNSUPPORTEDDRIVER,
    errCudaMemory = NVCV_ERR_CUDA_MEMORY,  // CUDA errors
    errCudaValue = NVCV_ERR_CUDA_VALUE,
    errCudaPitch = NVCV_ERR_CUDA_PITCH,
    errCudaInit = NVCV_ERR_CUDA_INIT,
    errCudaLaunch = NVCV_ERR_CUDA_LAUNCH,
    errCudaKernel = NVCV_ERR_CUDA_KERNEL,
    errCudaDriver = NVCV_ERR_CUDA_DRIVER,
    errCudaUnsupported = NVCV_ERR_CUDA_UNSUPPORTED,
    errCudaIllegalAddress = NVCV_ERR_CUDA_ILLEGAL_ADDRESS,
    errCuda = NVCV_ERR_CUDA,
  };

  FXApp() {
    _eff = nullptr;
    _effectName = nullptr;
    _inited = false;
    _showFPS = false;
    _show = false;
    _enableEffect = true, _drawVisualization = true, _framePeriod = 0.f;
  }
  ~FXApp() { NvVFX_DestroyEffect(_eff); }

  void setShow(bool show) { _show = show; }
  Err createEffect(const char *effectSelector, const char *modelDir);
  void destroyEffect();
  NvCV_Status allocBuffers(unsigned width, unsigned height,
                           const FlagInfo &finfo);
  NvCV_Status allocTempBuffers();
  Err processImage(const char *inFile, const char *outFile,
                   const FlagInfo &finfo, progressCallback cb);
  Err processMovie(const char *inFile, const char *outFile,
                   const FlagInfo &finfo, progressCallback cb);
  Err initCamera(cv::VideoCapture &cap, const FlagInfo &finfo);
  Err processKey(int key, const FlagInfo &finfo);
  void drawFrameRate(cv::Mat &img);
  void drawEffectStatus(cv::Mat &img);
  Err appErrFromVfxStatus(NvCV_Status status) { return (Err)status; }
  const char *errorStringFromCode(Err code);

  NvVFX_Handle _eff;
  cv::Mat _srcImg;
  cv::Mat _dstImg;
  NvCVImage _srcGpuBuf;
  NvCVImage _dstGpuBuf;
  NvCVImage _srcVFX;
  NvCVImage _dstVFX;
  NvCVImage _tmpVFX;  // We use the same temporary buffer for source and dst,
                      // since it auto-shapes as needed
  bool _show;
  bool _inited;
  bool _showFPS;
  bool _enableEffect;
  bool _drawVisualization;
  const char *_effectName;
  float _framePeriod;
  std::chrono::high_resolution_clock::time_point _lastTime;
};

const char *FXApp::errorStringFromCode(Err code) {
  struct LutEntry {
    Err code;
    const char *str;
  };
  static const LutEntry lut[] = {
      {errRead, "There was a problem reading a file"},
      {errWrite, "There was a problem writing a file"},
      {errQuit, "The user chose to quit the application"},
      {errFlag, "There was a problem with the command-line arguments"},
  };
  if ((int)code <= 0) return NvCV_GetErrorStringFromCode((NvCV_Status)code);
  for (const LutEntry *p = lut; p != &lut[sizeof(lut) / sizeof(lut[0])]; ++p)
    if (p->code == code) return p->str;
  return "UNKNOWN ERROR";
}

void FXApp::drawFrameRate(cv::Mat &img) {
  const float timeConstant = 16.f;
  std::chrono::high_resolution_clock::time_point now =
      std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> dur =
      std::chrono::duration_cast<std::chrono::duration<float>>(now - _lastTime);
  float t = dur.count();
  if (0.f < t && t < 100.f) {
    if (_framePeriod)
      _framePeriod +=
          (t - _framePeriod) * (1.f / timeConstant);  // 1 pole IIR filter
    else
      _framePeriod = t;
    if (_showFPS) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.1f", 1. / _framePeriod);
      cv::putText(img, buf, cv::Point(10, img.rows - 10),
                  cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 1);
    }
  } else {               // Ludicrous time interval; reset
    _framePeriod = 0.f;  // WAKE UP
  }
  _lastTime = now;
}

FXApp::Err FXApp::processKey(int key, const FlagInfo &finfo) {
  static const int ESC_KEY = 27;
  switch (key) {
    case 'Q':
    case 'q':
    case ESC_KEY:
      return errQuit;
    case 'f':
    case 'F':
      _showFPS = !_showFPS;
      break;
    case 'e':
    case 'E':
      break;
    case 'd':
    case 'D':
      if (finfo.webcam) _drawVisualization = !_drawVisualization;
      break;
    default:
      break;
  }
  return errNone;
}

FXApp::Err FXApp::initCamera(cv::VideoCapture &cap, const FlagInfo &finfo) {
  const int camIndex = 0;
  cap.open(camIndex);
  if (!finfo.camRes.empty()) {
    int camWidth, camHeight, n;
    n = sscanf_s(finfo.camRes.c_str(), "%d%*[xX]%d", &camWidth, &camHeight);
    switch (n) {
      case 2:
        break;  // We have read both width and height
      case 1:
        camHeight = camWidth;
        camWidth = (int)(camHeight * (16. / 9.) + .5);
        break;
      default:
        camHeight = 0;
        camWidth = 0;
        break;
    }

    if (camWidth) cap.set(cv::CAP_PROP_FRAME_WIDTH, camWidth);
    if (camHeight) cap.set(cv::CAP_PROP_FRAME_HEIGHT, camHeight);
    if (camWidth != cap.get(cv::CAP_PROP_FRAME_WIDTH) ||
        camHeight != cap.get(cv::CAP_PROP_FRAME_HEIGHT)) {
      printf("Error: Camera does not support %d x %d resolution\n", camWidth,
             camHeight);
      return errGeneral;
    }
  }
  return errNone;
}

void FXApp::drawEffectStatus(cv::Mat &img) {
  char buf[32];
  snprintf(buf, sizeof(buf), "Effect: %s", _enableEffect ? "on" : "off");
  cv::putText(img, buf, cv::Point(10, img.rows - 40), cv::FONT_HERSHEY_SIMPLEX,
              1, cv::Scalar(255, 255, 255), 1);
}

FXApp::Err FXApp::createEffect(const char *effectSelector,
                               const char *modelDir) {
  NvCV_Status vfxErr;
  BAIL_IF_ERR(vfxErr = NvVFX_CreateEffect(effectSelector, &_eff));
  _effectName = effectSelector;
  // Do not set NVVFX_MODEL_DIRECTORY for NVVFX_FX_SR_UPSCALE feature as it is
  // not a valid selector for that feature
  if (modelDir[0] != '\0' && strcmp(_effectName, NVVFX_FX_SR_UPSCALE)) {
    BAIL_IF_ERR(vfxErr =
                    NvVFX_SetString(_eff, NVVFX_MODEL_DIRECTORY, modelDir));
  }
bail:
  return appErrFromVfxStatus(vfxErr);
}

void FXApp::destroyEffect() {
  NvVFX_DestroyEffect(_eff);
  _eff = nullptr;
}

// Allocate one temp buffer to be used for input and output. Reshaping of the
// temp buffer in NvCVImage_Transfer() is done automatically, and is very low
// overhead. We expect the destination to be largest, so we allocate that first
// to minimize reallocs probablistically. Then we Realloc for the source to get
// the union of the two. This could alternately be done at runtime by feeding in
// an empty temp NvCVImage, but there are advantages to allocating all memory at
// load time.
NvCV_Status FXApp::allocTempBuffers() {
  NvCV_Status vfxErr;
  BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(
                  &_tmpVFX, _dstVFX.width, _dstVFX.height, _dstVFX.pixelFormat,
                  _dstVFX.componentType, _dstVFX.planar, NVCV_GPU, 0));
  BAIL_IF_ERR(vfxErr = NvCVImage_Realloc(
                  &_tmpVFX, _srcVFX.width, _srcVFX.height, _srcVFX.pixelFormat,
                  _srcVFX.componentType, _srcVFX.planar, NVCV_GPU, 0));
bail:
  return vfxErr;
}

static NvCV_Status CheckScaleIsotropy(const NvCVImage *src,
                                      const NvCVImage *dst) {
  if (src->width * dst->height != src->height * dst->width) {
    printf(
        "%ux%u --> %ux%u: different scale for width and height is not "
        "supported\n",
        src->width, src->height, dst->width, dst->height);
    return NVCV_ERR_RESOLUTION;
  }
  return NVCV_SUCCESS;
}

NvCV_Status FXApp::allocBuffers(unsigned width, unsigned height,
                                const FlagInfo &finfo) {
  NvCV_Status vfxErr = NVCV_SUCCESS;

  if (_inited) return NVCV_SUCCESS;

  if (!_srcImg.data) {
    _srcImg.create(height, width, CV_8UC3);  // src CPU
    BAIL_IF_NULL(_srcImg.data, vfxErr, NVCV_ERR_MEMORY);
  }
  if (!strcmp(_effectName, NVVFX_FX_TRANSFER)) {
    _dstImg.create(_srcImg.rows, _srcImg.cols, _srcImg.type());  // dst CPU
    BAIL_IF_NULL(_dstImg.data, vfxErr, NVCV_ERR_MEMORY);
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols,
                                         _srcImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _dstImg.cols,
                                         _dstImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // dst GPU
  } else if (!strcmp(_effectName, NVVFX_FX_ARTIFACT_REDUCTION)) {
    _dstImg.create(_srcImg.rows, _srcImg.cols, _srcImg.type());  // dst CPU
    BAIL_IF_NULL(_dstImg.data, vfxErr, NVCV_ERR_MEMORY);
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols,
                                         _srcImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _dstImg.cols,
                                         _dstImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // dst GPU
  } else if (!strcmp(_effectName, NVVFX_FX_SUPER_RES)) {
    if (!finfo.resolution) {
      printf("--resolution has not been specified\n");
      return NVCV_ERR_PARAMETER;
    }
    BAIL_IF_ERR(vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, finfo.strength));
    int dstWidth = _srcImg.cols * finfo.resolution / _srcImg.rows;
    _dstImg.create(finfo.resolution, dstWidth, _srcImg.type());  // dst CPU
    BAIL_IF_NULL(_dstImg.data, vfxErr, NVCV_ERR_MEMORY);
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols,
                                         _srcImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // src GPU
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _dstImg.cols,
                                         _dstImg.rows, NVCV_BGR, NVCV_F32,
                                         NVCV_PLANAR, NVCV_GPU, 1));  // dst GPU
    BAIL_IF_ERR(vfxErr = CheckScaleIsotropy(&_srcGpuBuf, &_dstGpuBuf));
  } else if (!strcmp(_effectName, NVVFX_FX_SR_UPSCALE)) {
    if (!finfo.resolution) {
      printf("--resolution has not been specified\n");
      return NVCV_ERR_PARAMETER;
    }

    BAIL_IF_ERR(vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, finfo.strength));
    int dstWidth = _srcImg.cols * finfo.resolution / _srcImg.rows;
    _dstImg.create(finfo.resolution, dstWidth, _srcImg.type());  // dst CPU
    BAIL_IF_NULL(_dstImg.data, vfxErr, NVCV_ERR_MEMORY);
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(
                    &_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_RGBA, NVCV_U8,
                    NVCV_INTERLEAVED, NVCV_GPU, 32));  // src GPU
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(
                    &_dstGpuBuf, _dstImg.cols, _dstImg.rows, NVCV_RGBA, NVCV_U8,
                    NVCV_INTERLEAVED, NVCV_GPU, 32));  // dst GPU
    BAIL_IF_ERR(vfxErr = CheckScaleIsotropy(&_srcGpuBuf, &_dstGpuBuf));
  }
  NVWrapperForCVMat(&_srcImg, &_srcVFX);  // _srcVFX is an alias for _srcImg
  NVWrapperForCVMat(&_dstImg, &_dstVFX);  // _dstVFX is an alias for _dstImg

// #define ALLOC_TEMP_BUFFERS_AT_RUN_TIME    // Deferring temp buffer allocation
// is easier
#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME  // Allocating temp buffers at load time
                                        // avoids run time hiccups
  BAIL_IF_ERR(
      vfxErr =
          allocTempBuffers());  // This uses _srcVFX and _dstVFX and allocates
                                // one buffer to be a temporary for src and dst
#endif                          // ALLOC_TEMP_BUFFERS_AT_RUN_TIME

  _inited = true;

bail:
  return vfxErr;
}

FXApp::Err FXApp::processImage(const char *inFile, const char *outFile,
                               const FlagInfo &finfo,
                               progressCallback cb = nullptr) {
  CUstream stream = 0;
  NvCV_Status vfxErr;

  if (!_eff) return errEffect;
  _srcImg = cv::imread(inFile);
  if (!_srcImg.data) return errRead;

  BAIL_IF_ERR(vfxErr = allocBuffers(_srcImg.cols, _srcImg.rows, finfo));

  // Since images are uploaded asynchronously, we may as well do this first.
  BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(
                  &_srcVFX, &_srcGpuBuf, 1.f / 255.f, stream,
                  &_tmpVFX));  // _srcVFX--> _tmpVFX --> _srcGpuBuf
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, stream));
  if (!strcmp(_effectName, NVVFX_FX_ARTIFACT_REDUCTION)) {
    BAIL_IF_ERR(vfxErr =
                    NvVFX_SetU32(_eff, NVVFX_MODE, (unsigned int)finfo.mode));
  } else if (!strcmp(_effectName, NVVFX_FX_SUPER_RES)) {
    BAIL_IF_ERR(vfxErr =
                    NvVFX_SetU32(_eff, NVVFX_MODE, (unsigned int)finfo.mode));
  }

  BAIL_IF_ERR(vfxErr = NvVFX_Load(_eff));
  BAIL_IF_ERR(vfxErr = NvVFX_Run(_eff, 0));  // _srcGpuBuf --> _dstGpuBuf
  BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(
                  &_dstGpuBuf, &_dstVFX, 255.f, stream,
                  &_tmpVFX));  // _dstGpuBuf --> _tmpVFX --> _dstVFX

  if (cb != nullptr) {
    cb(50.f);
  }

  if (outFile && outFile[0]) {
    if (IsLossyImageFile(outFile))
      fprintf(stderr,
              "WARNING: JPEG output file format will reduce image quality\n");

    try {
      cv::imwrite(outFile, _dstImg);
    } catch (...) {
      printf("Error writing: \"%s\"\n", outFile);
      return errWrite;
    }
  }

  if (cb != nullptr) {
    cb(100.f);
  }

  if (_show) {
    cv::imshow("Output", _dstImg);
  }
bail:
  return appErrFromVfxStatus(vfxErr);
}

FXApp::Err FXApp::processMovie(const char *inFile, const char *outFile,
                               const FlagInfo &finfo,
                               progressCallback cb = nullptr) {
  const int fourcc_h264 = cv::VideoWriter::fourcc('H', '2', '6', '4');
  CUstream stream = 0;
  FXApp::Err appErr = errNone;
  bool ok;
  cv::VideoCapture reader;
  cv::VideoWriter writer;
  NvCV_Status vfxErr;
  unsigned frameNum;
  VideoInfo vinfo;

  if (inFile && !inFile[0])
    inFile = nullptr;  // Set file paths to NULL if zero length

  if (!finfo.webcam && inFile) {
    reader.open(inFile);
  } else {
    appErr = initCamera(reader, finfo);
    if (appErr != errNone) return appErr;
  }

  if (!reader.isOpened()) {
    if (!finfo.webcam)
      printf("Error: Could not open video: \"%s\"\n", inFile);
    else
      printf("Error: Webcam not found\n");
    return errRead;
  }

  GetVideoInfo(reader, (inFile ? inFile : "webcam"), &vinfo, finfo);
  if (!(fourcc_h264 == vinfo.codec ||
        cv::VideoWriter::fourcc('a', 'v', 'c', '1') ==
            vinfo.codec))  // avc1 is alias for h264
    printf("Filters only target H264 videos, not %.4s\n", (char *)&vinfo.codec);

  BAIL_IF_ERR(vfxErr = allocBuffers(vinfo.width, vinfo.height, finfo));

  if (outFile && !outFile[0]) outFile = nullptr;
  if (outFile) {
    ok = writer.open(outFile, StringToFourcc(finfo.codec), vinfo.frameRate,
                     cv::Size(_dstVFX.width, _dstVFX.height));
    if (!ok) {
      printf("Cannot open \"%s\" for video writing\n", outFile);
      outFile = nullptr;
      if (!_show) return errWrite;
    }
  }

  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, stream));
  if (!strcmp(_effectName, NVVFX_FX_ARTIFACT_REDUCTION)) {
    BAIL_IF_ERR(vfxErr =
                    NvVFX_SetU32(_eff, NVVFX_MODE, (unsigned int)finfo.mode));
  } else if (!strcmp(_effectName, NVVFX_FX_SUPER_RES)) {
    BAIL_IF_ERR(vfxErr =
                    NvVFX_SetU32(_eff, NVVFX_MODE, (unsigned int)finfo.mode));
  }
  BAIL_IF_ERR(vfxErr = NvVFX_Load(_eff));

  for (frameNum = 0; reader.read(_srcImg); ++frameNum) {
    if (_srcImg.empty()) {
      printf("Frame %u is empty\n", frameNum);
    }

    // _srcVFX   --> _srcTmpVFX --> _srcGpuBuf --> _dstGpuBuf --> _dstTmpVFX -->
    // _dstVFX
    if (_enableEffect) {
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf,
                                              1.f / 255.f, stream, &_tmpVFX));
      BAIL_IF_ERR(vfxErr = NvVFX_Run(_eff, 0));
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, 255.f,
                                              stream, &_tmpVFX));
    } else {
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_srcVFX, &_dstVFX, 1.f / 255.f,
                                              stream, &_tmpVFX));
    }

    if (outFile) writer.write(_dstImg);

    if (_show) {
      drawFrameRate(_dstImg);
      cv::imshow("Output", _dstImg);
      int key = cv::waitKey(1);
      if (key > 0) {
        appErr = processKey(key, finfo);
        if (errQuit == appErr) break;
      }
    }

    if (cb != nullptr) {
      cb(100.f * frameNum / vinfo.frameCount);
    }
  }

  reader.release();
  if (outFile) writer.release();
bail:
  return appErrFromVfxStatus(vfxErr);
}
