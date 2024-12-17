One of the many components included in Git for Windows' SDK is the `perl` package. Part of its components are included in Git for Windows' installer and Portable Git versions, to allow for running Perl scripts. The two most prominent Perl scripts in Git for Windows are, of course, `git svn` and `git send-email`.

Upgrading to a new Perl version is unfortunately a bit more involved than only following the advice in [Building new package versions](Building-new-package-versions.md). In addition to adjusting the patches applied via the `PKGBUILD` script, the major problem with Perl is that its DLL embeds the Perl version in its file name.

# Making sure that the DLL has the correct version in its file name

This is important, and it is a manual step required on top of [the usual adjustments to the patches](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Building-new-package-versions.md#adjusting-patches-when-they-no-longer-apply-to-new-versions): In the [`PKGBUILD`](https://github.com/msys2/MSYS2-packages/blob/HEAD/perl/PKGBUILD) file, the line `-Dlibperl=msys-perl5_<N>.dll` needs to be adjusted.

# Rebuilding dependencees

As a consequence of the version name being embedded in the DLL's file name, _all_ native Perl modules are broken immediately after upgrading to a new major Perl version. As far as Git for Windows is concerned, the affected components are:

- subversion
- perl-Net-SSLeay
- perl-HTML-Parser
- perl-TermReadKey
- perl-Locale-Gettext
- perl-XML-Parser
- perl-YAML-Syck

Therefore, immediately after a successful upgrade of the `perl` component to a new major Perl version (as of time of writing, the most recent such upgrade was to v5.32.0), these components have to be rebuilt, typically by forcing an incremented `pkgrel`.