Troubleshooting
===============

Diagnosing Issues
-----------------

The `scalar diagnose` command collects logs and config details for the current
repository. The resulting zip file helps root-cause issues.

When run inside your repository, creates a zip file containing several important
files for that repository. This includes:

* Configuration files from your `.git` folder, such as the `config` file,
  `index`, `hooks`, and `refs`.

* A summary of your Git object database, including the number of loose objects
  and the names and sizes of pack-files.

As the `diagnose` command completes, it provides the path of the resulting
zip file. This zip can be attached to bug reports to make the analysis easier.
