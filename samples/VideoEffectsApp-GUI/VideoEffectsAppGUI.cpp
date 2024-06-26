// generated by Fast Light User Interface Designer (fluid) version 1.0400

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Value_Input.H>

#include <filesystem>

#include "Converter.cpp"


#define DEFAULT_CODEC "avc1"

namespace fs = std::filesystem;

Fl_Double_Window *mainWindow = (Fl_Double_Window *)0;

Fl_Choice *effetChoice = (Fl_Choice *)0;

Fl_Check_Button *modeInput = (Fl_Check_Button *)0;

Fl_Value_Input *resolutionInput = (Fl_Value_Input *)0;

Fl_Native_File_Chooser *fileChooser = (Fl_Native_File_Chooser *)0;
Fl_Native_File_Chooser *directoryChooser = (Fl_Native_File_Chooser *)0;

Fl_Button *modelFolderButton = (Fl_Button *)0;
Fl_Output *modelFolderOutput = (Fl_Output *)0;

Fl_Button *inputFileButton = (Fl_Button *)0;
Fl_Output *inputFileOutput = (Fl_Output *)0;

Fl_Button *outputFolderButton = (Fl_Button *)0;
Fl_Output *outputFolderOutput = (Fl_Output *)0;

Fl_Button *submitButton = (Fl_Button *)0;
Fl_Progress *conversionProgrees = (Fl_Progress *)0;

static void sendAlert(const char *message) {
  fl_alert(message);
  fl_beep();
}

static void showFileChooser(Fl_Native_File_Chooser *targetChooser,
                            Fl_Output *targetOutput) {
  switch (targetChooser->show()) {
    case -1:  // ERROR
      sendAlert(targetChooser->errmsg());
      break;
    case 1:  // CANCEL
      break;
    default:  // PICKED FILE
      if (targetChooser->filename()) {
        targetOutput->value(targetChooser->filename());
      }
      break;
  }
}

static void cb_modelFolderButton(Fl_Button *, void *) {
  directoryChooser->directory(
      "C:\\");
  directoryChooser->title("Select the Model Directory");

  showFileChooser(directoryChooser, modelFolderOutput);
}

static void cb_inputFileButton(Fl_Button *, void *) {
  fileChooser->title("Select the Input File");

  showFileChooser(fileChooser, inputFileOutput);
}

static void cb_outputFolderButton(Fl_Button *, void *) {
  directoryChooser->directory(
      "C:\\");
  directoryChooser->title("Select the Output Directory");

  showFileChooser(directoryChooser, outputFolderOutput);
}

static void deactivateGUI() {
  effetChoice->deactivate();
  modeInput->deactivate();
  resolutionInput->deactivate();
  modelFolderButton->deactivate();
  inputFileButton->deactivate();
  outputFolderButton->deactivate();
}

static void activateGUI() {
  effetChoice->activate();
  modeInput->activate();
  resolutionInput->activate();
  modelFolderButton->activate();
  inputFileButton->activate();
  outputFolderButton->activate();
}

void cb_guiUpdateProgress(float percentComplete) {
  conversionProgrees->value(percentComplete);
  Fl::check();
}

