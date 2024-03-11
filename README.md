# smallsh
Small Shell Project

This program is a fully functional terminal shell that can be run at the command line.
It includes a couple built-in commands, file redirection, as well as managing 
background and foreground parent/child processes. It also incorporates a custom
signal handler.

To get started, download or clone the files onto your computer. Next, open a bash
terminal and navigate to the directory where the files are saved. 

In the directory, type the following command and hit enter:

```
make smallsh
```

This will compile smallsh.c into an executable file called smallsh.exe.

Now you are ready to run the program by typing the following command and
hitting enter:

```
./ smallsh
```

While smallsh is running, feel free to experiment with basic commands you might
use in a regular bash terminal. Smallsh is able to perform typical open/read/write file
operations as you would expect in bash. You can create parent/child processes
for more advanced operations. Smallsh is also equipped to retrieve standard PATH variables
and display them for you; it can also handle parameter expansions. Feel free to experiment 
with keyboard signals while Smallsh is running. Take notice which ones are ignored by the 
custom signal handler!

Where you are ready to exit the program, type
the following command and hit enter:

```
exit
```
