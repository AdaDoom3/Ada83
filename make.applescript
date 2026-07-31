on linuxPlatform()
	return "linux"
end linuxPlatform

on macosPlatform()
	return "macos"
end macosPlatform

on windowsPlatform()
	return "windows"
end windowsPlatform

on knownPlatforms()
	return {linuxPlatform(), macosPlatform(), windowsPlatform()}
end knownPlatforms

on containerImage()
	return "gcc:13"
end containerImage

on containerEngines()
	return {"docker", "podman"}
end containerEngines

on isContainerEngine(toolName)
	repeat with engine in containerEngines()
		if toolName is (engine as text) then return true
	end repeat
	return false
end isContainerEngine

on toolIsPresent(toolName)
	return shellSucceeds("command -v " & quoted form of toolName)
end toolIsPresent

on linuxCompilerTool()
	if toolIsPresent("x86_64-linux-gnu-gcc") then return "x86_64-linux-gnu-gcc"
	if toolIsPresent("x86_64-unknown-linux-gnu-gcc") then return "x86_64-unknown-linux-gnu-gcc"
	repeat with engine in containerEngines()
		if toolIsPresent(engine as text) then return engine as text
	end repeat
	return "x86_64-linux-gnu-gcc"
end linuxCompilerTool

on compilerToolFor(chosenPlatform)
	if chosenPlatform is linuxPlatform() then return linuxCompilerTool()
	if chosenPlatform is macosPlatform() then return "gcc"
	return "x86_64-w64-mingw32-gcc"
end compilerToolFor

on compilerFor(chosenPlatform)
	set toolName to compilerToolFor(chosenPlatform)
	if isContainerEngine(toolName) then return toolName & ¬
		" run --rm --volume \"$PWD\":/work --workdir /work " & containerImage() & " gcc"
	return toolName
end compilerFor

on compilerFlagsFor(chosenPlatform)
	if chosenPlatform is linuxPlatform() then return "-O3 -Wall -g0 -std=gnu17 -march=x86-64 -mtune=generic"
	if chosenPlatform is macosPlatform() then return "-O3 -Wall -g0 -std=gnu2x"
	return "-O2 -Wall -g0 -std=gnu2x"
end compilerFlagsFor

on architecturesFor(chosenPlatform)
	if chosenPlatform is macosPlatform() then return {"arm64", "x86_64"}
	return {}
end architecturesFor

on linkLibrariesFor(chosenPlatform)
	if chosenPlatform is windowsPlatform() then return "-lm"
	return "-lm -lpthread"
end linkLibrariesFor

on resourceCompilerFor(chosenPlatform)
	if chosenPlatform is windowsPlatform() then return "x86_64-w64-mingw32-windres"
	return ""
end resourceCompilerFor

on resourceObjectFor(chosenPlatform)
	if resourceCompilerFor(chosenPlatform) is "" then return ""
	return "staging/ada83-icon.o"
end resourceObjectFor

on executableFor(chosenPlatform)
	if chosenPlatform is windowsPlatform() then return "ada83.exe"
	return "ada83"
end executableFor

on artworkFor(chosenPlatform)
	if chosenPlatform is linuxPlatform() then return "ada83-icon.png"
	if chosenPlatform is macosPlatform() then return "ada83-icon.icns"
	return "ada83-icon.ico"
end artworkFor

on resourceForkFor(chosenPlatform)
	if chosenPlatform is macosPlatform() then return "ada83-icon.rsrc"
	return ""
end resourceForkFor

on launcherFor(chosenPlatform)
	if chosenPlatform is linuxPlatform() then return "ada83.desktop"
	return ""
end launcherFor

on sharedLibrariesFor(chosenPlatform)
	if chosenPlatform is windowsPlatform() then return "*.dll"
	return ""
end sharedLibrariesFor

on archiveFor(chosenPlatform)
	return "bin-" & chosenPlatform & ".zip"
end archiveFor

on archiveContentsFor(chosenPlatform)
	set members to {executableFor(chosenPlatform), "ada83-runtime.ada", "ada83.vsix", artworkFor(chosenPlatform)}
	if launcherFor(chosenPlatform) is not "" then set members to members & {launcherFor(chosenPlatform)}
	if sharedLibrariesFor(chosenPlatform) is not "" then set members to members & {sharedLibrariesFor(chosenPlatform)}
	if resourceForkFor(chosenPlatform) is not "" then set members to members & {"__MACOSX/._" & executableFor(chosenPlatform)}
	return joinedWith(members, space)
end archiveContentsFor

on toolchainHintFor(chosenPlatform)
	if chosenPlatform is linuxPlatform() then return "Nothing here can build for Linux: no x86_64-linux-gnu-gcc, no x86_64-unknown-linux-gnu-gcc (the macos-cross-toolchains tap installs that one), and neither Docker nor Podman, which would compile in a " & containerImage() & " container instead. Install any one of those, then run this script again."
	return "Homebrew has it: brew install mingw-w64. Install it, then run this script again."
end toolchainHintFor

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

