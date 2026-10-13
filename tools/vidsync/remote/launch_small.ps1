$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = "wsl.exe -d Ubuntu -- bash /mnt/c/Users/alex/job_small.sh" }
"launched $($r.ProcessId) rc $($r.ReturnValue)"
