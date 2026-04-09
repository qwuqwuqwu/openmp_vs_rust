You need to compare the languages based on:

* programmability (i.e. how easy it is to program with the language)
* scalability (handling large number of threads)
* runtime overhead of the language (measure how long it takes to create threads, terminate threads, synchronize, etc)
* the amount of control given to the programmer (can the programmer decide which thread goes to which core, can the programmer decide which variables to be shared and which ones to be prviate, etc).
* Performance: on the same machine and same problem size

To test each of the above, you need to write a set of benchmark programs, not less than four program of different characteristics,  in each of the languages and compare.
Each program must test one of the above points.

One of your conclusions, after the comparison, is to come up with a set of guidelines on when to use each language from the one you compared.
