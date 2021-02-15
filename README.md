# time2name
Rename file according to its datetime

The ***time2name*** utility renames files according to their creation date/time.

The utility looks for file creation date/time in the following order:
1) Source file name (if it contains this information)
2) EXIF data (in case of JPG files)
3) File last write time (an explicit option should be set)

Command line looks like this:

    time2name DIRECTORY <-i INPUT_FILENAME_PATTERN> <-o OUTPUT_FILENAME_PATTERN> [-r] [-f] [-t]

    -i, --input [INPUT_FILENAME_PATTERN]
input file name pattern specifiying which files will be renamed.

May contain question mark ***?*** representing any single character and the asterisk ___*___ representing any string of characters.

May also contain date and time fields:

***%Y*** - 4-digit year (e.g. 2013)

***%y*** - 2-digit year (e.g. 96)

***%M*** - 2-digit month

***%D*** - 2-digit day of month

***%h*** - 2-digit hour

***%m*** - 2-digit minutes

***%s*** - 2-digit seconds


    -o, --output [OUTPUT_FILENAME_PATTERN]
Pattern describing output file name format.
Should contain date & time fields described above.
Should NOT contain file extension: extension will be copied from the input file name!

Directory, input pattern and output pattern are mandatory fields.

There are also optional fields:

    -r, --recursive
Process directories recursively i.e. including subdirectories.

    -f, --filetime
If creation datetime cannot be detected from input file name or from EXIF data, use file last write datetime instead. 

    -t, --test
Test mode to check input and output filename patterns correctness.
Files won't actually be renamed, but new names will be shown on the screen.

Each option has long and short formats that are interchangeable, so you can use *-i* or *--input*, *-r* or *--recursive* etc.


EXAMPLES
========

    time2name . -r -i *.jpg -o %Y%M%D_%h%m%s

Rename all JPG files in current directory and its subdirectories according to *%Y%M%D_%h%m%s* format.
For example, if EXIF date/time is 17 April 2005, 23:54:32, the new file name will be *20050417_235432.jpg*


    time2name "c:\My Documents\My Podcasts" -i %D%M%y.mp3 -o %Y-%M-%D

Rename all MP3 files in "c:\My Documents\My Podcasts" directory like this: _250397.mp3_ -> _1997-03-25.mp3_

In a situation when there are more than one file with the same creation datetime, time2name never overwrites files.
Instead, it appends an unique code to the filename. So, if we have 3 JPG files created on 17 April 2005 at 23:54:32,
the resulting file names will look like this:

    20050417_235432.jpg
    
    20050417_235432_27851687.jpg
    
    20050417_235432_27851718.jpg
    

