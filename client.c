#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>

#define SERVER_PORT 6990
#define REQUEST_NO 3
#define MAX_LINE 100

int main(int argc, char* argv[]) {

	struct packet{
		short type;
		char data[MAX_LINE];
	};

	struct packet packet_reg, packet_data;
	struct hostent *host_info;
	struct sockaddr_in sin;
	char *host;
	char *group;
	char *request;
	char clientname[128];
	char kb_msg[MAX_LINE];
	char recv_msg[MAX_LINE];

	int s;
	int new_s;
	int len;
	int max_count;
	int counter = 0;

	fd_set readfds;

	// if three inputs, parse accordingly
	if (argc == 3) {
		host = argv[1];
		group = argv[2];
	}
	else{
		fprintf(stderr, "usage: client [server hostname] [chat group name]\n");
		exit(1);
	}

	// get hostname of client and request, copy to packets
	gethostname(clientname, sizeof clientname);
	strcpy(packet_reg.data, group);
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
	for (int i=0; i < REQUEST_NO; i++) {
		sprintf(packet_reg.data, "test%d", i);
		printf("RG-%d sent\n", i+1);
		if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
			printf("RG-%d send failed\n", i+1);
			exit(1);
		}
	}
	// receive response ACK
	recv(s, &packet_reg, sizeof(packet_reg), 0);
	if(ntohs(packet_reg.type) == 221) {
		printf("Successfully registered to chat group %s:\n\n", group);
	}
	packet_data.type = htons(221);

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(s, &readfds);
		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if(FD_ISSET(s, &readfds)){
			recv(s, &packet_data, sizeof(packet_data), 0);
			strcpy(recv_msg, packet_data.data);
			printf(recv_msg);
			fflush(stdout);
		}
		if(FD_ISSET(0, &readfds)) {
			printf("hello\n");
			fgets(kb_msg, MAX_LINE, stdin);
			strcpy(packet_data.data, kb_msg);
			if (send(s, &packet_data, sizeof(packet_data), 0) < 0) {
				printf("\nMessage send failed");
			}
		}
	}
}
