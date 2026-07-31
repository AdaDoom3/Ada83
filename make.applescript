on shellSucceeds(command)
	try
		do shell script command
		return true
	on error
		return false
	end try
end shellSucceeds

on fileIsPresent(directory, fileName)
	return shellSucceeds("test -e " & quoted form of (directory & "/" & fileName))
end fileIsPresent

on scriptDirectory()
	try
		set scriptFile to path to me
		tell application "System Events" to set beside to POSIX path of (container of scriptFile)
		if fileIsPresent(beside, "make.applescript") then return beside
	end try
	return do shell script "pwd"
end scriptDirectory

on runInTerminal(directory, command)
	tell application "Terminal"
		activate
		do script "cd " & quoted form of directory & " && clear && " & command
	end tell
end runInTerminal

on stopWithMessage(headline, detail)
	display dialog headline & return & return & detail buttons {"OK"} default button "OK" with icon stop
	error number -128
end stopWithMessage

on requireFile(directory, fileName)
	if fileIsPresent(directory, fileName) then return
	stopWithMessage("Cannot find " & fileName & ".", "Run this script from the folder it came in, beside the rest of the sources.")
end requireFile

on requireCompiler()
	if shellSucceeds("gcc -E -x c /dev/null >/dev/null 2>&1") then return
	display dialog "The command line tools are needed to build the compiler." & return & return & ¬
		"Install them now?" buttons {"Cancel", "Install"} default button "Install" cancel button "Cancel"
	shellSucceeds("xcode-select --install")
	stopWithMessage("Installation requested.", "Accept Apple's installer, then run this script again once the command line tools are in place.")
end requireCompiler

on llvmIsPresent()
	return shellSucceeds("ls /opt/homebrew/opt/llvm/lib/libLLVM.dylib " & ¬
		"/usr/local/opt/llvm/lib/libLLVM.dylib " & ¬
		"/Library/Developer/CommandLineTools/usr/lib/libLLVM.dylib " & ¬
		"2>/dev/null | grep -q .")
end llvmIsPresent

on requireLLVM(directory)
	if llvmIsPresent() then return
	if not shellSucceeds("command -v brew") then
		stopWithMessage("libLLVM is missing and Homebrew is not installed.", ¬
			"Install Homebrew from https://brew.sh, then run this script again.")
	end if
	display dialog "libLLVM is needed to produce native executables." & return & return & ¬
		"Install it with Homebrew now?" buttons {"Cancel", "Install"} default button "Install" cancel button "Cancel"
	runInTerminal(directory, "brew install llvm")
	stopWithMessage("Installing libLLVM.", "Run this script again once Homebrew has finished.")
end requireLLVM

on requestedTarget(argv)
	try
		repeat with argument in argv
			if (argument as text) is "package" then return "package"
			if (argument as text) is "vsix" then return "vsix"
		end repeat
	end try
	return "build"
end requestedTarget

on joinedLines(lines)
	set joined to ""
	repeat with oneLine in lines
		if joined is "" then
			set joined to oneLine as text
		else
			set joined to joined & linefeed & (oneLine as text)
		end if
	end repeat
	return joined
end joinedLines

on guardedProgram(steps, failureNote)
	return joinedLines({"(", "set -e"} & steps & {") || echo '" & failureNote & "'"})
end guardedProgram

on extensionSplitLines()
	return {"awk '/^\\/\\/== end$/       { out = \"\"; next }", ¬
		"     /^\\/\\/== /           { name = substr ($0, 6)", ¬
		"                            if (name ~ /^(extension.vsixmanifest|\\[)/) out = \"staging/vsix/\" name", ¬
		"                            else out = \"staging/vsix/extension/\" name", ¬
		"                            next }", ¬
		"     /^\\/\\/= /            { next }", ¬
		"     out != \"\" && /^\\/\\// { print substr ($0, 3) > out }' ada83-extension.js"}
