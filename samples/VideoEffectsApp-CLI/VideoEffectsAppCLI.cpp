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
#include "Converter.cpp"
#include "nvVideoEffects.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#include <Windows.h>
#else  // !_MSC_VER
#include <sys/stat.h>
#endif  // _MSC_VER

#define NVCV_ERR_HELP 411

#ifdef _WIN32
#define DEFAULT_CODEC "avc1"
#else  // !_WIN32
#define DEFAULT_CODEC "H264"
#endif  // _WIN32

bool FLAG_debug = false, FLAG_verbose = false, FLAG_show = false,
     FLAG_progress = false, FLAG_webcam = false;
float FLAG_strength = 0.f;
int FLAG_mode = 0;
int FLAG_resolution = 0;
std::string FLAG_codec = DEFAULT_CODEC, FLAG_camRes = "1280x720", FLAG_inFile,
            FLAG_outFile, FLAG_outDir, FLAG_modelDir, FLAG_effect;

static bool GetFlagArgVal(const char* flag, const char* arg, const char** val) {
  if (*arg != '-') return false;
  while (*++arg == '-') continue;
  const char* s = strchr(arg, '=');
  if (s == NULL) {
    if (strcmp(flag, arg) != 0) return false;
    *val = NULL;
    return true;
  }
  size_t n = s - arg;
  if ((strlen(flag) != n) || (strncmp(flag, arg, n) != 0)) return false;
  *val = s + 1;
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, std::string* val) {
  const char* valStr;
  if (!GetFlagArgVal(flag, arg, &valStr)) return false;
  val->assign(valStr ? valStr : "");
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, bool* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) {
    *val = (valStr == NULL || strcasecmp(valStr, "true") == 0 ||
            strcasecmp(valStr, "on") == 0 || strcasecmp(valStr, "yes") == 0 ||
            strcasecmp(valStr, "1") == 0);
  }
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, float* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtof(valStr, NULL);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, long* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtol(valStr, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, int* val) {
  long longVal;
  bool success = GetFlagArgVal(flag, arg, &longVal);
  if (success) *val = (int)longVal;
  return success;
}

static void Usage() {
  printf(
      "VideoEffectsApp [args ...]\n"
      "  where args is:\n"
      "  --in_file=<path>           input file to be processed\n"
      "  --webcam                   use a webcam as the input\n"
      "  --out_file=<path>          output file to be written\n"
      "  --effect=<effect>          the effect to apply\n"
      "  --show                     display the results in a window (for "
      "webcam, it is always true)\n"
      "  --strength=<value>         strength of the upscaling effect, [0.0, "
      "1.0]\n"
      "  --mode=<value>             mode of the super res or artifact "
      "reduction effect, 0 or 1, \n"
      "                             where 0 - conservative and 1 - aggressive\n"
      "  --cam_res=[WWWx]HHH        specify camera resolution as height or "
      "width x height\n"
      "                             supports 720 and 1080 resolutions (default "
      "\"720\") \n"
      "  --resolution=<height>      the desired height of the output\n"
      "  --model_dir=<path>         the path to the directory that contains "
      "the models\n"
      "  --codec=<fourcc>           the fourcc code for the desired codec "
      "(default " DEFAULT_CODEC
      ")\n"
      "  --progress                 show progress\n"
      "  --verbose                  verbose output\n"
      "  --debug                    print extra debugging information\n");
  const char* cStr;
  NvCV_Status err = NvVFX_GetString(nullptr, NVVFX_INFO, &cStr);
  if (NVCV_SUCCESS != err)
    printf("Cannot get effects: %s\n", NvCV_GetErrorStringFromCode(err));
  printf("where effects are:\n%s", cStr);
}

