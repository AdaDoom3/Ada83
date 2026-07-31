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
		if fileIsPresent(beside, "ada83.c") then return beside
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
			if (argument as text) is in {"package", "vsix"} then return argument as text
		end repeat
	end try
	return "build"
end requestedTarget

on run argv
	try
		set directory to scriptDirectory()
		set chosenTarget to requestedTarget(argv)
		requireFile(directory, "ada83.c")
		if chosenTarget is not "vsix" then
			requireFile(directory, "ada83-runtime.ada")
			requireCompiler()
			requireLLVM(directory)
		end if
		if chosenTarget is not "build" then
			requireFile(directory, "ada83-extension.js")
			requireFile(directory, "ada83-icon.png")
			if chosenTarget is "package" then requireFile(directory, "ada83-icon.icns")
		end if
		if chosenTarget is "build" then
			runInTerminal(directory, "make && echo && echo 'Built ada83.' && echo 'Compile a program with:  ./ada83 myprogram.ada -o myprogram'")
		else
			runInTerminal(directory, "make " & chosenTarget)
		end if
	on error errorMessage number errorNumber
		if errorNumber is not -128 then error errorMessage number errorNumber
	end try
end run
