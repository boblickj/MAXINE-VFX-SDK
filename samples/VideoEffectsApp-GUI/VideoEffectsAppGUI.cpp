#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>

#include "nvCVOpenCV.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#include <Windows.h>
#else  // !_MSC_VER
#include <sys/stat.h>
#endif  // _MSC_VER

// Set this when using OTA Updates
// This path is used by nvVideoEffectsProxy.cpp to load the SDK dll
// when using  OTA Updates
char *g_nvVFXSDKPath = NULL;

// --model_dir=.\bin\models
// --in_file=.\samples\input\input1.jpg
// --out_file=.\output\out.png
// --effect=ArtifactReduction
// --mode=1

struct Info {
  // The widgets
  Fl_Input *instr;
  Fl_Int_Input *inint;
  Fl_Text_Buffer *disp_infile;
  Fl_Native_File_Chooser *infile;

  // Saved values
  char sval[255];
  int ival;
};

// Callback for the done button
void done_cb(Fl_Widget *w, void *param) {
  Info *input = reinterpret_cast<Info *>(param);

  // Get the values from the widgets
  strcpy_s(input->sval, input->instr->value());
  input->ival = atoi(input->inint->value());

  // Print the values
  printf("String value is %s\n", input->sval);
  printf("Integer value is %d\n", input->ival);
  printf("File value is %s\n", input->disp_infile->text());
}

// Callback for the output button
void output_cb(Fl_Widget *w, void *param) {
  Info *input = reinterpret_cast<Info *>(param);

  switch (input->infile->show()) {
    case -1:  // ERROR
      fprintf(stderr, "ERROR: %s\n", input->infile->errmsg());
      break;
    case 1:  // CANCEL
      fprintf(stderr, "*** CANCEL\n");
      fl_beep();
      break;
    default:  // PICKED FILE
      if (input->infile->filename()) {
        input->disp_infile->text(input->infile->filename());
      }
      break;
  }
}

int main(int argc, char **argv) {
  Info input;

  // Setup the colours
  Fl::args(argc, argv);
  Fl::get_system_colors();

  // Create the window
  Fl_Window *window = new Fl_Window(500, 500);
  int x = 50, y = 10, w = 400, h = 30;
  input.instr = new Fl_Input(x, y, w, h, "Str");
  input.instr->tooltip("String input");

  y += 35;
  input.inint = new Fl_Int_Input(x, y, w, h, "Int");
  input.inint->tooltip("Integer input");

  y += 75;
  Fl_Text_Display *disp_infile =
      new Fl_Text_Display(x, y, w, 50, "Output Directory");
  input.disp_infile = new Fl_Text_Buffer();
  disp_infile->buffer(input.disp_infile);

  y += 50;
  input.infile =
      new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
  input.infile->directory("C:\\");
  input.infile->title("Select an Output Directory");

  Fl_Button *output_folder =
      new Fl_Button(x, y, 100, h, "Select Output Directory");
  output_folder->callback(output_cb, &input);

  y += 75;
  Fl_Button *done = new Fl_Button(x, y, 100, h, "Done");
  done->callback(done_cb, &input);
  window->end();

  window->show(argc, argv);
  return Fl::run();
}
