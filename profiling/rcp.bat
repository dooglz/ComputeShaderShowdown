set rcp="G:/RadeonComputeProfiler/bin/rcprof-x64.exe"
set out="rcp_out"
set wd="/"
set app="../../../BUILD/shader_showdown/bin/Release/MAIN.exe"
set now=%DATE:~4,2%_%DATE:~7,2%_%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%_
REM assume dev 0 is AMDGPU
set appdevice=0

%app% --device %appdevice% --cl --codeXL

mkdir %out% 2> NUL

echo collect OpenCL performance counters
%rcp% -o "%out%\%now%perfcounters.csv" -p -w "%wd%" "%app%" --device %appdevice% --cl --codeXL

echo collect an OpenCL API trace:
%rcp% -o "%out%\%now%apiTrace.atp" -t -w "%wd%" "%app%" --device %appdevice% --cl --codeXL

echo collect an OpenCL API trace:
%rcp% -o "%out%\%now%.occupancy" -O -w "%wd%" "%app%" --device %appdevice% --cl --codeXL

echo generate summary pages
%rcp% -a "%out%\%now%apiTrace.atp"  -T

echo generate  occupancy display page
%rcp% -P "%out%\%now%.occupancy" --occupancyindex 2 -o "%out%\%now%occupancy.html"