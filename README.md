# tinyfont
A file format for vector fonts.

Inspired by the image format 'farbfeld', this was an attempt to create
a publishing format (not editing) for fonts.

It was way beyond my capabilities, but i got some interesting results.

# Dependencies
http://git.suckless.org/libutf/

# Try it
Build the programs with `$ make` and run it as follows:

`$ ./sfd2tf < ~/downloads/Inconsolata.sfd > Inconsolata.tf`

`$ ./txt2ff Inconsolata.tf 48 tinyfont`

`$ feh out.ff`
