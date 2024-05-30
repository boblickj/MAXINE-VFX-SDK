SETLOCAL
SET PATH=%PATH%;.\samples\external\opencv\bin;.\bin;
.\build\Release\VideoEffectsAppCLI.exe --model_dir=.\bin\models --in_file=.\samples\input\input1.jpg --out_file=.\output\out.png --effect=ArtifactReduction --mode=1
.\build\Release\VideoEffectsAppCLI.exe --model_dir=.\bin\models --in_file=.\samples\input\input_100_200_frames.mp4 --out_file=.\output\out.mp4 --effect=SuperRes --resolution=2160 --mode=1