on requireTool(chosenPlatform, toolName)
	if shellSucceeds("command -v " & quoted form of toolName) then return
	stopWithMessage("packaging for " & chosenPlatform & " needs " & toolName, toolchainHintFor(chosenPlatform))
end requireTool

on requireToolchain(chosenPlatform)
	if chosenPlatform is macosPlatform() then
		requireCompiler()
		return
	end if
	requireTool(chosenPlatform, compilerToolFor(chosenPlatform))
	if resourceCompilerFor(chosenPlatform) is not "" then requireTool(chosenPlatform, resourceCompilerFor(chosenPlatform))
end requireToolchain

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

on argumentList(argv)
	try
		set given to {}
		repeat with argument in argv
			set given to given & {argument as text}
		end repeat
		return given
	end try
	return {}
end argumentList

on requestedAction(argv)
	repeat with argument in argumentList(argv)
		set requested to argument as text
		if requested is "package" then return "package"
		if requested is "vsix" then return "vsix"
	end repeat
	return "build"
end requestedAction

on canonicalPlatform(requested)
	repeat with candidate in knownPlatforms()
		if requested is (candidate as text) then return candidate as text
	end repeat
	stopWithMessage("The platform is '" & requested & "'; it must be linux, macos or windows.", ¬
		"Ask for one of: package, package linux, package macos, package windows.")
end canonicalPlatform

on requestedPlatform(argv)
	repeat with argument in argumentList(argv)
		set requested to argument as text
		if requested is not "package" and requested is not "vsix" then return canonicalPlatform(requested)
	end repeat
	return macosPlatform()
end requestedPlatform

on joinedWith(pieces, separator)
	set joined to ""
	set isFirst to true
	repeat with piece in pieces
		if isFirst then
			set joined to piece as text
			set isFirst to false
		else
			set joined to joined & separator & (piece as text)
		end if
	end repeat
	return joined
end joinedWith

on joinedLines(theLines)
	return joinedWith(theLines, linefeed)
end joinedLines

on guardedProgram(steps, failureNote)
	return joinedLines({"(", "set -e"} & steps & ¬
		{")", "test $? -eq 0 || echo '" & failureNote & "'"})
end guardedProgram

on extensionSplitLines()
	return {"awk '/^<\\/script>$/ { out = \"\"; next } \\", ¬
		"     /^<script/ { match ($0, /id=\"[^\"]*\"/); \\", ¬
		"                  name = substr ($0, RSTART + 4, RLENGTH - 5); \\", ¬
		"                  out = (name ~ /^(extension.vsixmanifest|\\[)/) \\", ¬
		"                        ? \"staging/vsix/\" name \\", ¬
		"                        : \"staging/vsix/extension/\" name; next } \\", ¬
		"     out != \"\" { print > out }' ada83-extension.html"}
end extensionSplitLines

on extensionSteps()
	return {"command -v zip >/dev/null || { echo 'zip is needed to package'; exit 1; }", ¬
		"rm -rf staging/vsix", ¬
		"mkdir -p staging/vsix/extension/syntaxes", ¬
		"cp ada83-icon.png staging/vsix/extension/", ¬
		"if [ -f manual.md ]; then", ¬
		"  cp manual.md staging/vsix/extension/", ¬
		"else", ¬
		"  echo 'manual.md is missing; packaging without the manual search tool'", ¬
		"fi"} & extensionSplitLines() & ¬
		{"rm -f staging/ada83.vsix", ¬
		"( cd staging/vsix && zip -qr ../ada83.vsix . )", ¬
		"rm -rf staging/vsix"}
end extensionSteps

on compileCommand(chosenPlatform, architectureFlag, outputPath)
	set resourceObject to resourceObjectFor(chosenPlatform)
	if resourceObject is not "" then set resourceObject to " " & resourceObject
	return compilerFor(chosenPlatform) & " " & compilerFlagsFor(chosenPlatform) & architectureFlag & ¬
		" -o " & outputPath & " ada83.c" & resourceObject & " " & linkLibrariesFor(chosenPlatform)
end compileCommand

on compileSteps(chosenPlatform)
	set slicePaths to {}
	set steps to {}
	repeat with architecture in architecturesFor(chosenPlatform)
		set slicePath to "staging/" & executableFor(chosenPlatform) & "-" & (architecture as text)
		set slicePaths to slicePaths & {slicePath}
		set steps to steps & {compileCommand(chosenPlatform, " -arch " & (architecture as text), slicePath)}
	end repeat
	if slicePaths is {} then return {compileCommand(chosenPlatform, "", "staging/" & executableFor(chosenPlatform))}
	return steps & {"lipo -create -output staging/" & executableFor(chosenPlatform) & " " & joinedWith(slicePaths, space), ¬
		"rm -f " & joinedWith(slicePaths, space)}
end compileSteps

