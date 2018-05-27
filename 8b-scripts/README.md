This directory includes sample EightBall scripts.

I used the suffix `.8b` to designate EightBall source code.

The Commodore machines need source code to be converted from ASCII to
PETSCII, and also to substitute `{` to `[` and `}` to `]`.  The
top-level `Makefile` uses the Linux `tr` command to do this conversion.
The converted files have suffix `.8bp`.

Scripts in this directory:
 - `fact.8b` - Recursive factorial demo
 - `sieve.8b` - Prime number sieve demo / benchmark
 - `str.8b` - Example string handling functions, similar to C
 - `tetris.8b` - Tetris for Apple //e low resolution mode
 - `unittest.8b` - Unit tests for EightBall
