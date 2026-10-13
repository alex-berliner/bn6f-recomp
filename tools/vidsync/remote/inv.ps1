$c = Get-CimInstance Win32_Processor
"CPU: $($c.Name) cores=$($c.NumberOfCores) threads=$($c.NumberOfLogicalProcessors)"
"RAM GB: " + [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB)
"Free disk C GB: " + [math]::Round((Get-PSDrive C).Free/1GB)
foreach ($t in "wsl","python","py","gcc","git","ffmpeg","uv","choco","winget","scoop") { $x = Get-Command $t -ErrorAction SilentlyContinue; "$t : $($x.Source)" }
"WSL:"; wsl.exe -l -v
