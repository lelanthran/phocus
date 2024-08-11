---------------------------------------------------------------
# Empty lines get swallowed in c/line `status`

When running `frame status` empty lines get swallowed. This only happens in
the c/line. The GUI display empty lines just fine, so probably just a but in
the c/line display functions.

---------------------------------------------------------------



---------------------------------------------------------------
# Build/install instructions

Some build and/or install instructions are needed. Even better would be
binaries built for common platforms: Windows-x64, Linux-x64, BSD-x64

---------------------------------------------------------------



---------------------------------------------------------------
# GH Issue-11

While all the other commands I tried work as expected, the "back" command
always goes to the same frame no matter what frame I'm currently on and no
matter what the history looks like.

I can use the "history" command to see a list of frames I've been on recently
but whether I add a number to the command with "frame back 3" or the default
"frame back" to go back one frame it will always end up back at the same
frame. If it helps I only have one root branch with only a few frames in a
linear sequence and the frame the "back" command always goes to is my bottom
leaf frame.

---------------------------------------------------------------
