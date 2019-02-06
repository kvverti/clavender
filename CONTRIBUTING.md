# It looks like you want to contribute...
That's great! CLavender is always open to contributions and improvements. Here's how.

### Reporting Issues
You can and should report any issues you find with the interpreter or the language on the issue tracker.
* Before creating a new issue, check to see if the issue is already known.
* Flag your issue with the correct tags (`bug` or `feature request` with the appropriate category).
* Include in the report a short description of the issue, and, in the case of a bug, a (preferably minimal) test case, or, in the case of a feature request, an example of the desired feature.

If you see an issue and would like to fix it, feel free to work on it and create a pull request.

### Pull Requests
Pull requests should contain a short summary (2-3 sentences) of the changes and of any issues resolved. Changes should adhere to the style conventions.

### Style Guide
CLavender is written in mostly standard C11 with minimal use of GNU C extensions and assumes a POSIX-compatible platform.
* Other C language extensions (for example Microsoft extensions) should not be used.
* Code should not depend on undefined behavior.
* The tab stop is set to four spaces.
* All named struct and union definitions should have a `typedef` of the same name.
* Type names are in UpperCamelCase, variable names are in lowerCamelCase.
* Functions with external linkage have names of the form `lv_<ns>_<name>`, where `<ns>` is a short form (2-4 characters) of the header name.
* Functions with internal linkage are named like variables.
* Dynamic memory management should be done with the `lv_alloc` family, which are guarenteed to not return `NULL`.

### Becoming a Contributor
If you would like to have push access to the CLavender repository, please contact the author directly. Otherwise, anyone is welcome to fork and request pulls.
