if (Test-Path "build\debug") { Remove-Item -Path "build\debug" -Recurse -Force }
New-Item -Path "build\debug" -ItemType Directory -Force | Out-Null

$DBG = @()
$WARN = @()
$MACRO = @()

foreach ($arg in $args) {
	if ($arg -eq "-debug") {
		$DBG = @("-g", "-gcodeview", "-O0")
	}
	elseif ($arg -eq "-warn") {
		$WARN = @(
			'-Weverything',
			'-pedantic',
			'-isystem',
			'D:/Windows Kits/10/Include/10.0.26100.0/um/',
			'-isystem',
			'D:/Windows Kits/10/Include/10.0.26100.0/shared/',
			'-Wno-c++98-compat',
			'-Wno-c++98-compat-pedantic',
			'-Wno-gnu',
			'-Wno-gnu-anonymous-struct',
			'-Wno-nested-anon-types',
			'-Wno-gnu-offsetof-extensions',
			'-Wno-zero-as-null-pointer-constant',
			'-Wno-cast-qual', '-Wno-cast-align',
			'-Wno-old-style-cast',
			'-Wno-cast-function-type-strict',
			'-Wno-missing-prototypes',
			'-Wno-global-constructors',
			'-Wno-unsafe-buffer-usage',
			'-Wno-unsafe-buffer-usage-in-libc-call',
			'-Wno-nonportable-system-include-path'
		)
	}
	elseif ($arg -like "-D*") {
		# -DHANDMADE_INTERNAL
		$MACRO += $arg
	}
}

$CXXFLAGS = @("-std=gnu++14", "-fno-exceptions", "-fno-rtti")
$LIBS = @("-luser32", "-lgdi32", "-lXinput", "-lWinmm")
$XLINKER = @("-Wl,/SUBSYSTEM:WINDOWS,/OPT:REF,/OPT:ICF")

Get-ChildItem -Recurse -Filter *.cc |
ForEach-Object { '"{0}"' -f ($_.FullName -replace '\\', '/') } |
Out-File "build\debug\sources.rsp" -Force

Push-Location "build\debug"

$CC = @()

$CC += $CXXFLAGS
$CC += $LIBS
$CC += $XLINKER

if ($DBG.Count -gt 0) { $CC += $DBG }
if ($WARN.Count -gt 0) { $CC += $WARN }
if ($MACRO.Count -gt 0) { $CC += $MACRO }

$CC += "@sources.rsp"
$CC += "-o"
$CC += "HandmadeHero.exe"

& clang++ @CC

Pop-Location
