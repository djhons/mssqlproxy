/*
* Copyright (c) 2022 BlackArrow
*
* Author:
*  DJ (https://djhons.com)
*
*/

#pragma comment(lib,"Ws2_32.lib")
#pragma warning(disable : 4996)

#include "stdafx.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>​


#define MSG_EXIT_ACK "\x65\x43\x21"
#define MSG_END_OF_TRANSMISSION "\x31\x41\x59\x26\x53\x58\x97\x93\x23\x84"

#define BUFSIZE 65536
#define IPSIZE 4
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

enum socks {
	RESERVED = 0x00,
	VERSION = 0x05
};

enum socks_auth_methods {
	NOAUTH = 0x00,
	USERPASS = 0x02,
	NOMETHOD = 0xff
};
enum socks_auth_userpass {
	AUTH_OK = 0x00,
	AUTH_VERSION = 0x01,
	AUTH_FAIL = 0xff
};
enum socks_command {
	CONNECT = 0x01
};
enum socks_command_type {
	IP = 0x01,
	DOMAIN = 0x03,
	IPV6 = 0x04
};
enum socks_status {
	OK = 0x00,
	FAILED = 0x05
};



// Receive n bytes
int readn(SOCKET fd, void* buf, int n)
{
	int nread, left = n;
	while (left > 0) {
		if ((nread = recv(fd, (char*)buf, left, 0)) == 0) {
			return 0;
		}
		else if (nread != -1) {
			left -= nread;
			buf = (char*)buf + nread;
		}
	}
	return n;
}

// Receive client negotiation
int socks5_recv_nego(SOCKET fd) {

	while (1) {

		char tmp[7] = { 0 };
		char init[3] = { 0 };

		readn(fd, init, ARRAY_SIZE(init));

		// Ok: version 5
		if (init[0] == VERSION && init[1] == 1 && init[2] == NOAUTH)
		{
			return 0;
		}
		// Exit command
		else if (init[0] == 0x12 && init[1] == 0x34 && init[2] == 0x56) {
			return -1;
		}
		// End-of-transmission mark
		else if (init[0] == 0x31 && init[1] == 0x41 && init[2] == 0x59) { // Mark: "\x31\x41\x59\x26\x53\x58\x97\x93\x23\x84" (10 bytes)
			readn(fd, tmp, ARRAY_SIZE(tmp));
			if (strstr(tmp, "\x26\x53\x58\x97\x93\x23\x84") != NULL) {
				continue;
			}
		}

		// Unknown
		return -1;
	}
}

// Send selected method (version 5, no auth)
void socks5_send_method(SOCKET fd) {
	char answer[2] = { VERSION, NOAUTH };
	send(fd, answer, ARRAY_SIZE(answer), 0);
}

// Recv first 4 bytes of request to check if address is an IP
int socks5_recv_addr_type(SOCKET fd)
{
	char data[4];
	readn(fd, data, ARRAY_SIZE(data));

	if (data[0] == VERSION && data[1] == CONNECT && data[2] == RESERVED)
		return data[3];

	return 0;
}

// Recv IPv4 address
char* socks5_recv_ip(SOCKET fd)
{
	char* ip = (char*)malloc(sizeof(char) * IPSIZE);
	readn(fd, ip, IPSIZE);
	return ip;
}

// Recv dest port
unsigned short int socks5_read_port(SOCKET fd)
{
	unsigned short int p;
	readn(fd, &p, sizeof(p));
	return p;
}

// Connect to target and return client socket
SOCKET app_connect(void* buf, unsigned short int portnum, SOCKET orig) {
	char address[16];
	struct sockaddr_in remote;
	SOCKET new_fd = INVALID_SOCKET;

	new_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	char* ip = NULL;
	ip = (char*)buf;

	memset(address, 0, ARRAY_SIZE(address));
	snprintf(address, ARRAY_SIZE(address), "%hhu.%hhu.%hhu.%hhu", ip[0], ip[1], ip[2], ip[3]);

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	InetPtonA(AF_INET, address, &remote.sin_addr.s_addr);
	remote.sin_port = htons(portnum);

	if (connect(new_fd, (struct sockaddr*) & remote, sizeof(remote)) == SOCKET_ERROR) {
		return -1;
	}
	return new_fd;

}

