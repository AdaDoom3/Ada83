-- Builds the Ada 83 compiler on macOS.
-- Run with: osascript build.applescript
-- or open it in Script Editor and press Run.

on shell(command)
	return do shell script command
end shell

on succeeds(command)
	try
		shell(command)
		return true
	on error
		return false
	end try
end succeeds

on present(program)
	return succeeds("command -v " & quoted form of program)
end present

on existsAt(directory, name)
	return succeeds("test -e " & quoted form of (directory & name))
end existsAt

-- Under osascript "path to me" is the interpreter, not this file, so the
-- sources are looked for beside the script and then in the working directory.
on scriptDirectory()
	set beside to ""
	try
		set self to path to me
		tell application "System Events" to set beside to POSIX path of (container of self)
	end try
	if beside is not "" and existsAt(beside, "ada83.c") then return beside
	return shell("pwd") & "/"
end scriptDirectory

on fail(headline, detail)
	display dialog headline & return & return & detail buttons {"OK"} default button "OK" with icon stop
	error number -128
end fail

on requireFile(directory, name)
	if existsAt(directory, name) then return
	fail("Cannot find " & name & ".", "Run this script from the folder it came in, beside the rest of the sources.")
end requireFile

on requireCompiler()
	if present("cc") or present("clang") or present("gcc") then return
	display dialog "The command line tools are needed to build the compiler." & return & return & ¬
		"Install them now?" buttons {"Cancel", "Install"} default button "Install"
	shell("xcode-select --install")
	fail("Installation started.", "Run this script again once the command line tools have finished installing.")
end requireCompiler

on llvmPresent()
	repeat with candidate in {"/opt/homebrew/opt/llvm/lib/libLLVM.dylib", ¬
		"/usr/local/opt/llvm/lib/libLLVM.dylib", ¬
		"/Library/Developer/CommandLineTools/usr/lib/libLLVM.dylib"}
		if succeeds("test -e " & quoted form of (candidate as text)) then return true
	end repeat
	return false
end llvmPresent

on inTerminal(directory, command)
	tell application "Terminal"
		activate
		do script "cd " & quoted form of directory & " && clear && " & command
	end tell
end inTerminal

on requireLLVM(directory)
	if llvmPresent() then return
	if not present("brew") then
		fail("libLLVM is missing and Homebrew is not installed.", ¬
			"Install Homebrew from https://brew.sh, then run this script again.")
	end if
	display dialog "libLLVM is needed to produce native executables." & return & return & ¬
		"Install it with Homebrew now?" buttons {"Cancel", "Install"} default button "Install"
	inTerminal(directory, "brew install llvm")
	fail("Installing libLLVM.", "Run this script again once Homebrew has finished.")
end requireLLVM

on run
	set directory to scriptDirectory()
	requireFile(directory, "ada83.c")
	requireFile(directory, "runtime.ada")
	requireCompiler()
	requireLLVM(directory)
	inTerminal(directory, "make && echo && echo 'Built ada83.' && echo 'Compile a program with:  ./ada83 myprogram.ada -o myprogram'")
end run
