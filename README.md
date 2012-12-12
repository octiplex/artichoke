# Artichoke

## Presentation
When building a static library using `libtool`, even if the input object files did not change, the output archive is never the same because the current date is stored into its header.

Though some developers may consider it useful to have a timestamped library, some others (e.g. [me](mailto:nicolas@octiplex.com)) resent it as a pain in their buttocks, which causes their source code management system (a.k.a git) to generate unnecessary long diffs.

Artichoke solves that problem by replacing that faulty timestamp with the modification date of the archive's most recent object file.

It also works with Mach-O Fat libraries.

## Usage

```
$ artichoke libBadMotherFuckerFeatures.a
```
 
There's no step 3!

## License
This project is provided under the Modified BSD License. You can read more about it in the `LICENSE.md` file.