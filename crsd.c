/***************************************************************************

Written by: Shyam S R
DPS HW 1

Chatroom server
Implemented using 	threads for each chatroom and
			select() for the command handling from clients
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
#include <signal.h>

#define port_no 11486
#define MSG_SIZE 8
#define MAX_PAYLOAD 246
#define MAX_CHATROOMS 36
#define MAX_CHATROOM_NAME 56
#define MAX_CLIENTS 64

struct msg {
	int type;					/* type of message 0:create 1:Join 2:Delete 10:CreateACK 11:JoinACK */
	int length;					/*Length of pacekt, interchangably used as port number for Join and Create ACKs*/
};

struct chatroom_inst {
	int 		sd;				/* socket descriptor of new chatroom socket */
	int  		port;				/* this is the slave socket port */
	pthread_t       id;				/* thread id of particular chatroom */
	char 		name[MAX_CHATROOM_NAME];	/* chat room name */
	int		clients[MAX_CLIENTS];		/* holds all the client's master socket descriptors */
};


struct chatroom_inst* locateChatroom(char* roomname, struct chatroom_inst* chat_ptr) {
	
	int     i;            /* iterators */

        /* check for existing chat rooms ~case-sensitive*/
        for(i=0;i<MAX_CHATROOMS;i++) {
                if((strcmp(chat_ptr[i].name, roomname) == 0)  && (chat_ptr[i].port != 0)) { // valid chat room exists
                        return &chat_ptr[i];
                }
	}
	return NULL;
}

struct chatroom_inst* servJoin(int csd, char* roomname, struct chatroom_inst* chat_ptr) {

        int     i;            /* iterators */

	struct chatroom_inst* local_chat_ptr;
	
	local_chat_ptr = locateChatroom(roomname, chat_ptr);

	if(local_chat_ptr != NULL) {
		for(i=0;i<MAX_CLIENTS;i++) {
        	        if(local_chat_ptr->clients[i] == 0) { // got some space to store the client exists
                	        local_chat_ptr->clients[i] = csd;
				return local_chat_ptr;
	                }
        	}
	}
        return NULL;
}

void* chatroomMain(void* chat_room_heap) {

        char    buf[MAX_PAYLOAD];

	int	sd, csd, temp_fd;
        int     i,j;					/*iterattors*/
	int	br;					/*return val for read*/
	int	num_clients = 0;			/* number of clients in chat room */
	int	clients[MAX_CLIENTS];			/* holds all the client's slave(returned from accept() ) sockets */

        fd_set  allfd, modfd;                           /* used in fd_isset and select() */

	struct chatroom_inst chat_room_parms;

	memcpy(&chat_room_parms, (struct chatroom_inst*)chat_room_heap, sizeof(struct chatroom_inst));
	free(chat_room_heap);

	memset(clients, 0, sizeof(int)*MAX_CLIENTS);
	sd = chat_room_parms.sd;

	listen(sd, 1);

        /* Initializing the file descriptors to select on */
        FD_ZERO(&allfd);        /* first, clear the allfd set */
        FD_SET(sd, &allfd);    /* adding chatroom to the set */
        printf("\n New Chatroom thread Started\n");
        while(1) {
                modfd = allfd;
                select(FD_SETSIZE, &modfd, NULL, NULL, NULL);

                for(temp_fd = 0; temp_fd < FD_SETSIZE; temp_fd++) {
                	if(FD_ISSET(temp_fd, &modfd)) {
				memset(buf, 0, sizeof(buf));
                        	if(temp_fd == sd) {

                                	csd = accept(sd, NULL, NULL);
                                	FD_SET(csd, &allfd);
					for(i=0;i<MAX_CLIENTS;i++) {
						if(clients[i] == 0) {  // found empty spot 
							clients[i] = csd;
							break;
						}
					} //create a client chat record to register name

					sprintf(buf, "Number of other clients in chatroom:  %d", num_clients);
					write(csd, buf, strlen(buf));
					num_clients++;
					
	                        } else {  /* Client fd: all data coming here should be forwarded to other clients in the chatroom */
					if((br = read(temp_fd,buf, MAX_PAYLOAD)) > 0) { 
						for(i=0;i<MAX_CLIENTS;i++) {
							if((clients[i] != temp_fd) && (clients[i] != 0)) {
								write(clients[i], buf, br);
							}
						}
					} else if(br <= 0) {
						FD_CLR(temp_fd, &allfd);
						for(i=0;i<MAX_CLIENTS;i++) {
							if(clients[i] == temp_fd) 
								clients[i] =0;
						}
						num_clients--;
						close(csd);
					}
				} //end of second if check
			} // end of FD__ISSET
		} //end of for
	} //end of while

}

