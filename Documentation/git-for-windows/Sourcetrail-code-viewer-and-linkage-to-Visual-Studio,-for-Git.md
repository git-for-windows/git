[Sourcetrail](https://www.sourcetrail.com) is an interactive source explorer that simplifies navigation in existing source code. It is [now open source](https://www.sourcetrail.com/blog/open_source/). You can support them on Patreon.

This note is a quick start to setting up a Sourcetrail project for exploring the Git source code in conjunction with Visual Studio. Source trail is configurable and some parameter need to be set on initial project setup.

1. Git-for-Windows is available as a Visual Studio build in the `main` [branch](https://github.com/git-for-windows/git/tree/main). Visual Studio will detect the `CMakeLists.txt` file in `\contrib\buildsystems` to build the project. See that file for instructions on generating the `.sln` and `vcproj` files needed for Sourcetrail and other VS Extensions. The VS project comprises ~19 sub-projects including `ALL BUILD` and `libgit`.
2. This was tested with the free VS2019 community edition.
3. Build the project in Visual Studio so that it's `vcpkg` dependencies are downloaded and compiled (may take some time).
4. Download and install the Sourcetrail software. Check the [documentation](https://www.sourcetrail.com/documentation/), especially the [intro video](https://youtu.be/7fguPwKR_7Y).
5. Download and install the [VS Sourcetrail Extension](https://marketplace.visualstudio.com/items?itemName=vs-publisher-1208751.SourcetrailExtension).

For Git, Sourcetrail will need it's pre-processor dependencies which are numerous, so let's extract them from Visual Studio. We need to collect the main preprocessor directives from `libgit` (which everything depends on) and `git`.

1. Browse the VS project properties. Find the `C/C++` tab list, open, find 'Preprocessor', then (see top of list) "Preprocessor Definitions" with a long list of ";" separated values. Copy them for both `git` and `libgit` into your favourite editor.
2. Change ";" to "\n-D " (i.e. newline then -D ), with a `-D ` for the first value from each original line as well, etc.
3. Remove the "-D %(additional..)" lines.
4. Add `-std=c99` and `-nologo` options to the end of the option list, which now should be one per line.
5. Don't worry about duplicates. Internally Sourcetrail uses a Clang compiler with a 'last one wins' where required.

Now let's create the Sourcetrail project
1. Open Sourcetrail. Leave at the initial prompt.
2. In VS, from the Sourcetrail menu, export the compile database, selecting C99 standard.
3. Switch back to Sourcetrail "New Source Group" window.

4. "Headers & directories to index": click 'select from Compilation database', then tick './' only (leave all the 'Windows Kits' unchecked). scroll down, "exluded's" left blank; Next.

(flicks back to the New Project dialog, but as it is still selecting the source groups (C/C++ from Compilation Database) its looks unchanged),

5. scroll down, "Additional Compile Flags" selecting the scribble pen icon, paste in the "Preprocessor Definitions" (one per line) prepared above.
6. select "General" in top left side, and update the project name to (e.g.) "Git_sourcetrail". Location is usually ok e.g."C:/git-sdk-64/usr/src/git".
"Create" ! (or maybe Next)

* switch back to the other Sourcetrail window (now active, with a fresh blue 'start Indexing' dialog). "All files" (dot in circle) should already be selected.
* Click 'Start', wait ~one minute (00:01:38).

Should work.