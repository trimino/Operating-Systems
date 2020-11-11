* Compiling and Installing the kernel 
	- We will be building our kernel out of tree
	> cd code/kernel-code/linux-5.8

* To build the kernel
	> make O=../build 
	> cd ../build 

* Install the new kernel
	> make modules_install install 

* Reboot machine 
	> reboot 
	
