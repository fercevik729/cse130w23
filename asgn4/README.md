#Assignment 4 directory

This directory contains source code and other files for Assignment 4.

Use this README document to store notes about design, testing, and
questions you have while developing your assignment.

# References
* Utilized Professor Quinn's starter code
* Referred to Vince's pseudocode for the Threadpool implementation & mutually exclusive file creation ideas
* Referenced manpages of getopt, pthread, and flock frequently

# Issues
1. Ran into issues with the queue not working, had to ensure the queue size was non-negative
2. Ran into issues with the worker threads not properly accessing the queue of connfds, so I made the queue global
3. Ran into some formatting issues with write\_to\_audit that I ironed out with the test cases
4. Ran into issues with the global file\_creation\_lock not properly working. Had to ensure that the file was truncated *after* flock()-ing it and not while opening it. This allowed me to pass stress\_mix consistently. I also had to make sure that handle\_get() and handle\_put() waited to acquire the fcl first thing in the procedure. This solved the issue with 2 PUT's incorrectly creating the same file and allowed me to pass the stress\_puts test consistently. 

# Considerations
* I decided to forego the headache of having to join the threads and deallocating the data I allocated, because 1. I only allocated data for the queue and the array of pthreads once and 2. it was not part of the requirements.

