/***************************************************************************

Written by: Shyam S R
DPS HW 1

Chatroom Client
1/28/2012

****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

#define port_no 11486
#define MSG_SIZE 8
#define MAX_PAYLOAD 246
struct msg {
        int type;
        int length;
};

struct thread_parms {

	int port;
	struct hostent* hp;
};

int sendCommand(int csd, char* buf, int type) {
	struct 	msg temp_msg;
	char 	temp_buf[MAX_PAYLOAD];
	char 	room_name[MAX_PAYLOAD];
	int	i;

	if(type == 0) i=6;
	else if(type == 1) i=4;
	else if(type == 2) i=6;
	
	for(; i< strlen(buf); i++) {	
		if(buf[i] == ' ') continue;
		else break;
	}
	strcpy(room_name, buf+i);
	memset(temp_buf, 0, MAX_PAYLOAD);
	temp_msg.type = type;
	temp_msg.length = sizeof(temp_msg) + strlen(room_name)+1;  //add 1 for NULL termination of string

	memcpy(temp_buf, &temp_msg, sizeof(temp_msg));
	memcpy(temp_buf+sizeof(temp_msg), room_name, strlen(room_name)+1);

	write(csd, temp_buf, temp_msg.length);
	
	return 0;
}

void* connectToChatroom(int port, struct hostent* hp, int master) {
        char    buf[256];
	char	temp_buf[256];

        int     csd;                                /* all socket descriptors */
        int     i;                                  /* iterators */
        int     bytes_read;
        int     temp_fd;

	fd_set  allfd, modfd;

        struct  sockaddr_in sock;                /* this is the socket struct used in connect() */

	printf("Successfully joined chatroom\n");
	memset(buf, 0, sizeof(buf));
        memset(&sock, 0, sizeof(sock));
        sock.sin_family = AF_INET;
        sock.sin_port   = htons(port);
	memcpy((char*)&sock.sin_addr.s_addr, hp->h_addr, hp->h_length);

        csd = socket(AF_INET, SOCK_STREAM, 0);  /* got a socket descriptor */
        if(csd == -1) {
                perror("Error creating socket");
                return NULL;
        }
        if(connect(csd, (struct sockaddr*)&sock, sizeof(struct sockaddr)) < 0) {
                perror("Error inside connect()");
                return NULL;
        }
	/* Initializing the file descriptors to select on */
        FD_ZERO(&allfd);        /* first, clear the allfd set */
        FD_SET(csd, &allfd);    /* adding chatroom to the set */
        FD_SET(0, &allfd);      /* adding stdin to set */
	FD_SET(master, &allfd);

	while(1) {
                modfd = allfd;
                select(FD_SETSIZE, &modfd, NULL, NULL, NULL);

                for(temp_fd = 0; temp_fd < FD_SETSIZE; temp_fd++) {
                        if(FD_ISSET(temp_fd, &modfd)) {

				memset(buf, 0, sizeof(buf));
				memset(temp_buf, 0, sizeof(buf));
                                if(temp_fd ==  0) {  // stdin
                                        
					bytes_read = read(0, buf, 256);
					buf[bytes_read-1] = '\0';
					write(csd, buf, strlen(buf));
					
                                } else if(temp_fd == csd) { /* for server chat messages */
					bytes_read = read(temp_fd, buf, 256);
					buf[bytes_read] = '\0';
					memcpy(temp_buf, buf, bytes_read);
					printf("%s \n", temp_buf);	// Display CHAT Message	
										
                                } else if(temp_fd == master) {
					struct msg temp_msg;
					bytes_read = read(temp_fd, buf, 8);
					memcpy(&temp_msg, buf, 8);
					if(temp_msg.type == 12) {
						printf("Chatroom being deleted, Shutting down connection\n");
						return NULL;
					}
				}
                        } //end of FD_ISSET
                } //end of for
        } //end of while



	return 0;
}
	
int main(int argc, char* argv[]) {

	char 	buf[256];

        int 	csd;                                /* all socket descriptors */
	int 	i;                                  /* iterators */
	int 	bytes_read;
	int	temp_fd;
	int 	flag = 1;				/* flag = 1 means there is no chat thread running, so can accpet input from stdin*/

        fd_set	allfd, modfd;

        char 	server_ip[35];
	struct	hostent* hp;
        struct 	sockaddr_in sock;                /* this is the socket struct used in connect() */
	struct	msg temp_msg;
	
	pthread_attr_t  attr;
        pthread_t       tid;

        if(argc < 2) {
                printf("The correct format is ./crc <server IP> \n");
                return -1;
        }
	pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	memset(&sock, 0, sizeof(sock));
        sock.sin_family = AF_INET;
        sock.sin_port   = htons(port_no);
        strcpy(server_ip ,argv[1]);
        if ( (hp = (struct hostent*)gethostbyname(server_ip)) == NULL) {	/*converting to proper IP*/
                perror("Failed in gethostbyname() ");
                return -1;
        }
        memcpy((char*)&sock.sin_addr.s_addr, hp->h_addr, hp->h_length);

        csd = socket(AF_INET, SOCK_STREAM, 0);  /* got a socket descriptor */
        if(csd == -1) {
                perror("Error creating socket");
                return -1;
        }
	if(connect(csd, (struct sockaddr*)&sock, sizeof(struct sockaddr)) < 0) {
		perror("Error inside connect()");
		return -1;
	}
	/* Initializing the file descriptors to select on */
        FD_ZERO(&allfd);        /* first, clear the allfd set */
        FD_SET(csd, &allfd);    /* adding client to the set */
        FD_SET(0, &allfd);      /* adding stdin to set */
        printf("***********Welcome to Chatroom client***********\n");
	
	while(1) {
		modfd = allfd;
                select(FD_SETSIZE, &modfd, NULL, NULL, NULL);

                for(temp_fd = 0; temp_fd < FD_SETSIZE; temp_fd++) {
                	if(FD_ISSET(temp_fd, &modfd)) {

		      		if((temp_fd ==  0) ) {  // stdin
			 		bytes_read = read(0, buf, 256);
			 		buf[bytes_read-1] = '\0';  // subract 1 to ignore newline character, but insert null termination in its place 
			 		if((strncasecmp(buf,"create ",7) == 0) ) { // send create message to server 
			  			sendCommand(csd, buf, 0);
	
					} else if((strncasecmp(buf,"join ",5) == 0) ) {
					  	sendCommand(csd, buf, 1);	
				
					} else if((strncasecmp(buf,"delete ",7) == 0) ) {
					  	sendCommand(csd, buf, 2);	

			 		} else {
						printf("Please enter a valid command: create <room_name>, join <room_name>, delete <room_name>\n");
					}

		      		} else if(temp_fd == csd) { /* for client sd */

					bytes_read = read(temp_fd, buf, MSG_SIZE);
                                        memcpy(&temp_msg, buf, sizeof(temp_msg));
					if((temp_msg.type == 10)  ) { 
						printf("Successfully created chatroom\n");
					
					} else if((temp_msg.type == 11) ) {
						connectToChatroom(temp_msg.length, hp, csd);
                                                fflush(stdin);
	
					} else if((temp_msg.type == 15) ) {
						printf("Chatroom already exists. Type join <room_name> to join\n");
					}
						
		      		} 
		  	} //end of FD_ISSET
		} //end of for
	} //end of while

	close(csd);
	return 0;
}
