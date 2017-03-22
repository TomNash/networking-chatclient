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
#define REQUEST_NO 3

pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

struct global_table{
	int sockid;
};
struct global_table record[20];
int counter = -1; // counter for table, used to know if empty

struct packet{
	short type;
	char data[MAX_LINE];
};

void *multicaster() {
	char *filename;
	char text[100];
	struct packet filedata;
	int fd;
	int send_sock;
	int file_chunk = 0;
	int seq_no = 0;
	int nread;

	filename = "multicasterinput.txt";

	// continuously send data
	while (1) {
		sleep(3);
		// if table not empty
		if(counter >= 0) {
			// open file, set nread to 1 since 0 bytes read means reached end of file
			fd = open(filename,O_RDONLY,0);
			file_chunk=0;
			nread = 1;
			// while there is more of the file to read
			while (nread > 0) {
				nread = read(fd, text, 100);
				// close file if reached end
				if(nread <= 0) {
					close(fd);
					break;
				}
				// copy those bytes read into the data packet
				strncpy(filedata.data, text, nread);
				// end string terminator
				filedata.data[nread] = 0;
				for (int j=0;j<=counter;j++) {
					// get socket id from table and send
					pthread_mutex_lock(&my_mutex);
					send_sock = record[j].sockid;
					pthread_mutex_unlock(&my_mutex);
					if(send(send_sock,&filedata,sizeof(filedata),0) < 0){
			                	printf("Data send failed\n");
				        }
				}
			}
		}
	}
}

void *leave_handler(struct packet *rec) {
	int newsock;
	int position;
	int send_sock;
	int find_sock = atoi(rec->data);

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

void *join_handler(struct global_table *rec) {
	int newsock;
	int req_no = 1;
	struct packet packet_reg;
	newsock = rec->sockid;

	// receive follow up registration packets
	for (int i=req_no; i < REQUEST_NO; i++) {
		if(recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0) {
			printf("Could not receive RG-%d\n", i+1);
			exit(1);
		}
		req_no++;
	}

	// add to table
	pthread_mutex_lock(&my_mutex);
	counter++;
	record[counter].sockid = newsock;
	pthread_mutex_unlock(&my_mutex);

	// send response acknowledging registration
	packet_reg.type = htons(221);
	sprintf(packet_reg.data, "%d", newsock);
	if(send(newsock, &packet_reg, sizeof(packet_reg), 0) < 0) {
		printf("ACK send failed\n");
		exit(1);
	}
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

	int sock_comm, new_s;
	int req_no;
	int len;
	int fd;
	int exit_value;
	pthread_t threads[3];

	struct global_table client_info;
	struct packet packet_recv;

	struct hostent *he;
	struct in_addr **addr_list;
	struct sockaddr_in sin;
	struct sockaddr_in client_addr;

	struct timeval tv;

	fd_set readfds;

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

	FD_ZERO(&readfds);
	FD_SET(sock_comm, &readfds);

	tv.tv_sec = 10;
	tv.tv_usec = 500000;

	printf("sock_comm: %d\n", sock_comm);
	printf("size: %d\n", FD_SETSIZE);
	// start multicaster
	pthread_create(&threads[2],NULL,multicaster,NULL);

	int n = sock_comm+1;

	printf("sock comm: %d\n", sock_comm);

        while (1) {
		int rv = select(n, &readfds, NULL, NULL, &tv);
		if (rv) {
			for (fd = 0; fd < n; fd++) {
				if (FD_ISSET(fd, &readfds)) {
					printf("Connection on %d\n", fd);
					if (fd == sock_comm) {
			        	        if ((new_s = accept(sock_comm, (struct sockaddr *)&client_addr, &len)) < 0) {
							printf("%d\n", new_s);
			                        	perror("tcpserver: accept");
							exit(1);
						}
						printf("Accepted\n");
						recv(new_s, &packet_recv, sizeof(packet_recv), 0);
						FD_SET(new_s, &readfds);
						printf("New connection: %d\n", new_s);
						n++;
						client_info.sockid = new_s;
						pthread_create(&threads[0],NULL,join_handler,&client_info);
						pthread_join(threads[0],&exit_value);
					}
					else if (fd) {
						recv(fd, &packet_recv, sizeof(packet_recv), 0);
						if (ntohs(packet_recv.type) == 221) {
							printf("hello\n");
							printf("%s\n", packet_recv.data);
							fflush(stdout);
						}
						else if (ntohs(packet_recv.type) == 321) {
							pthread_create(&threads[1],NULL,leave_handler,&packet_recv);
				                        pthread_join(threads[1],&exit_value);
						}
					}
				}
			}
		}
	}
	close(sock_comm);
}
