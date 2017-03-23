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

struct group {
	int groupid;
	int clients[4];
	int count;
};
struct group group_list[5];

int group_index = -1; // counter for table, used to know if empty

struct reg_packet {
	int type;
	int group;
};

struct data_packet {
	int type;
	int group;
	char data[MAX_LINE];
};

void *multicaster() {
	char *filename;
	char text[100];
	struct data_packet filedata;
	int fd;
	int send_sock;
	int file_chunk = 0;
	int seq_no = 0;
	int nread;

	// continuously send data
	while (1) {
		sleep(1);
	}
}
/*
void *leave_handler(struct reg_packet *rec) {
	int newsock;
	int position;
	int send_sock;
	int find_sock = atoi(rec->);

	printf("Leaving\n");
	for (int j=0;j<=counter;j++) {
		pthread_mutex_lock(&my_mutex);
		send_sock = record[j].sockid;
		pthread_mutex_unlock(&my_mutex);
		if (send_sock == find_sock) {
			position = j;
		}
	}
	for (int c=position; c < counter-1; c++) {
		pthread_mutex_lock(&my_mutex);
		record[c] = record[c+1];
		pthread_mutex_unlock(&my_mutex);
	}
	counter--;
	pthread_exit(NULL);
}

void *send_handler(struct data_packet) {

	for (int j=0;j<=counter;j++) {
		pthread_mutex_lock(&my_mutex);
		send_sock = record[j].sockid;
		pthread_mutex_unlock(&my_mutex);
		if (send_sock == new_s) {
			position = j;
		}
	}
	for (int c=position; c < counter-1; c++) {
		pthread_mutex_lock(&my_mutex);
		record[c] = record[c+1];
		pthread_mutex_unlock(&my_mutex);
	}
	pthread_exit(NULL);

}
*/

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
		printf("Creating new group at position %d\n", group_index);
		group_list[group_index].groupid = group_id;
		group_list[group_index].count = 1;
		group_list[group_index].clients[0] = newsock;
	} else { // at least one group exists
		do {
			if (group_list[i].groupid == group_id) { //found group
				int client_counter = group_list[i].count;
				if (client_counter == 4) { // group full
					printf("Group %d full\n", group_id);
					packet_reg.type = htons(231);
					inserting = 0;
				}
				else { // spot in group
					printf("Adding to group list position %d\n", i);
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
			} else {
				group_index++;
				printf("Creating group at position %d\n", group_index);
				group_list[group_index].groupid = group_id;
				group_list[group_index].count = 1;
				group_list[group_index].clients[0] = newsock;
			}
		}
	}
	pthread_mutex_unlock(&my_mutex);

	// send response acknowledging registration
	packet_reg.group = packet_reg.group;
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

	for (int i=0; i < MAX_CLIENTS; i++) {
		client_socket[i] = 0;
	}

        while (1) {
		FD_ZERO(&readfds);
		FD_SET(sock_comm, &readfds);
		max_sock = sock_comm;
		for (int i=0; i < MAX_CLIENTS; i++) {
			sd = client_socket[i];
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > max_sock)
				max_sock = sd;
		}
		int activity = select(max_sock+1, &readfds, NULL, NULL, NULL);
		if (activity < 0) {
			printf("select error");
		}
		if (FD_ISSET(sock_comm, &readfds)) {
	       	        if ((new_s = accept(sock_comm, (struct sockaddr *)&client_addr, &len)) < 0) {
	                       	perror("tcpserver: accept");
				exit(1);
			}
			printf("New socket: %d\n", new_s);
			pthread_create(&threads[0],NULL,join_handler,new_s);
			pthread_join(threads[0],&exit_value);
			if (exit_value == 0) {
				for (int i=0; i < MAX_CLIENTS; i++) {
					if (client_socket[i] == 0) {
						client_socket[i] = new_s;
						n++;
						break;
					}
				}
			}
		}
		for (int i=0; i < n; i++) {
			sd = client_socket[i];
			printf("Socket %d\n", sd);
			if (FD_ISSET(sd, &readfds)) {
				if (recv(sd, &packet_data, sizeof(packet_data), 0) < 0) {
					close(sd);
					client_socket[i] = 0;
					n--;
				}
				else {
					printf("hello\n");
//					pthread_create(&threads[1],NULL,send_handler,&packet_data);
//					pthread_join(threads[1],&exit_value);
				}
			}
		}
	}
}
