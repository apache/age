To run the sample project, make sure you have flex and bison installed in you system.
Run the following commands in proper sequence as shown below from the terminal. 
It's a better practice to run these files by keeping them in a separate directory.
Run the commands using terminal from inside the directory containing these files.



$ flex match.l
$ bison -d match.y
$ gcc -o match match.tab.c lex.yy.c
$ ./match
