# MakePak

MakePak is a tool that makes a Pak file, which is just a collection of data used by the game. Objects in a Pak file are referred to by index. C header files are generated with constants for the indexes.

Pak files are generated from manifests. A manifest is a list of identifiers and file paths, one per line, separated by whitespace. Additional whitespace, empty lines, and any content after a '#' character is ignored. For example:

    # Manifest file
    player  images/player.png
    script  scenario/script.txt
