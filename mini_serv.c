#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct s_client {
	int fd;
	int gid;
	char *buffer;
} t_client;

t_client clients[1024];
int max_fd = 0; 
int next_id = 0;
fd_set read_set, write_set;

void broadcast(int except, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &write_set) && fd != except && clients[fd].fd != -1) {
			int total_len = strlen(msg);
			int sent = 0;
			
			while (sent < total_len) {
				int ret = send(fd, msg + sent, total_len - sent, 0);
				if (ret <= 0) {
					break;
				}
				sent += ret;
			}
		}
	}
}

void fatal() {
	write(2, "Fatal error\n", 12);
	exit(2);
}

char *str_join(char *s1, char *s2) {
	if (!s1) {
		s1 = calloc(1,1);
		if (!s1)
			fatal();
	}
	int len = strlen(s1) + strlen(s2);
	char *res = malloc(len + 1);
	strcpy(res, s1);
	strcpy(res + strlen(s1), s2);
	res[len] = '\0';
	printf("str_join %ld + %ld = %ld\n", strlen(s1), strlen(s2), strlen(res)); // debug
	free(s1);
	s1 = NULL;
	return res;
}

int extract_message(char **buf, char **msg) {
    if (!*buf)
        return 0;
    
	printf("extract_message *buf: %ld\n", strlen(*buf)); // debug
    int i = 0;
    while ((*buf)[i]) {
        if ((*buf)[i] == '\n') {
            int len = i + 1;
            char *line = malloc(len + 1);
            for (int j = 0; j < len; j++) {
                line[j] = (*buf)[j];
            }
            line[len] = '\0';
            *msg = line;

            int len_buf = strlen(*buf) - len;
            char *new_buf = malloc(len_buf + 1);
            strcpy(new_buf, *buf + len);
            free(*buf);
            *buf = new_buf;

            return 1;
        }
        i++;
    }
    return 0;
}

int main(int ac, char** av) {
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	for (int i = 0; i < 1024; i++) {
		clients[i].fd = -1;
		clients[i].gid = -1;
		clients[i].buffer = NULL;
	}

	struct sockaddr_in serveraddr;
	socklen_t len = sizeof(serveraddr);
	memset(&serveraddr, 0, len);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(av[1]));
	serveraddr.sin_addr.s_addr = htonl(2130706433);

	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd <= 0) 
		fatal();
	max_fd = sock_fd;

	if (bind(sock_fd, (struct sockaddr *)&serveraddr, len) < 0)
		fatal();
	if (listen(sock_fd, 10) < 0)
		fatal();

	FD_ZERO(&write_set);
	FD_SET(sock_fd, &write_set);

	while(1) {
		read_set = write_set;

		if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0)
			continue;

		for (int i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &read_set)) {
				if (i == sock_fd) {
					// new connection
					int client_fd = accept(sock_fd, (struct sockaddr*)&serveraddr, &len);
					if (client_fd < 0)
						continue;
					printf("new connection fd: %d\n", client_fd);

					clients[client_fd].fd = client_fd;
					clients[client_fd].gid = next_id++;
					clients[client_fd].buffer = NULL;

					char msg[64];
					sprintf(msg, "server: client %d just arrived\n", clients[client_fd].gid);
					broadcast(client_fd, msg);

					if (max_fd < client_fd)
						max_fd = client_fd;

					FD_SET(client_fd, &write_set);

				} else {
					char recv_buffer[4096]; // 4096 only for debug
					memset(recv_buffer, 0, 4096); // to make sure i can copy the entire recv_buffer even if bytes does not correctly return
					int bytes = recv(i, recv_buffer, 4095, 0); // 4095
					printf("fd: %d, recv bytes: %ld\n", i, strlen(recv_buffer));

					if (bytes <= 0) {
						// disconnect
						char msg[64];
						sprintf(msg, "server: client %d just left\n", clients[i].gid);
						broadcast(clients[i].fd, msg);
						
						FD_CLR(clients[i].fd, &write_set);
						close(i);
						if (clients[i].buffer)
							free(clients[i].buffer);

						clients[i].fd = -1;
						clients[i].gid = -1;
						clients[i].buffer = NULL;

					} else {
						//recv_buffer[bytes] = '\0'; //because of previous memset
						clients[i].buffer = str_join(clients[i].buffer, recv_buffer);
						printf("in message loop fd: %d, strlen %ld\n", i, strlen(clients[i].buffer)); // debug

						char *line = NULL;
						while(extract_message(&clients[i].buffer, &line)) {
							char *formatted = malloc(strlen(line) + 20);
							sprintf(formatted, "client %d: %s", clients[i].gid, line);
							broadcast(i, formatted);
							printf("message send fd: %d\n", i); // debug

							free(formatted);
							free(line);
							line = NULL;
						}
					}
				}
			}
		}
	}

	return 0;
}

