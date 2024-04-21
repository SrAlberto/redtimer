$project = "C:\Users\49702566A\Documents\Proyectos\RedTimer"

qmake.exe $project\redtimer.pro -spec win32-g++ "CONFIG+=qml_debug" "CONFIG+=qtquickcompiler" "CONFIG+=force_debug_info" "CONFIG+=separate_debug_info"
mingw32-make.exe qmake_all
mingw32-make.exe -j20

$dist = "$project\dist\build-redtimer-Desktop_Qt_5_15_2_MinGW_64_bit-Profile"

cp $dist\libqtredmine\release\qtredmine.dll $dist\gui\release

& $dist\gui\release\redtimer.exe