static int ParseMyArgs(int argc, char** argv) {
  int errs = 0;
  for (--argc, ++argv; argc--; ++argv) {
    bool help;
    const char* arg = *argv;
    if (arg[0] != '-') {
      continue;
    } else if ((arg[1] == '-') &&
               (GetFlagArgVal("verbose", arg, &FLAG_verbose) ||
                GetFlagArgVal("in", arg, &FLAG_inFile) ||
                GetFlagArgVal("in_file", arg, &FLAG_inFile) ||
                GetFlagArgVal("out", arg, &FLAG_outFile) ||
                GetFlagArgVal("out_file", arg, &FLAG_outFile) ||
                GetFlagArgVal("effect", arg, &FLAG_effect) ||
                GetFlagArgVal("show", arg, &FLAG_show) ||
                GetFlagArgVal("webcam", arg, &FLAG_webcam) ||
                GetFlagArgVal("cam_res", arg, &FLAG_camRes) ||
                GetFlagArgVal("strength", arg, &FLAG_strength) ||
                GetFlagArgVal("mode", arg, &FLAG_mode) ||
                GetFlagArgVal("resolution", arg, &FLAG_resolution) ||
                GetFlagArgVal("model_dir", arg, &FLAG_modelDir) ||
                GetFlagArgVal("codec", arg, &FLAG_codec) ||
                GetFlagArgVal("progress", arg, &FLAG_progress) ||
                GetFlagArgVal("debug", arg, &FLAG_debug))) {
      continue;
    } else if (GetFlagArgVal("help", arg, &help)) {
      return NVCV_ERR_HELP;
    } else if (arg[1] != '-') {
      for (++arg; *arg; ++arg) {
        if (*arg == 'v') {
          FLAG_verbose = true;
        } else {
          printf("Unknown flag ignored: \"-%c\"\n", *arg);
        }
      }
      continue;
    } else {
      printf("Unknown flag ignored: \"%s\"\n", arg);
    }
  }
  return errs;
}

void consoleCallback(float percentComplete) {
  fprintf(stderr, "\b\b\b\b%3.0f%%", percentComplete);
}

int main(int argc, char** argv) {
  FXApp::Err fxErr = FXApp::errNone;
  int nErrs;
  FXApp app;

  nErrs = ParseMyArgs(argc, argv);
  if (nErrs) std::cerr << nErrs << " command line syntax problems\n";

  if (FLAG_verbose) {
    const char* cstr = nullptr;
    NvVFX_GetString(nullptr, NVVFX_INFO, &cstr);
    std::cerr << "Effects:" << std::endl << cstr << std::endl;
  }
  if (FLAG_webcam) {
    // If webcam is on, enable showing the results and turn off displaying the
    // progress
    if (FLAG_progress) FLAG_progress = !FLAG_progress;
    if (!FLAG_show) FLAG_show = !FLAG_show;
  }
  if (FLAG_inFile.empty() && !FLAG_webcam) {
    std::cerr << "Please specify --in_file=XXX or --webcam=true\n";
    ++nErrs;
  }
  if (FLAG_outFile.empty() && !FLAG_show) {
    std::cerr << "Please specify --out_file=XXX or --show\n";
    ++nErrs;
  }
  if (FLAG_effect.empty()) {
    std::cerr << "Please specify --effect=XXX\n";
    ++nErrs;
  }

  FlagInfo finfo;
  finfo.camRes = FLAG_camRes;
  finfo.codec = FLAG_codec;
  finfo.mode = FLAG_mode;
  finfo.resolution = FLAG_resolution;
  finfo.strength = FLAG_strength;
  finfo.verbose = FLAG_verbose;
  finfo.webcam = FLAG_webcam;

  app.setShow(FLAG_show);

  if (nErrs) {
    Usage();
    fxErr = FXApp::errFlag;
  } else {
    fxErr = app.createEffect(FLAG_effect.c_str(), FLAG_modelDir.c_str());
    if (FXApp::errNone != fxErr) {
      std::cerr << "Error creating effect \"" << FLAG_effect << "\"\n";
    } else {
      if (IsImageFile(FLAG_inFile.c_str()))
        fxErr = app.processImage(FLAG_inFile.c_str(), FLAG_outFile.c_str(),
                                 finfo, *consoleCallback);
      else
        fxErr = app.processMovie(FLAG_inFile.c_str(), FLAG_outFile.c_str(),
                                 finfo, *consoleCallback);
    }
  }

  if (fxErr)
    std::cerr << "Error: " << app.errorStringFromCode(fxErr) << std::endl;
  return (int)fxErr;
}
