#Assignment 3 directory

This directory contains source code and other files for Assignment 3.

Use this README document to store notes about design, testing, and
questions you have while developing your assignment.

## Design Notes
* I utilized some pseudocode from Vincent's Feb 9 Section and Professor Quinn's
Feb 9 Lecture

* I frequently referred to man pages for POSIX threads and semaphores

* I decided to go with a circular array to avoid having to allocate extra memory for each element that was pushed to the queue and to avoid the complexity of dealing with pointers in a thread-safe manner

* I decided to use semaphores over mutexes and condvars because I found the implementation to be much shorter and simpler, with less places to go wrong

* I decided to use one binary semaphore for push and pop to ensure no race conditions between the two of them and two counting semaphores to indicate when the queue was empty or full

* I decided that the critical regions would be within the push and pop operations because this was where threads accessed a shared resource--the queue itself--and figured that each would need to wait on the counting semaphores (full\_spaces and empty\_spaces) first before waiting on the binary "lock" semaphore. Then after the thread is done waiting it can perform its necessary operation, post to the binary lock and post to the opposite counting semaphore. 

* To test the queue I decided to go head first with multiple producers and consumers and decided to push a unique set of numbers and see if the order in which they were popped was valid

* I reverted to one binary semaphore instead of two for fear of introducing new concurrency issues

