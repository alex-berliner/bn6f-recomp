# launch.ps1 -Job "SCRIPT ARGS": start tools/vidsync/remote/SCRIPT in WSL through WMI, so it outlives the SSH
# session that asked for it (SSH sessions to Windows kill their children). deploy.py copies this file to
# C:/Users/alex/vidsync_launch.ps1; the workflows call it as
#   powershell -NoProfile -ExecutionPolicy Bypass -File C:/Users/alex/vidsync_launch.ps1 -Job "job_driver.sh RUN START"
param([Parameter(Mandatory = $true)][string]$Job)
$line = "wsl.exe -d Ubuntu -- bash -c `"bash ~/vidsync/recomp/tools/vidsync/remote/$Job`""
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $line }
"launched $($r.ProcessId) rc $($r.ReturnValue)"
