SuperSimpleCServer
==================

This is a super simple, basic server written in mostly posix-compliant c to help me learn about sockets and networking and the like, and could maybe help you too. It does not fully implement HTTP, but it does kind of fumble through it enough to simulate maybe using HTTP and gets interpreted validly for the most part by the browsers I've used.

What it does is take in a directory and port, and serve the files within that directory on that port from your computer.

The directory defaults to '/' and the port defaults to 80 (which you probably/maybe need permissions to use as a port? In order to host on port 80, which is like the default http port I think, on my computer at least, I need to run it with sudo)

For example, if you compiled it with `gcc -o simpleserver supersimpleserver.c` [the letter s has a nice sound], and ran it with something like `sudo simpleserver /home/user/documents/internets`, and your internets folder had an index.html, you could go to a browser, open up "localhost/index.html" and you would/should get that file along with any associated files, like css or js files or images or the like.

It was written to minimize everything that I could? No library for dealing with strings, because who needs that, I just rolled my own super basic string struct that is appendable. I wanted to see the barest web server I could write. I mean, I could probably skip the whole FILE* thing and my freads and printfs, just go that last step of reads, writes, and opens, but...I didn't? I work in mysterious ways.
