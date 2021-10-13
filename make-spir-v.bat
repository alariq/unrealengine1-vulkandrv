SET InputPath=%1
SET OutputPath=%2

echo hello
echo %VK_SDK_PATH%\Bin\glslangValidator.exe -V %InputPath% -o %OutputPath%.spv.bin
%VK_SDK_PATH%\Bin\glslangValidator.exe -V %InputPath% -o %OutputPath%.spv.bin