static void cb_submitButton(Fl_Button *, void *) {
  FXApp::Err fxErr = FXApp::errNone;
  int nErrs = 0;
  FXApp app;

  deactivateGUI();

  if (effetChoice->text() == nullptr) {
    sendAlert("Efect Cannot be Empty!");
    ++nErrs;
  }

  if ((effetChoice->text() == "SuperRes" or
       effetChoice->text() == "Upscale") and
      resolutionInput->value() == 0) {
    sendAlert("Resolution cannot be 0 when effect is SuperRes or Upscale!");
    ++nErrs;
  }

  if (modelFolderOutput->value() == "") {
    sendAlert("Model Directory Cannot be Empty!");
    ++nErrs;
  }

  if (inputFileOutput->value() == "") {
    sendAlert("Input File Cannot be Empty!");
    ++nErrs;
  }

  if (outputFolderOutput->value() == "") {
    sendAlert("Output Directory Cannot be Empty!");
    ++nErrs;
  }

  if (nErrs) {
    activateGUI();
    return;
  }

  FlagInfo finfo;
  finfo.camRes = "1280x720";
  finfo.codec = DEFAULT_CODEC;
  finfo.mode = int(modeInput->value());
  finfo.resolution = int(resolutionInput->value());
  finfo.strength = 0.f;
  finfo.verbose = false;
  finfo.webcam = false;

  std::string outputFullPath = std::format(
      "{}\\{}", outputFolderOutput->value(),
      fs::path(inputFileOutput->value()).filename().string().c_str());

  fxErr = app.createEffect(effetChoice->text(), modelFolderOutput->value());
  if (FXApp::errNone == fxErr) {
    if (IsImageFile(inputFileOutput->value())) {
      app.setShow(true);
      fxErr = app.processImage(inputFileOutput->value(), outputFullPath.c_str(),
                               finfo, *cb_guiUpdateProgress);
    } else {
      app.setShow(false);
      fxErr = app.processMovie(inputFileOutput->value(), outputFullPath.c_str(),
                               finfo, *cb_guiUpdateProgress);
    }
  }

  if (fxErr) {
    sendAlert(app.errorStringFromCode(fxErr));
  }

  sendAlert("Conversion Complete!");
  conversionProgrees->value(0.f);
  activateGUI();

  ShellExecuteA(NULL, "open", outputFolderOutput->value(), NULL, NULL,
                SW_SHOWDEFAULT);
}

int main(int argc, char **argv) {
  mainWindow = new Fl_Double_Window(344, 475, "NVIDIA Maxine SDK");

  effetChoice = new Fl_Choice(115, 23, 150, 20, "Effect");
  effetChoice->down_box(FL_BORDER_BOX);
  effetChoice->add("ArtifactReduction");
  effetChoice->add("SuperRes");
  effetChoice->add("Upscale");

  modeInput = new Fl_Check_Button(228, 365, 20, 20, "Low Quaility Source");
  modeInput->down_box(FL_DOWN_BOX);
  modeInput->align(Fl_Align(FL_ALIGN_LEFT));

  resolutionInput = new Fl_Value_Input(153, 55, 88, 20, "Target Resolution");
  resolutionInput->tooltip(
      "Must be 1.33x, 1.5x, 2x, 3x, or 4x of input resolution");

  Fl_Box *box1 = new Fl_Box(9, 95, 320, 84);
  box1->box(FL_BORDER_FRAME);
  box1->color(FL_GRAY0);

  // BROWSE_MULTI_FILE
  fileChooser = new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_FILE);

  directoryChooser =
      new Fl_Native_File_Chooser(Fl_Native_File_Chooser::BROWSE_DIRECTORY);

  modelFolderButton = new Fl_Button(110, 108, 118, 28, "Model Directory");
  modelFolderButton->callback((Fl_Callback *)cb_modelFolderButton);

  modelFolderOutput = new Fl_Output(21, 144, 300, 20);

  Fl_Box *box2 = new Fl_Box(9, 179, 320, 84);
  box2->box(FL_BORDER_FRAME);
  box2->color(FL_GRAY0);

  inputFileButton = new Fl_Button(116, 187, 112, 28, "Input File");
  inputFileButton->callback((Fl_Callback *)cb_inputFileButton);

  inputFileOutput = new Fl_Output(21, 223, 300, 20);

  Fl_Box *box3 = new Fl_Box(9, 263, 320, 84);
  box3->box(FL_BORDER_FRAME);
  box3->color(FL_GRAY0);

  outputFolderButton = new Fl_Button(116, 271, 118, 28, "Output Directory");
  outputFolderButton->callback((Fl_Callback *)cb_outputFolderButton);

  outputFolderOutput = new Fl_Output(21, 307, 300, 24);

  submitButton = new Fl_Button(30, 411, 93, 36, "Convert");
  submitButton->callback((Fl_Callback *)cb_submitButton);

  conversionProgrees =
      new Fl_Progress(153, 411, 168, 35, "Conversion Progrees");

  mainWindow->end();
  mainWindow->show(argc, argv);
  return Fl::run();
}
