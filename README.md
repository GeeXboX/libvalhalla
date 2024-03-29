
# ![libvalhalla](Documentations/valhalla.png)

GeeXboX Valhalla (or libvalhalla) is a scanner and parser for audio/video
files based on SQLite and FFmpeg/libavformat. Media files are retrieved in
paths defined by the user and metadata are saved in a database.
It provides a very simple API and it can use several parsers concurrently
to speed up on a multi-core/cpu system.
Its goal is to interact with GeeXboX / Enna Media Center.

## Why Valhalla?

> In Norse mythology, Valhalla (from Old Norse Valhöll "hall of the slain")
> is a majestic, enormous hall located in Asgard, ruled over by the god Odin.
> Chosen by Odin, half of those that die in combat travel to Valhalla upon
> death, led by valkyries, while the other half go to the goddess Freyja's
> field Fólkvangr. In Valhalla, the dead join the masses of those who have
> died in combat known as Einherjar, as well as various legendary Germanic
> heroes and kings, as they prepare to aid Odin during the events of Ragnarök.
>
>>  Valhalla. (2009, February 11). In Wikipedia, The Free Encyclopedia.  
>>    Retrieved 14:49, February 14, 2009,  
>>    from http://en.wikipedia.org/w/index.php?title=Valhalla&oldid=270025459
>
> In Valhalla, the heroes are happy: they fight, kill, are reborn to assail
> again in a closed field.


You can imagine by analogy, that the scanner (the valkyries) finds the
media files (the legendary heroes) in order to insert them into the database
(to Valhalla). And each file (hero) is checked in loop (is reborn to assail
again).

## DEVELOPERS

Build using traditional `./configure && make` commands.
Try `./configure --help` for list of available build options.

ALWAYS consider `valhalla.h` as the ONLY public header.
Private definitions have to go to `valhalla_internals.h`
NEVER use `printf()`, always use `valhalla_log()`.
Check your code for warnings before commits.

### Versioning policy

The major version means no backward compatibility to previous versions (e.g.
removal of a function from the public API). The minor version means backward
compatible change (e.g. addition of a function to the public API or extension
of an existing data structure). The micro version means a noteworthy binary
compatible change (e.g. new grabber).

## AUTHORS

Mathieu Schroeter <mathieu@schroetersa.ch>  
Benjamin Zores <ben@geexbox.org>  
Fabien Brisset <fbrisset@gmail.com>  
Davide Cavalca <davide@geexbox.org>  
