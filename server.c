#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>
#include<fcntl.h>

#define SERVER_PORT 6990
#define MAX_LINE 100
#define MAX_PENDING 5
#define MAX_CLIENTS 20

pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msg_lock = PTHREAD_MUTEX_INITIALIZER;

// Group struct
struct group {
	int groupid;
	int clients[4];
	int count;
};
// Support 5 groups
struct group group_list[5];

int group_index = -1; // counter for table, used to know if empty

// Registration packet
struct reg_packet {
	int type;
	int group;
	int client_id;
};

// Data packet
struct data_packet {
	int type;
	int group;
	int client_id;
	char username[MAX_LINE];
	char data[MAX_LINE];
};

// Set up linked list
struct buffer_list{
	struct data_packet data;
	struct buffer_list* next;
};
struct buffer_list* head = NULL;
struct buffer_list* next = NULL;
struct buffer_list* last = NULL;

void *multicaster() {
	char *filename;
	char text[100];
	struct data_packet filedata;
	int fd;
	int send_sock;
	int file_chunk = 0;
	int seq_no = 0;
	int nread;
	int client;
	int group_id;

	// continuously send data
	while (1) {
		sleep(1);
		// Check if message to send
		while (head != NULL){
			// Get the client who sent the message
			client = ntohs(head->data.client_id);
			// Get client's group
			group_id = ntohs(head->data.group);
			// For every client in the group, minus the sender, send the message
			for (int i = 0; i <= group_index; i++) {
				if (group_list[i].groupid==group_id) {
					printf("Message from %s sent to group %i\n", &head->data.username, group_id);
					for (int j = 0; j < group_list[i].count; j++) {
						if (group_list[i].clients[j]!=client) {
							send(group_list[i].clients[j], &head->data, sizeof(head->data), 0);
						}
					}
				}
			}
			// Change link on the list
			if(head->next != NULL){
				next = &head->next;
				head = next;
			} else{
				head = NULL;
			}
		}
	}
}

void *join_handler(int newsock) {
	struct reg_packet packet_reg;
	int group_id;
	int i = 0;
	int inserting = 1;

	if (recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0) {
		printf("Could not receive RG-%d\n", i+1);
		exit(1);
	} else {
		group_id = ntohs(packet_reg.group);
		printf("Registration received on %d for group %d\n", newsock, group_id);
	}

	packet_reg.type = htons(221);
	pthread_mutex_lock(&my_mutex);

	if (group_index < 0) { // no groups initialized
		group_index++;
		group_list[group_index].groupid = group_id;
		group_list[group_index].count = 1;
		group_list[group_index].clients[0] = newsock;
		inserting = 0;
	} else { // at least one group exists
		do {
			if (group_list[i].groupid == group_id) { //found group
				int client_counter = group_list[i].count;
				if (client_counter == 4) { // group full
					printf("Group %d full\n", group_id);
					packet_reg.type = htons(231);
					inserting = -1;
				}
				else { // spot in group
					group_list[i].clients[client_counter] = newsock;
					group_list[i].count++;
					inserting = 0;
				}
			}
			i++;
		} while (i < group_index && inserting);

		if (inserting == 1) { // couldn't find group
			if (group_index == 4) {
				printf("Too many groups exist\n");
				packet_reg.type = htons(241);
				inserting = -1;
			} else {
				group_index++;
				group_list[group_index].groupid = group_id;
				group_list[group_index].count = 1;
				group_list[group_index].clients[0] = newsock;
				inserting = 0;
			}
		}
	}
	pthread_mutex_unlock(&my_mutex);

	// send response acknowledging registration
	packet_reg.client_id=htons(newsock);
	if(send(newsock, &packet_reg, sizeof(packet_reg), 0) < 1) {
		printf("ACK send failed\n");
		exit(1);
	}
	pthread_exit(inserting);
}

int main(int argc, char* argv[]) {

	int sock_comm, new_s;
	int req_no;
	int len;
	int exit_value;
	pthread_t threads[3];

	struct reg_packet packet_reg;
	struct data_packet packet_data;

	struct hostent *he;
	struct in_addr **addr_list;
	struct sockaddr_in sin;
	struct sockaddr_in client_addr;

	// passive open socket
	if ((sock_comm = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcpserver: socket");
		exit(1);
	}

	// build address data structure
	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(SERVER_PORT);

	// bind socket and listen
	if (bind(sock_comm,(struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcpclient: bind");
		exit(1);
	}
	listen(sock_comm, MAX_PENDING);

	// start multicaster
	pthread_create(&threads[2], NULL, multicaster, NULL);

	int n = 0;
	int max_sock;
	int sd;
	int client_socket[MAX_CLIENTS];
	fd_set readfds;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		client_socket[i] = 0;
	}

        while (1) {
		// initialize file descriptors
		FD_ZERO(&readfds);
		FD_SET(sock_comm, &readfds);
		// find max socket of all connections
		max_sock = sock_comm;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			sd = client_socket[i];
			if (sd > 0) {
				FD_SET(sd, &readfds);
			}
			if (sd > max_sock) {
				max_sock = sd;
			}
		}
		// check activity on all sockets
		int activity = select(max_sock+1, &readfds, NULL, NULL, NULL);
		if (FD_ISSET(sock_comm, &readfds)) {
	       	        if ((new_s = accept(sock_comm, (struct sockaddr *)&client_addr, &len)) < 0) {
	                       	perror("tcpserver: accept");
				exit(1);
			}
			pthread_create(&threads[0], NULL, join_handler, new_s);
			pthread_join(threads[0], &exit_value);
			if (exit_value == 0) {
				for (int i = 0; i < MAX_CLIENTS; i++) {
					if (client_socket[i] == 0) {
						client_socket[i] = new_s;
						n++;
						break;
					}
				}
			} else {
				close(new_s);
			}
		}
		// check for messages from clients
		for (int i = 0; i < n; i++) {
			sd = client_socket[i];
			if (FD_ISSET(sd, &readfds)) {
				if (recv(sd, &packet_data, sizeof(packet_data), 0) < 0) {
					close(sd);
					client_socket[i] = 0;
					n--;
				} else {
					pthread_mutex_lock(&msg_lock);
					if (head != NULL){
						head->next=malloc(sizeof(struct buffer_list));
						memcpy(&head->next->data, &packet_data, sizeof(packet_data));
						pthread_mutex_unlock(&msg_lock);
					} else{
						head = malloc(sizeof(struct buffer_list));
						memcpy(&head->data,&packet_data, sizeof(packet_data));
						pthread_mutex_unlock(&msg_lock);
					}
				}
			}
		}
	}
}
