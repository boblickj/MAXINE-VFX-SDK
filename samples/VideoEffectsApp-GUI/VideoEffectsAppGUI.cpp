#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <string>
#include <iostream>

#include "nvCVOpenCV.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>


#ifdef _MSC_VER
  #define strcasecmp _stricmp
  #include <Windows.h>
#else // !_MSC_VER
  #include <sys/stat.h>
#endif // _MSC_VER


// Set this when using OTA Updates
// This path is used by nvVideoEffectsProxy.cpp to load the SDK dll
// when using  OTA Updates
char *g_nvVFXSDKPath = NULL;

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(300,180);
  Fl_Box *box = new Fl_Box(20,40,260,100,"Hello, World!");
  box->box(FL_UP_BOX);
  box->labelsize(36);
  box->labelfont(FL_BOLD+FL_ITALIC);
  box->labeltype(FL_SHADOW_LABEL);
  window->end();
  window->show(argc, argv);
  return Fl::run();
}