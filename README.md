# smallsh
Basic shell program supporting input/output redirection and background execution. Written in C.

## Usage
Visit https://replit.com/@maganinirj/smallsh and click 'Run',
or compile and run on Linux systems using:
```
$ gcc -o smallsh main.c
$ ./smallsh
```
Execute commands of the form `command [arg1 arg2 ...] [< inputFile] [> outputFile] [&]`.
Arguments should be no more than 64 characters in length, with a maximum of 512 arguments.

## Motivation
I first completed this project while taking an OS course at Oregon State, and
it taught me a lot about the core functionality of operating systems. You can take a look at
the full project requirements set by the professor in requirements.md.

Though my first attempt at this project received a perfect score from the grading script, it was nearly 600 lines
of overcomplicated and unclear code. So, to brush up on OS concepts and avoid completely losing my
hard-earned C skills, I decided to reimplement.
