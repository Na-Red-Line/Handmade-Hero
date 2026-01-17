# flags
$DBG = @()
$WARN = @()
$MACRO = @()
$isDLL = $false

switch -Wildcard ($args) {
	'-clean' {
		if (Test-Path 'build\debug') { Remove-Item -Recurse -Force 'build\debug' }
		New-Item -Path 'build\debug' -ItemType Directory -Force | Out-Null
		exit 0
	}
	'-debug' { $DBG = @('-g','-gcodeview','-O0') }
	'-dll' { $isDLL = $true }
	'-warn' { $WARN = @(
			        '-Wall',
			        '-Wextra',
			        '-Wshadow',
			        '-isystem','D:/Windows Kits/10/Include/10.0.26100.0/um/',
			        '-isystem','D:/Windows Kits/10/Include/10.0.26100.0/shared/'
							) }
	default { if ($_ -like '-D*') { $MACRO += $_ } }
}

$CXXFLAGS = @('-std=gnu++17','-fno-exceptions','-fno-rtti')

Push-Location 'build\debug'

# build flags (concatenate; empty arrays are fine)
$common = $CXXFLAGS + $DBG + $WARN + $MACRO

# find sources from repo path
$rootPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$winPath = $rootPath + '\winHandmade.cc'
$handmadePath =  $rootPath + '\handmade.cc'

if (-not $isDLL) {
	$LIBS = @('-luser32','-lgdi32','-lXinput','-lWinmm')
	$XLINKER = @('-Wl,/SUBSYSTEM:WINDOWS,/OPT:REF,/OPT:ICF')
	$CC = $common + $LIBS + $XLINKER + @($winPath,'-o','HandmadeHero.exe')
	& clang++ $CC
} else {
	$XLINKER = @('-Wl,/EXPORT:gameUpdateAndRender,/EXPORT:gameGetSoundSamples')
	$CC = $common + $XLINKER + @('-shared',$handmadePath,'-o','handmade.dll')
	& clang++ $CC
}

Pop-Location