struct chatroom_inst* addChatroom(int csd, char *roomname, struct chatroom_inst* chat_ptr) {

	int     sd;		/* socket descriptors */
        int     i,j;		/* iterators */

	struct sockaddr_in	sock;

	static int port = port_no;

        memset(&sock, 0, sizeof(sock));

	/* check for existing chat rooms ~case-sensitive*/
	for(i=0, j = -1;i<MAX_CHATROOMS;i++) {
		if((strcmp(chat_ptr[i].name, roomname) == 0)  && (chat_ptr[i].port != 0)) { // valid chat room exists
			sendCreateResp(csd, chat_ptr[i].port, 15);
			return NULL;
		}

		else if((chat_ptr[i].port == 0) && (j ==-1)) {
			j = i;
		}
	}

	if( j == -1)
		return NULL;

        sd = socket(AF_INET, SOCK_STREAM, 0);  /* get a socket descriptor*/
        if(sd == -1) {
                perror("Error while creating chatroom socket");
                return NULL;
        }

        sock.sin_family = AF_INET;
	port++;    //srs: get better way to assign port number, may be assign zero and let OS decide
        sock.sin_port   = htons(port);
	sock.sin_addr.s_addr = INADDR_ANY;
        
	if(bind(sd, (struct sockaddr*)&sock, sizeof(struct sockaddr_in)) == -1) {
                perror("Error in bind() while creating chatroom socket");
                return NULL;
        }

	chat_ptr[j].sd = sd;
	chat_ptr[j].port = port;
	strcpy(chat_ptr[j].name, roomname);

	return &chat_ptr[j];	
}

int sendCreateResp(int csd, int new_port, int type) {
        struct  msg temp_msg;
        char    temp_buf[MAX_PAYLOAD];
        int     i;

        memset(temp_buf, 0, MAX_PAYLOAD);
        temp_msg.type = type; // Create ACK type
        temp_msg.length = new_port; // Length field contains port number of new port created for chatroom

        memcpy(temp_buf, &temp_msg, sizeof(temp_msg));

        write(csd, temp_buf, MSG_SIZE);

        return 0;
}

int sendJoinResp(int csd, int new_port) {
        struct  msg temp_msg;
        char    temp_buf[MAX_PAYLOAD];
        int     i;

        memset(temp_buf, 0, MAX_PAYLOAD);
        temp_msg.type = 11; // Join ACK type
        temp_msg.length = new_port; // Length field contains port number of new port created for chatroom

        memcpy(temp_buf, &temp_msg, sizeof(temp_msg));

        write(csd, temp_buf, MSG_SIZE);

        return 0;
}

int sendDeleteResp(int csd, struct chatroom_inst* chat_ptr) {
	struct  msg temp_msg;
        char    temp_buf[MAX_PAYLOAD];
        int     i;

        memset(temp_buf, 0, MAX_PAYLOAD);
        temp_msg.type = 12; // Delete ACK type: send to all clients in chat room
        temp_msg.length = sizeof(temp_msg); 

        memcpy(temp_buf, &temp_msg, sizeof(temp_msg));

	for(i=0;i<MAX_CLIENTS;i++) {
		if((chat_ptr->clients[i] != csd) && (chat_ptr->clients[i] != 0))
		        write(chat_ptr->clients[i], temp_buf, MSG_SIZE);
	}


	return 0;
}