end extensionSplitLines

on extensionSteps()
	return {"command -v zip >/dev/null || { echo 'zip is needed to package'; exit 1; }", ¬
		"rm -rf staging/vsix", ¬
		"mkdir -p staging/vsix/extension/syntaxes", ¬
		"cp ada83-extension.js ada83-icon.png staging/vsix/extension/", ¬
		"if [ -f manual.md ]; then", ¬
		"  cp manual.md staging/vsix/extension/", ¬
		"else", ¬
		"  echo 'manual.md is missing; packaging without the manual search tool'", ¬
		"fi"} & extensionSplitLines() & ¬
		{"rm -f staging/ada83.vsix", ¬
		"( cd staging/vsix && zip -qr ../ada83.vsix . )", ¬
		"rm -rf staging/vsix"}
end extensionSteps

on buildProgram()
	return guardedProgram({"gcc -O3 -Wall -std=gnu2x -o ada83 ada83.c -lm -lpthread", ¬
		"test -f ada83-runtime.ada || echo 'ada83-runtime.ada is not here; ada83 needs it beside the executable.'", ¬
		"echo", ¬
		"echo 'Built ada83.'", ¬
		"echo 'Compile a program with:  ./ada83 myprogram.ada -o myprogram'"}, ¬
		"The build failed; the message above says why.")
end buildProgram

on vsixProgram()
	return guardedProgram(extensionSteps() & ¬
		{"echo 'Built staging/ada83.vsix:'", ¬
		"unzip -l staging/ada83.vsix | tail -n +4"}, ¬
		"Building the extension failed; the message above says why.")
end vsixProgram

on packageProgram()
	return guardedProgram(extensionSteps() & ¬
		{"command -v lipo >/dev/null || { echo 'joining the macOS slices needs lipo'; exit 1; }", ¬
		"gcc -O3 -Wall -g0 -std=gnu2x -arch arm64 -o staging/ada83-arm64 ada83.c -lm -lpthread", ¬
		"gcc -O3 -Wall -g0 -std=gnu2x -arch x86_64 -o staging/ada83-x86_64 ada83.c -lm -lpthread", ¬
		"lipo -create -output staging/ada83 staging/ada83-arm64 staging/ada83-x86_64", ¬
		"rm -f staging/ada83-arm64 staging/ada83-x86_64", ¬
		"cp ada83-runtime.ada ada83-icon.icns staging/", ¬
		"rm -f bin-macos.zip.new", ¬
		"( cd staging && zip -q ../bin-macos.zip.new ada83 ada83-runtime.ada ada83.vsix ada83-icon.icns )", ¬
		"mv bin-macos.zip.new bin-macos.zip", ¬
		"echo 'Packaged bin-macos.zip:'", ¬
		"unzip -l bin-macos.zip | tail -n +4"}, ¬
		"Packaging failed; the message above says why.")
end packageProgram

on programFor(chosenTarget)
	if chosenTarget is "package" then return packageProgram()
	if chosenTarget is "vsix" then return vsixProgram()
	return buildProgram()
end programFor

on run argv
	try
		set directory to scriptDirectory()
		set chosenTarget to requestedTarget(argv)
		if chosenTarget is not "vsix" then
			requireFile(directory, "ada83.c")
		end if
		if chosenTarget is "package" then
			requireFile(directory, "ada83-runtime.ada")
			requireFile(directory, "ada83-icon.icns")
		end if
		if chosenTarget is not "build" then
			requireFile(directory, "ada83-extension.js")
			requireFile(directory, "ada83-icon.png")
		end if
		if chosenTarget is not "vsix" then
			requireCompiler()
		end if
		if chosenTarget is "build" then
			requireLLVM(directory)
		end if
		runInTerminal(directory, programFor(chosenTarget))
	on error errorMessage number errorNumber
		if errorNumber is not -128 then error errorMessage number errorNumber
	end try
end run
