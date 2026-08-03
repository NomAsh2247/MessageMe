// MessageMe.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
/*
* 
* Needs to be compiled as such if windows.h is used:
* 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
*/

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <iostream>
#include <thread>

#define STATICBUFLEN 512

int startupWSA(WSADATA* wsaData, int lo=2, int hi=2)
{
    if (WSAStartup(MAKEWORD(lo, hi), wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
    if (LOBYTE(wsaData->wVersion) != lo ||
        HIBYTE(wsaData->wVersion) != hi)
    {
        fprintf(stderr, "Version %d.%d of Winsock not available.\n", lo, hi);
        WSACleanup();
        return 2;
    }

    return 0;
}

int populateAddrInfo(addrinfo*& serverInfo, PCSTR address, PCSTR port, const addrinfo& hints)
{
    int status = getaddrinfo(address, port, &hints, &serverInfo);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    return 0;
}

int populateAddrInfo(addrinfo*& serverInfo, PCSTR address, PCSTR port, int socktype, int flags = 0, int family = AF_UNSPEC)
{
    addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = socktype;
    hints.ai_flags = flags;
    hints.ai_family = family;

    return populateAddrInfo(serverInfo, address, port, hints);
}

int bindFromAddrInfo(SOCKET& sockfd, PCSTR port, int aisocktype = SOCK_DGRAM, PCSTR address = NULL, int aiflags = AI_PASSIVE, int aifamily = AF_UNSPEC)
{
    addrinfo *serverInfo = nullptr, *p;

    if (populateAddrInfo(serverInfo, address, port, aisocktype, aiflags, aifamily) != 0) {
        return 1;
    }

    for (p = serverInfo; p != NULL; p = p->ai_next)
    {
		// try to create a socket with the current addrinfo
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET) {
            std::cerr << "fail attempt: socket (" << WSAGetLastError() << ")\n";
            continue;
		}

		// try to bind the socket to the current addrinfo
        if (bind(sockfd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sockfd);
            std::cerr << "fail attempt: bind (" << WSAGetLastError() << ")\n";
            continue;
        }

		break; // if we get here, we have successfully bound the socket
    }

	// no longer need the serverInfo linked list
    freeaddrinfo(serverInfo);

    // fail if no binds found
    if (p == NULL) {
        fprintf(stderr, "failed to bind socket on %s:%s\n", address ? address : "AUTO_IP", port);
        return 2;
	}

	// success
	return 0;
}

int connectTCPAddrInfo(SOCKET& sockfd, PCSTR port, PCSTR address = NULL)
{
    const int aifamily = AF_UNSPEC;
    const int aisocktype = SOCK_STREAM;
	addrinfo *serverInfo = nullptr, *p;

    if (populateAddrInfo(serverInfo, address, port, aisocktype, 0, aifamily) != 0) {
        return 1;
    }

    for (p = serverInfo; p != NULL; p = p->ai_next)
    {
        // try to create a socket with the current addrinfo
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET) {
            std::cerr << "fail attempt: socket (" << WSAGetLastError() << ")\n";
            continue;
		}

		// try to connect the socket to the current addrinfo
        if (connect(sockfd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sockfd);
            std::cerr << "fail attempt: connect (" << WSAGetLastError() << ")\n";
            continue;
		}

		break; // if we get here, we have successfully connected the socket
    }

	// no longer need the serverInfo linked list
    freeaddrinfo(serverInfo);

    // fail if no connects found
    if (p == NULL) {
        fprintf(stderr, "failed to connect socket on %s:%s\n", address ? address : "AUTO_IP", port);
        return 2;
	}

	return 0;
}

int connectUDPAddrInfo(SOCKET& sockfd, PCSTR port, PCSTR address = NULL)
{
    const int aifamily = AF_UNSPEC;
    const int aisocktype = SOCK_DGRAM;
    addrinfo *serverInfo = nullptr, *p;

    if (populateAddrInfo(serverInfo, address, port, aisocktype, 0, aifamily) != 0) {
        return 1;
    }

    for (p = serverInfo; p != NULL; p = p->ai_next)
    {
        // try to create a socket with the current addrinfo
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET) {
            perror("fail attempt: socket");
            continue;
        }

        break;
    }

    // no longer need the serverInfo linked list
    freeaddrinfo(serverInfo);

    // fail if no connects found
    if (p == NULL) {
        fprintf(stderr, "failed to connect socket on %s:%s\n", address ? address : "AUTO_IP", port);
        return 2;
    }

    return 0;
}

int main()
{
    std::cout << "Server (0) / Client (1):\n";

    int option = -1, host[2], status;
    std::cin >> option;

    WSADATA wsaData;

	startupWSA(&wsaData);

    std::cout << "WSA init succcess\n";

    SOCKET mysock;
    const PCSTR myport[2] = {"1888", "1889"};

    if ((status = bindFromAddrInfo(mysock, myport[option], SOCK_DGRAM, "127.0.0.1")) != 0) {
        std::cout << "Failed (" << status << ") to bind on 127.0.0.1:" << myport[option] << std::endl;
        WSACleanup();
        return 1;
    }

    std::cout << "Bind success.\n";

	sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

	sockaddr_in server_addr;
	socklen_t server_addr_len = sizeof(server_addr);

	char buf[STATICBUFLEN];
	int numbytes;

    if (option == 0) {
        // server
		std::cout << "Waiting for message...\n";

        if ((numbytes = recvfrom(mysock, buf, STATICBUFLEN - 1, 0, (sockaddr*)&client_addr, &client_addr_len)) == SOCKET_ERROR) {
            perror("recvfrom");
            closesocket(mysock);
            WSACleanup();
            return 1;
        }

		buf[numbytes] = '\0';

		std::cout << "Received:\n" << buf << "\nEnter message to send back:\n";

        std::cin.ignore(); // ignore the newline character left in the input buffer
        std::cin.getline(buf, STATICBUFLEN);
        if ((numbytes = sendto(mysock, buf, (int)strlen(buf), 0, (sockaddr*)&client_addr, client_addr_len)) == SOCKET_ERROR) {
            std::cerr << "fail: sendto (" << WSAGetLastError() << ")\n";
            closesocket(mysock);
            WSACleanup();
            return 1;
		}

		std::cout << "Message sent back to client.\n";
    }
    else {
		// client
        std::cout << "Enter message to send:\n";
        std::cin.ignore(); // ignore the newline character left in the input buffer
        std::cin.getline(buf, STATICBUFLEN);

		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(atoi(myport[option * -1 + 1]));
		inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if ((numbytes = sendto(mysock, buf, (int)strlen(buf), 0, (sockaddr*)&server_addr, server_addr_len)) == SOCKET_ERROR) {
            std::cerr << "fail: sendto (" << WSAGetLastError() << ")\n";
            closesocket(mysock);
            WSACleanup();
            return 1;
        }

		std::cout << "Message sent to server. Waiting for response...\n";

        if ((numbytes = recvfrom(mysock, buf, STATICBUFLEN - 1, 0, (sockaddr*)&server_addr, &server_addr_len)) == SOCKET_ERROR) {
            std::cerr << "fail: recvfrom (" << WSAGetLastError() << ")\n";
            closesocket(mysock);
            WSACleanup();
            return 1;
		}

		buf[numbytes] = '\0';

		std::cout << "Received from server:\n" << buf << std::endl;
    }

	closesocket(mysock);
    WSACleanup();
    return 0;
}