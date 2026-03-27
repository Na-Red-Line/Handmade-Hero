$COMMON_COMPILER_FLAGS = @('-std=gnu++14','-fno-exceptions','-fno-rtti','-g','-gcodeview','-O0','-DHANDMADE_INTERNAL=1','-DRECORD=1')

$WARN = @('-Wall','-Wextra','-Wshadow',
					'-Wno-gnu-anonymous-struct',
					'-Wno-unused-parameter',
					'-Wno-unused-variable',
					'-Wno-unused-function',
					'-Wno-missing-field-initializers',
			    '-isystem','D:/Windows Kits/10/Include/10.0.26100.0/um/',
			    '-isystem','D:/Windows Kits/10/Include/10.0.26100.0/shared/')

if ($args[0] -eq "-clean") {
	if (Test-Path 'build\debug') { Remove-Item -Recurse -Force 'build\debug' }
	New-Item -Path 'build\debug' -ItemType Directory -Force | Out-Null
	exit 0
}

$INPUT_PATH =  if ($args[0] -eq "-dll") { (Get-Location).Path + '\handmade.cc' } else { (Get-Location).Path + '\winHandmade.cc' }

Push-Location 'build\debug'

if ($args[0] -eq "-dll") {
	Set-Content -Path 'lock.tmp' -Value '' -NoNewline
	$XLINKER = @('-Wl,/EXPORT:gameUpdateAndRender,/EXPORT:gameGetSoundSamples')
	$PDB_NAME = "handmade_$(Get-Random).pdb"
	$XLINKER += @('-Wl,/PDB:' + $PDB_NAME)
	$CC = $COMMON_COMPILER_FLAGS + $WARN + $XLINKER + @('-shared',$INPUT_PATH,'-o','handmade.dll')
	& clang++ $CC
	Remove-Item -Force 'lock.tmp'
} else {
	$LIBS = @('-luser32','-lgdi32','-lXinput','-lWinmm')
	$XLINKER = @('-Wl,/SUBSYSTEM:WINDOWS,/OPT:REF,/OPT:ICF')
	$CC = $COMMON_COMPILER_FLAGS + $WARN + $LIBS + $XLINKER + @($INPUT_PATH,'-o','HandmadeHero.exe')
	& clang++ $CC
}

Pop-Location
