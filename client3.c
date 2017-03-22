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
		short type;
		char data[MAX_LINE];
	};

	struct data_packet{
		short header;
		char data[MAX_LINE];
	};

	struct reg_packet packet_reg;
	struct data_packet packet_data;

	struct hostent *host_info;
	struct sockaddr_in sin;
	char *host;
	char *request;
	char clientname[128];

	int s;
	int new_s;
	int len;
	int max_count;
	int counter = 0;

	// if three inputs, parse accordingly
	if (argc == 3) {
		host = argv[1];
		max_count = atoi(argv[2]);
	}
	else{
		fprintf(stderr, "usage: client [server hostname] [request machine name]\n");
		exit(1);
	}

	// get hostname of client and request, copy to packets
	gethostname(clientname, sizeof clientname);
	strcpy(packet_reg.data, clientname);
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

	// send registration packet RG-1
	printf("Requesting registration RG-1 of %s on server running on %s at port %d\n", clientname, packet_reg.data, ntohs(sin.sin_port));
	if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
		printf("RG-1 send failed\n");
		exit(1);
	}

	// send registration packet RG-2
	printf("Requesting registration RG-2 of %s on server running on %s at port %d\n", clientname, packet_reg.data, ntohs(sin.sin_port));
	if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
		printf("RG-2 send failed\n");
		exit(1);
	}

	// send registration packet RG-3
	printf("Requesting registration RG-3 of %s on server running on %s at port %d\n", clientname, packet_reg.data, ntohs(sin.sin_port));
	if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
		printf("\nRG-3 send failed\n");
		exit(1);
	}
	// receive response ACK
	recv(s, &packet_reg, sizeof(packet_reg), 0);
	printf("%d\n", ntohs(packet_reg.type));
	if(ntohs(packet_reg.type) == 221) {
		printf("Received registration acknowledgement\nBeginning to receive multicast message:\n\n");
	}
	while (len = recv(s, &packet_data, sizeof(packet_data), 0)) {
                printf("%s", packet_data.data);
                fflush(stdout);
		counter++;
		if (counter == max_count) {
			close(s);
			break;
		}
        }
	// active open
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("tcpclient: socket");
		exit(1);
	}
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("tcpclient: connect");
		close(s);
		exit(1);
	}
	packet_reg.type = htons(321);
        if (send(s, &packet_reg, sizeof(packet_reg),0) < 0) {
		printf("\nLeave request send failed\n");
		exit(1);
	} else {
		printf("Leaving\n");
	}
	close(s);
}
