A very powerful method to identify performance bottlenecks is *performance profiling*. Visual Studio (including the free-of-cost Community edition) offers convenient tools for that via the *Debug>Performance Pro<u>f</u>iler...* menu entry.



*This space intentionally left blank for volunteers to fill in walkthroughs with screenshots, and links for further reading*



# Analyze the performance of a Git command specified as a command-line

1. Open the "git.exe" file via *File>Open*
2. Specify the arguments and the working directory via *Debug>git Properties*
3. *Debug>Performance Profiler...*

## Analyze executables compiled by GCC

Visual Studio obtains the debug information used in the output from ".pdb" files, but GCC does not generate debug information in that format.

However, the debug information can be extracted from the ".exe" files and be written out as ".pdb" file via the ["cv2pdb" utility](https://github.com/rainers/cv2pdb).