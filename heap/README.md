# Heap Management 


## Summary

This mini operating systems project is to build and implement a custom malloc and free function. This project will I needed to implement a library that interacts with the operating system to perform heap management on behalf of a user process, 




## Building and Running the Code
	* The code compiles into four shared libraries and six test programs. To build the code to your top level directory and type:
		$ make

	* Once you have the library, you can use it to override the existing malloc by using LD_PRELOAD:
			$ env LD_PRELOAD=lib/libmalloc-ff.so cat README.md
		or
			$ env LD_PRELOAD=lib/libmalloc-ff.so tests/test 1


	* Best-Fit: libmalloc-bf.so
	* First-Fit: libmalloc-ff.so
	* Next-Fit: libmalloc-nf.so
	* Worst-Fit: libmalloc-wf.so
