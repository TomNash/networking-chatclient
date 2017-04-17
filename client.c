#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>

#define SERVER_PORT 6990
#define MAX_LINE 100

int main(int argc, char* argv[]) {

	// Registration packet
	struct reg_packet{
		int type;
		int group;
		int client_id;
	};

	// Data packet
	struct data_packet{
		int type;
		int group;
		int client_id;
		char username[MAX_LINE];
		char data[MAX_LINE];
	};

	struct reg_packet packet_reg;
	struct data_packet packet_data;
	struct hostent *host_info;
	struct sockaddr_in sin;

	char *host;
	char *group;
	char *request;
	char clientname[128];
	char kb_msg[MAX_LINE];
	char username[MAX_LINE];
	char recv_msg[MAX_LINE];

	int s;
	int new_s;
	int client_id;

	fd_set readfds;

	// if three inputs, parse accordingly
	if (argc == 3) {
		host = argv[1];
		group = argv[2];
	}
	else{
		fprintf(stderr, "usage: client [server hostname] [chat group number]\n");
		exit(1);
	}

	// Get username
	printf("Enter your name: ");
	fgets(username, sizeof(username), stdin);
	username[strlen(username)-1] = '\0';

	// get hostname of client and request, copy to packets
	gethostname(clientname, sizeof clientname);
	int groupid = atoi(group);
	packet_reg.group = htons(groupid);
	packet_reg.type = htons(121);

	// translate host name into peer's IP address
	host_info = gethostbyname(host);
	if (!host_info) {
		fprintf(stderr, "unkown host: %s\n", host);
		exit(1);
	}

	// active open
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcpclient: socket");
		exit(1);
	}

	// build address data structure
	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(host_info->h_addr, (char *)&sin.sin_addr, host_info->h_length);
	sin.sin_port = htons(SERVER_PORT);

	// attempt to connect to server
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcpclient: connect");
		close(s);
		exit(1);
	}
	// send registration packets
	if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
		printf("Registration send failed\n");
		exit(1);
	}
	// receive response ACK
	recv(s, &packet_reg, sizeof(packet_reg), 0);
	if(ntohs(packet_reg.type) == 221) {
		printf("Successfully registered to chat group %d:\n\n", ntohs(packet_reg.group));
		client_id = ntohs(packet_reg.client_id);
	}
	else if(ntohs(packet_reg.type) == 231) {
		printf("Selected group is curently full\n");
		exit(1);
	}
	else if(ntohs(packet_reg.type) == 241) {
		printf("Cannot create new group, max count reached\n");
		exit(1);
	}

	while (1) {
		// Initialize file descriptor set
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(s, &readfds);
		// Check which ones changed
		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		// If packet received, print
		if(FD_ISSET(s, &readfds)){
			recv(s, &packet_data, sizeof(packet_data), 0);
			printf("%s: %s",packet_data.username, packet_data.data);
			fflush(stdout);
		}
		// If user sent a message
		if(FD_ISSET(0, &readfds)) {
			fgets(kb_msg, MAX_LINE, stdin);
			strcpy(packet_data.data, kb_msg);
			strcpy(packet_data.username, username);
			packet_data.client_id = htons(client_id);
			packet_data.type = htons(221);
			packet_data.group = packet_reg.group;
			if (send(s, &packet_data, sizeof(packet_data), 0) < 0) {
				printf("\nMessage send failed");
			}
		}
	}
}