on chosenRouteSteps(chosenPlatform)
	if chosenPlatform is not linuxPlatform() then return {}
	set toolName to compilerToolFor(chosenPlatform)
	if isContainerEngine(toolName) then return {"echo 'Building the Linux executable in a " & containerImage() & ¬
		" container under " & toolName & ".'", ¬
		"echo 'The first run downloads that image; nothing else here is fetched.'"}
	return {"echo 'Building the Linux executable with " & toolName & ".'"}
end chosenRouteSteps

on sharedLibraryGuardSteps(chosenPlatform)
	if sharedLibrariesFor(chosenPlatform) is "" then return {}
	return {"test -f " & archiveFor(chosenPlatform) & " || { echo '" & archiveFor(chosenPlatform) & ¬
		" holds the only copy of the libraries " & chosenPlatform & "'; echo 'loads at run time, and is missing'; exit 1; }"}
end sharedLibraryGuardSteps

on sliceGuardSteps(chosenPlatform)
	if architecturesFor(chosenPlatform) is {} then return {}
	return {"command -v lipo >/dev/null || { echo 'joining the " & chosenPlatform & " slices needs lipo'; exit 1; }"}
end sliceGuardSteps

on resourceObjectSteps(chosenPlatform)
	if resourceObjectFor(chosenPlatform) is "" then return {}
	return {"printf '1 ICON \"%s\"\\n' \"$PWD/" & artworkFor(chosenPlatform) & "\" | " & ¬
		resourceCompilerFor(chosenPlatform) & " -O coff -o " & resourceObjectFor(chosenPlatform)}
end resourceObjectSteps

on resourceForkSteps(chosenPlatform)
	if resourceForkFor(chosenPlatform) is "" then return {}
	return {"mkdir -p staging/__MACOSX", ¬
		"cp " & resourceForkFor(chosenPlatform) & " staging/__MACOSX/._" & executableFor(chosenPlatform)}
end resourceForkSteps

on launcherSteps(chosenPlatform)
	if launcherFor(chosenPlatform) is "" then return {}
	return {"printf '%s\\n' '[Desktop Entry]' 'Type=Application' 'Name=Ada 83' 'Comment=Ada 83 compiler' " & ¬
		"'Exec=ada83 %F' 'Icon=ada83-icon' 'Terminal=true' 'Categories=Development;Building;' > staging/" & ¬
		launcherFor(chosenPlatform)}
end launcherSteps

on sharedLibrarySteps(chosenPlatform)
	if sharedLibrariesFor(chosenPlatform) is "" then return {}
	return {"unzip -qoj " & archiveFor(chosenPlatform) & " '" & sharedLibrariesFor(chosenPlatform) & "' -d staging"}
end sharedLibrarySteps

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

on packageProgram(chosenPlatform)
	set archiveName to archiveFor(chosenPlatform)
	return guardedProgram(extensionSteps() & sharedLibraryGuardSteps(chosenPlatform) & ¬
		sliceGuardSteps(chosenPlatform) & resourceObjectSteps(chosenPlatform) & ¬
		chosenRouteSteps(chosenPlatform) & compileSteps(chosenPlatform) & ¬
		{"cp ada83-runtime.ada " & artworkFor(chosenPlatform) & " staging/"} & ¬
		resourceForkSteps(chosenPlatform) & launcherSteps(chosenPlatform) & ¬
		sharedLibrarySteps(chosenPlatform) & ¬
		{"rm -f " & archiveName & ".new", ¬
		"( cd staging && zip -q ../" & archiveName & ".new " & archiveContentsFor(chosenPlatform) & " )", ¬
		"mv " & archiveName & ".new " & archiveName, ¬
		"echo 'Packaged " & archiveName & ":'", ¬
		"unzip -l " & archiveName & " | tail -n +4"}, ¬
		"Packaging failed; the message above says why.")
end packageProgram

on programFor(chosenAction, chosenPlatform)
	if chosenAction is "package" then return packageProgram(chosenPlatform)
	if chosenAction is "vsix" then return vsixProgram()
	return buildProgram()
end programFor

on run argv
	try
		set directory to scriptDirectory()
		set chosenAction to requestedAction(argv)
		set chosenPlatform to macosPlatform()
		if chosenAction is "package" then set chosenPlatform to requestedPlatform(argv)
		if chosenAction is not "vsix" then
			requireFile(directory, "ada83.c")
		end if
		if chosenAction is "package" then
			requireFile(directory, "ada83-runtime.ada")
			requireFile(directory, artworkFor(chosenPlatform))
			if resourceForkFor(chosenPlatform) is not "" then
				requireFile(directory, resourceForkFor(chosenPlatform))
			end if
		end if
		if chosenAction is not "build" then
			requireFile(directory, "ada83-extension.html")
			requireFile(directory, "ada83-icon.png")
		end if
		if chosenAction is "package" then
			requireToolchain(chosenPlatform)
		end if
		if chosenAction is "build" then
			requireCompiler()
			requireLLVM(directory)
		end if
		runInTerminal(directory, programFor(chosenAction, chosenPlatform))
	on error errorMessage number errorNumber
		if errorNumber is not -128 then error errorMessage number errorNumber
	end try
end run
