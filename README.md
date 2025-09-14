# now

A minimal **command-line to-do manager** written in C.  
Fast, portable, and designed to stay out of your way.

`now` stores tasks in a single hidden file at `~/.nowfile`, so it works on any POSIX-like system without extra dependencies.

---

## Build & Install

Requires a C compiler (e.g. `gcc`, `clang`).

```bash
make                # build the 'now' binary
sudo make install   # install to /usr/local/bin
