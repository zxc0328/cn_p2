#include "cmu_tcp.h"

/*
 * Param: sock - used for reading and writing to a connection
 *
 * Purpose: To provide some simple test cases and demonstrate how 
 *  the sockets will be used.
 *
 */
void functionality(cmu_socket_t  * sock){
    char buf[30000];
    FILE *fp;
    int n, total = 0;
    struct timeval start_timer, current_timer;

    // n = cmu_read(sock, buf, 200, NO_FLAG);
    // printf("R: %s\n", buf);
    // printf("N: %d\n", n);
    // cmu_write(sock, "hi there im server 1", 21);
    // n = cmu_read(sock, buf, 200, NO_FLAG);
    // printf("R: %s\n", buf);
    // printf("N: %d\n", n);
    // cmu_write(sock, "hi there im server 2", 21);
    // sleep(5);

    gettimeofday(&start_timer,NULL);
    gettimeofday(&current_timer,NULL);
    fp = fopen("./test/file.c", "w+");
    while(current_timer.tv_sec - start_timer.tv_sec < 120){
        n = cmu_read(sock, buf, 30000, NO_WAIT);
        total += n;
        fwrite(buf, 1, n, fp);
if(n!=0)
    printf("server.c: wrote %d bytes to file.\n", n);
        gettimeofday(&current_timer,NULL);
    }
    fclose(fp);
    return;
}


/*
 * Param: argc - count of command line arguments provided
 * Param: argv - values of command line arguments provided
 *
 * Purpose: To provide a sample listener for the TCP connection.
 *
 */
int main(int argc, char **argv) {
	int portno;
    char *serverip;
    char *serverport;
    cmu_socket_t socket;
    
    serverip = getenv("server15441");
    if (serverip) ;
    else {
        serverip = "10.0.0.1";

    }

    serverport = getenv("serverport15441");
    if (serverport) ;
    else {
        serverport = "15441";
    }
    portno = (unsigned short)atoi(serverport);


    if(cmu_socket(&socket, TCP_LISTENER, portno, serverip) < 0)
        exit(EXIT_FAILURE);

    functionality(&socket);
printf("I am closing: server.\n");
    if(cmu_close(&socket) < 0)
        exit(EXIT_FAILURE);
    return EXIT_SUCCESS;
}