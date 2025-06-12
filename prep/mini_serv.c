#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct s_client {
	int fd;
	int gid;
	char *buffer;
} t_client;

t_client clients[1024];
fd_set read_set, active_set;
int max_fd = 0;
int next_id = 0;

void fatal() {
	write(2, "Fatal error\n", 12);
	exit(2);
}

void broadcast(int except, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &active_set) && fd != except && clients[fd].fd != -1) {
			send(fd, msg, strlen(msg), 0);
		}
	}
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
	free(s1);
	s1 = NULL;
	return res;
}

int extract_message(char **buf, char **msg) {
	if (!*buf)
		return 0;
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
		
int main(int ac, char **av) {
	if (ac != 2) {
		write(2, "Wrong number of arguemtns\n", 26);
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
	serveraddr.sin_port = htons(atoi(av[1]));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(2130706433);

	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd <= 0)
		fatal();
	FD_ZERO(&active_set);
	FD_SET(sock_fd, &active_set);
	max_fd = sock_fd;

	if (bind(sock_fd, (struct sockaddr*)&serveraddr, len) < 0)
		fatal();
	if (listen(sock_fd, 10) < 0)
		fatal();

	while (1) {
		read_set = active_set;

		if (select(max_fd + 1, &active_set, NULL, NULL, NULL) < 0)
			continue;
		
		for (int fd = 0; fd <= max_fd; fd++) {
			if (FD_ISSET(fd, &read_set)) {
				if (fd == sock_fd) {
					// new connection
					int client_fd = accept(sock_fd, (struct sockaddr*)&serveraddr, &len);
					if (client_fd < 0)
						continue;

					if (client_fd < max_fd)
						max_fd = client_fd;

					clients[client_fd].gid = next_id++;
					clients[client_fd].fd = client_fd;
					clients[client_fd].buffer = NULL;

					FD_SET(client_fd, &active_set);

					char msg[64];
					sprintf(msg, "server: client %d just arrived\n", clients[client_fd].gid);
					broadcast(client_fd, msg);

				} else {
					char recv_buffer[4096];
					int bytes = recv(fd, recv_buffer, 4095, 0);

					if (bytes == 0) {
						// disconnect
						close(fd);
						FD_CLR(fd, &active_set);

						char msg[64];
						sprintf(msg, "server: client %d just left\n", clients[fd].gid);
						broadcast(fd, msg);

						if (clients[fd].buffer)
							free(clients[fd].buffer);

						clients[fd].fd = -1;
						clients[fd].gid = -1;
						clients[fd].buffer = NULL;
						
					} else {
						// message
						clients[fd].buffer = str_join(clients[fd].buffer, recv_buffer);
						char *line = NULL;
						while (extract_message(&clients[fd].buffer, &line)) {
							char *formatted = malloc(strlen(line) + 20);
							sprintf(formatted, "client %d: %s", clients[fd].gid, line);
							broadcast(fd, formatted);
							free(formatted);
							free(line);
							line = NULL;
						}
					}
				}
			}
		}
		
	}

}

