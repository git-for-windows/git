The self-extracting 7z archives that are currently released (e.g. for the portable version) can easily be installed in a headless manner using the extraction options.

The self-extracting 7z archives are far more compact than regular .zip archives as the compaction is done across the whole content, rather than file by file, which would make .zip files inconveniently large.

For a quiet extraction of the archive from the command line:

```
> .\PortableGit-2.11.0-64-bit.7z.exe -y -gm2 -InstallPath="C:\\use\\double\\backslashes"
```

We use the old .sfx component that used to be hosted on http://7zsfx.info. This provides more options for a fully quiet extraction - as shown, in this case:

*     `-y` hide some prompts
*     `-gm2` hides the extraction dialog completely (silent mode)
*     `-InstallPath` sets the target path (you need double backslashes)

You can still see the switches in the Internet Archive: https://web.archive.org/web/20160402074100/http://www.7zsfx.info/en/switches.html. (see also this helpful StackOverflow answer: http://stackoverflow.com/a/32105548).



This page responds to the question in https://github.com/git-for-windows/git/issues/986