int main(int argc, char* argv[]) {
        int 	ssd, csd;                 		/* all socket descriptors */
	int 	temp_fd;

	char 	server_ip[35];
	char 	buf[256];
	char 	msg_buf[8];

	fd_set 	allfd, modfd;                           /* used in fd_isset and select() */

        struct sockaddr_in 	sock;                /* this is the socket struct  for server */
	struct hostent* 	hp;
	
	struct chatroom_inst 	chatrooms[MAX_CHATROOMS];  /* list of chat rooms */

	memset(chatrooms, 0, sizeof(struct chatroom_inst) * MAX_CHATROOMS);
	memset(&sock, 0, sizeof(sock));


        ssd = socket(AF_INET, SOCK_STREAM, 0); 	/* got a socket descriptor for server*/
	if(ssd == -1) {
                perror("Error creating socket");
                return -1;
        }

        sock.sin_family = AF_INET;
        sock.sin_port   = htons(port_no);
	sock.sin_addr.s_addr = INADDR_ANY; // does not take care of cases where server has multiple interfaces
/*
	strcpy(server_ip ,argv[1]);

	if ( (hp = (struct hostent*)gethostbyname(server_ip)) == NULL) {
		perror("Failed in gethostbyname() ");
		return -1;
	}
	memcpy((char*)&sock.sin_addr.s_addr, hp->h_addr, hp->h_length);
*/
	if(bind(ssd, (struct sockaddr*)&sock, sizeof(struct sockaddr_in)) == -1) {
                perror("Error in bind()");
                return -1;
        }
/*
	// code to get a random port number: disable for time-being: srs
	socklen_t size = sizeof(struct sockaddr_in);
	getsockname(ssd, (struct sockaddr*)&sock, &size);
	printf("\nport %d\n ", sock.sin_port );
*/
        listen(ssd, 1);
	
	/* Initializing the file descriptors to select on */
        FD_ZERO(&allfd); 	/* first, clear the allfd set */
        FD_SET(ssd, &allfd); 	/* adding server to the set */
        FD_SET(0, &allfd);  	/* adding stdin to set */
        printf("\n *************Welcome to Chatroom Server*************\n");
        while(1) {
		modfd = allfd;
                select(FD_SETSIZE, &modfd, NULL, NULL, NULL);
		
		for(temp_fd = 0; temp_fd < FD_SETSIZE; temp_fd++) {
                  if(FD_ISSET(temp_fd, &modfd)) {

                    switch(temp_fd) {

                      case 0: /* stdin */
			// write server side utilities if necessary : srs
	
			break;

                      default:/*client or server socket fd */
                        if(temp_fd == ssd) {

			  	csd = accept(ssd, NULL, NULL);
                          	FD_SET(csd, &allfd);
			  } else {  /* Client fd */
				char 	msg_buf[MSG_SIZE];
				char	buf[MAX_PAYLOAD];	
	        		char    room_name[MAX_PAYLOAD];
        			int     i,j;

				struct  msg 		temp_msg;
				struct chatroom_inst* 	local_chat_ptr = NULL;

				void* 	(*init_chatroom)() = &chatroomMain;
				void* 	thread_args;
				
				pthread_attr_t 	attr;
				pthread_t 	tid;
				
				pthread_attr_init(&attr);		
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

				memset(buf, 0, sizeof(buf));
				memset(msg_buf, 0, sizeof(buf));
				msg_buf[19] = 0;
				i = read(temp_fd, msg_buf, MSG_SIZE);
				if(i > 0) {
					memcpy(&temp_msg, msg_buf, sizeof(temp_msg));
					read(temp_fd, buf,temp_msg.length - sizeof(temp_msg) );
		        		memcpy(room_name, buf, temp_msg.length - sizeof(temp_msg));
					if(temp_msg.type == 0) { //create
						local_chat_ptr = addChatroom(temp_fd, room_name, chatrooms);
                          			if(local_chat_ptr != NULL) { 
							thread_args = (void*) malloc(sizeof(struct chatroom_inst));
							memcpy((struct chatroom_inst*)thread_args, local_chat_ptr, sizeof(struct chatroom_inst));
							pthread_create(&tid, &attr, init_chatroom, thread_args);
							local_chat_ptr->id = tid;  // only if thread creation success
							sendCreateResp(temp_fd, local_chat_ptr->port,10);
						}/* addChatroom returned a new chatroom socket, so create new thread */
						
					} else if(temp_msg.type == 1) { //join
						local_chat_ptr = servJoin(temp_fd, room_name, chatrooms);
						if(local_chat_ptr != NULL) {
							sendJoinResp(temp_fd, local_chat_ptr->port);
						}
	
					} else if(temp_msg.type == 2) { //delete
						local_chat_ptr = locateChatroom(room_name, chatrooms);
						printf("Deleting a chatroom ...\n");
						if(local_chat_ptr != NULL) {

							local_chat_ptr->port = 0;			/*logical deletion of chatroom */
							close(local_chat_ptr->sd);
							sendDeleteResp(temp_fd, local_chat_ptr);
							for(j=0;j<MAX_CLIENTS;j++) {
								if(local_chat_ptr->clients[j]!=0) {
									FD_CLR(local_chat_ptr->clients[j], &modfd);
									local_chat_ptr->clients[j] = 0;
								}
							}
							pthread_cancel(local_chat_ptr->id);
						}
					}

		        	} else if(i <= 0) {
					FD_CLR(temp_fd, &allfd);
					// Search all chatrooms for this client sd and then remove it
					close(temp_fd);

				//the client teminated the connection

				}
			}
		    } /* end of switch */
		  }  /* end of if */
		} /* end of for loop iter over FD */
	} /* end of while */

	
	return 0;

}
