// Ram Karnani 2012B3A7750P

1) To create an executable use command
   mpicc ABCDnews.c

2) Format for execution
   mpirun -n #p ./a.out #p #s #r #e #reportersForEditor1 .. #reportersForEditorN #sourceFileName1 .. #sourceFileNameK

   #s = #sources
   #p = #processes
   #r = #reporters
   #e = #editors

Example:
	mpirun -n 20 ./a.out 20 2 17 1 17 test1 test2

	mpirun -n 20 ./a.out 20 3 14 3 5 5 4 test1 test2 test3