// Request reply
void socks5_ip_send_response(SOCKET fd, char* ip, unsigned short int port)
{
	char response[4] = { VERSION, OK, RESERVED, IP };

	send(fd, response, ARRAY_SIZE(response), 0);
	send(fd, ip, IPSIZE, 0);
	send(fd, (char*)&port, sizeof(port), 0);
}

// Comm with target
void app_socket_pipe(SOCKET fd0, SOCKET fd1)
{
	int maxfd, ret;
	fd_set rd_set;
	size_t nread;
	char buffer_r[BUFSIZE];
	maxfd = (fd0 > fd1) ? fd0 : fd1;
	while (1) {
		FD_ZERO(&rd_set);
		FD_SET(fd0, &rd_set);
		FD_SET(fd1, &rd_set);
		ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);
		if (ret < 0 && errno == EINTR) {
			continue;
		}
		if (FD_ISSET(fd0, &rd_set)) {
			nread = recv(fd0, buffer_r, BUFSIZE, 0);
			if (nread <= 0)
				break;
			send(fd1, buffer_r, nread, 0);
		}
		if (FD_ISSET(fd1, &rd_set)) {
			nread = recv(fd1, buffer_r, BUFSIZE, 0);

			if (nread <= 0)
				break;

			// End of transmission
			if (nread >= strlen(MSG_END_OF_TRANSMISSION) && strstr(buffer_r, MSG_END_OF_TRANSMISSION) != NULL) {
				send(fd0, buffer_r, nread - strlen(MSG_END_OF_TRANSMISSION), 0);
				break;
			}

			send(fd0, buffer_r, nread, 0);
		}
	}
}

int worker(SOCKET fd) {
	SOCKET inet_fd;
	int address_type = 0;

	unsigned short int p = 0;

	int res = socks5_recv_nego(fd);
	if (res != 0) {
		return res;
	}

	socks5_send_method(fd);

	address_type = socks5_recv_addr_type(fd);

	if (address_type == IP) {
		char* ip = NULL;
		ip = socks5_recv_ip(fd);

		p = socks5_read_port(fd);
		inet_fd = app_connect((void*)ip, ntohs(p), fd);

		if (inet_fd != -1) {
			socks5_ip_send_response(fd, ip, p);
			free(ip);

			app_socket_pipe(inet_fd, fd);
			closesocket(inet_fd);
		}
	}
	else if (address_type == IPV6)
	{
		// Consume 18 remaining bytes...
		char tmp[18];
		readn(fd, tmp, 18);

		char error_msg[] = { 0x05, 0x08, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x22, 0x22 };
		send(fd, error_msg, ARRAY_SIZE(error_msg), NULL);
	}

	return 0;
}


// Requests dispatcher
int proxy(SOCKET socks) {

	// Initialization
	int sendsuccess = send(socks, "Powered by blackarrow.net\n", strlen("Powered by blackarrow.net\n") + 1, 0);
	if (sendsuccess<0) {
		return sendsuccess;
	}

	// Main loop
	while (worker(socks) != -1);

	// Exit ACK
	send(socks, MSG_EXIT_ACK, 3, 0);
	return 0;
}



extern "C" __declspec(dllexport) int main(char *client_addr, int client_port) {
	WSADATA wsaData;
	int i;
	int len;
	struct sockaddr_in sockaddr;
	char ipbuf[INET_ADDRSTRLEN];

	WSAStartup(MAKEWORD(2, 0), &wsaData);

	int max_socket = 65536;
	for (i = 1; i < max_socket; i++) {
		len = sizeof(sockaddr);

		// Check if it is a socket
		if (getpeername((SOCKET)i, (SOCKADDR*)&sockaddr, &len) == 0) {
			if (strcmp(inet_ntoa(sockaddr.sin_addr), client_addr) == 0 && (client_port == 0 || sockaddr.sin_port == htons(client_port))) {
				SOCKET dup = INVALID_SOCKET;
				WSAPROTOCOL_INFO sockstate;
				if (WSADuplicateSocket(i, GetCurrentProcessId(), &sockstate) == 0)
				{
					dup = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &sockstate, 0, 0);
				}

				if (dup == INVALID_SOCKET) {
					return -1; // exit :(
				}

				// Close original socket and use the duplicated one
				closesocket(i);

				int reproxy=proxy(dup);
				if (reproxy == 0) {
					closesocket(dup);
					break;
				}
			}
		}
	}
	return 0;
}



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	default:
		break;
	}
	return TRUE;
}
