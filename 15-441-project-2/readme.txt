###################################################################################
# README - 15-641 Project 2							  #
#										  #
# Description: this project implements the TCP interface with flow control and	  #
#	       congestion control(Reno).					  #
#										  #
#										  #
# Authors: Haodong Ji haodongj@andrew.cmu.edu					  #
#	   Yun Chen Sheng yunchens@andrew.cmu.edu  				  #
#										  #
#										  #
###################################################################################

[TOC-1] Table of Contents
-----------------------------------------------------------------------------------

        [TOC-1] Table of Contents
        [DES-2] Description of Files
        [USE-3] Use of interface


[DES-2] Description of Files
-----------------------------------------------------------------------------------
Here is a listing of all files associated with 15-641 Project 2 CP2 and what their
purpose is:

	README			- current document 
	Makefile		- for make convenience
	tests.txt		- a description of all tests
	src/			- all source files
		backend.c	- layer 3 backbone
		cmu_packet.c	- functions to construct and parse packets
		cmu_tcp.c	- functions of this tcp interface
		client.c	- source code for a testing client/initiator
		server.c	- source code for a testing server/lisener
	inc/			- all header files
		backend.h	- header file for backend.c
		cmu_packet.h	- header file for cmu_packet.c
		cmu_tcp.h	- header file for cmu_tcp.c
		global.h	- global declarations
		grading.h	- global declarations under instructor’s will
	build/			- make places all object files in this directory
	tests/			- all testing scripts and files
	utils/			- utilities to help capture/analyze packets
		capture_packets.sh	- utility to capture and analyze packets
		tcp.lua		- plugin to parse pcap file in cmu_packet format


[USE-3] Use of interface
-----------------------------------------------------------------------------------
	int cmu_socket(cmu_socket_t * dst, int flag, int port, char * serverIP);
		Creates a socket of type cmu_socket, which is a type of TCP socket.
		Returns 0 on success, other return value indicates failure during
		socket creation.

	int cmu_close(cmu_socket_t * sock);
		Closes the cmu_socket. Upon successful completion, a value of 0 is
		returned.  Otherwise, a value of -1 is returned.

	int cmu_read(cmu_socket_t * sock, char* dst, int length, int flags);
		Reads [length] of bytes from cmu_socket [sock] into buffer [dst].
		Returns the number of bytes read, or -1 if on error.
		Different flag options include:
			NO_FLAG: cmu_read blocks. It waits util there is data to
				be read from socket.
			NO_WAIT: cmu_read does not block.
		
	int cmu_write(cmu_socket_t * sock, char* src, int length);
		Writes data [length] bytes of data from buffer [src]to cmu_socket 
		[sock]. Returns 0 on success.

