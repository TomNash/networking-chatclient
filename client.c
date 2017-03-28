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

	struct reg_packet{
		int type;
		int group;
	};

	struct data_packet{
		int type;
		int group;
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
	char recv_msg[MAX_LINE];

	int s;
	int new_s;

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
	}
	else if(ntohs(packet_reg.type) == 231) {
		printf("Selected group is curently full\n");
		exit(1);
	}
	else if(ntohs(packet_reg.type) == 241) {
		printf("Cannot create new group, max count reached\n");
		exit(1);
	}

	packet_data.type = htons(221);
	packet_data.group = packet_reg.group;

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
			fgets(kb_msg, MAX_LINE, stdin);
			strcpy(packet_data.data, kb_msg);
			if (send(s, &packet_data, sizeof(packet_data), 0) < 0) {
				printf("\nMessage send failed");
			}
				printf("message sent\n");
		}
	}
}
