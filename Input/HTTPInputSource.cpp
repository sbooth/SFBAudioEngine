//
//  HTTPInputSource.cpp
//  SFBAudioEngine-iOS
//
//  Created by Jason Swain on 14/07/2011.
//  Copyright 2011 Bookshelf Apps Limited. All rights reserved.
//

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include <log4cxx/logger.h>

#include "HTTPInputSource.h"

using namespace std;

const SInt16 kHeaderBufferLength = 2048;

int connect_with_timeout(int sockfd, struct sockaddr* addr, int sec) { 
	int res; 
	long arg; 
	fd_set myset; 
	struct timeval tv; 
	int valopt; 
	socklen_t lon; 
    
	// Set non-blocking 
	if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) { 
		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
		return -1;
	} 
	arg |= O_NONBLOCK; 
	if( fcntl(sockfd, F_SETFL, arg) < 0) { 
		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
		return -1;
	} 
    
	// Trying to connect with timeout 
	res = connect(sockfd, addr, sizeof(struct sockaddr)); 
	if (res < 0) { 
        if (errno == EINPROGRESS) { 

            do { 
                tv.tv_sec = sec; 
                tv.tv_usec = 0; 
                FD_ZERO(&myset); 
                FD_SET(sockfd, &myset); 
                res = select(sockfd+1, NULL, &myset, NULL, &tv); 
                if (res < 0 && errno != EINTR) { 
                    fprintf(stderr, "1 Error connecting %d - %s\n", errno, strerror(errno)); 
                    return -1; 
                } else if (res > 0) { 
                    // Socket selected for write 
                    lon = sizeof(int); 
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) { 
                        fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno)); 
                        return -1; 
                    } 
                    // Check the value returned... 
                    if (valopt) { 
                        fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt)); 
                        return -1; 
                    } 
                    break; 
                } else if( res == 0 ) { 
                    fprintf(stderr, "Timeout in select() - Cancelling!\n"); 
                    return -1;
                } 
            } while (1); 
        } else { 
            fprintf(stderr, "2 Error connecting %d - %s\n", errno, strerror(errno)); 
            return -1;
        } 
    } 

    // Set to blocking mode again... 
    if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) { 
        fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
        return -1; 
    } 
    arg &= (~O_NONBLOCK); 
    if( fcntl(sockfd, F_SETFL, arg) < 0) { 
        fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
        return -1; 
    } 
    
    return 0;
}

int socket_connect(const char* addr, const char* port) {
	
    int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
	
    if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
	
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
		
        if (connect_with_timeout(sockfd, p->ai_addr, 5) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
		
        break;
    }
	
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect to %s %s\n", addr, port);
        return 2;
    }
	
    // inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
	//		  s, sizeof s);
    // printf("client: connecting to %s\n", s);
	
    freeaddrinfo(servinfo); // all done with this structure
    
	return sockfd;
}

#pragma mark Creation and Destruction

HTTPInputSource::HTTPInputSource(CFURLRef url)
: InputSource(url), mSocket(0)
{
}

HTTPInputSource::~HTTPInputSource()
{
	if(IsOpen())
		Close();
}

bool HTTPInputSource::Open(CFErrorRef *error)
{
	if(IsOpen()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InputSource.HTTP");
		LOG4CXX_WARN(logger, "Open() called on an InputSource that is already open");
		return true;
	}

    UInt8 urlBuffer[2048];
    CFURLGetBytes(mURL, urlBuffer, 2048);
    
    CFRange hostRange = CFURLGetByteRangeForComponent(mURL, kCFURLComponentHost, NULL);
    char* host = (char *)calloc(hostRange.length + 1, 1);
    strncpy(host, (const char *)&urlBuffer[hostRange.location], hostRange.length);
    
    
    const char* port = "9000";
    // const SInt32 port = CFURLGetPortNumber(mURL);

    CFRange pathRange = CFURLGetByteRangeForComponent(mURL, kCFURLComponentPath, NULL);
    char* path = (char *)calloc(pathRange.length + 1, 1);
    strncpy(path, (const char *)&urlBuffer[pathRange.location], pathRange.length);
    
    // Create HTTP request
    string body = "GET ";
    body += path;
    body += " HTTP/1.1\x0D\x0A";
    body += "Host: ";
    body += host;
    body += ":";
    body += port;
    body += "\x0D\x0A";
    body += "Content-Length: 0\r\n\r\n";

    // Open Socket
    mSocket = socket_connect(host, port);

    // Send request
    int result = send(mSocket, body.c_str(), body.length(), 0);

    // Read and parse header
    mOffset = 0;
    mLength = 0;
    mHeaderBuffer = (char *)malloc(kHeaderBufferLength);
    if( result > 0 ) {
        int recved = recv(mSocket, mHeaderBuffer, kHeaderBufferLength, 0);
        mHeaderEnd = strnstr(mHeaderBuffer, "\r\n\r\n", recved);
        if( mHeaderEnd ) {
            mHeaderEnd += strlen("\r\n\r\n");
        } else {
            close(mSocket);
            return false;
        }
        
        char* contentLen = strnstr(mHeaderBuffer, "Content-Length: ", mHeaderEnd - mHeaderBuffer);
        if( !contentLen ) contentLen = strnstr(mHeaderBuffer, "CONTENT-LENGTH: ", mHeaderEnd - mHeaderBuffer);
        if( !contentLen ) contentLen = strnstr(mHeaderBuffer, "content-length: ", mHeaderEnd - mHeaderBuffer);
        if( !contentLen ) {
            close(mSocket);
            return false;
        }
        contentLen += strlen("content-length: ");
        sscanf(contentLen, "%ul", &mLength);
    }
    
	mIsOpen = true;
	return true;
}

bool HTTPInputSource::Close(CFErrorRef *error)
{
	if(!IsOpen()) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InputSource.HTTP");
		LOG4CXX_WARN(logger, "Close() called on an InputSource that hasn't been opened");
		return true;
	}

    close(mSocket);
    mSocket = 0;
    
	mIsOpen = false;
	return true;
}

SInt64 HTTPInputSource::Read(void *buffer, SInt64 byteCount)
{
	if(!IsOpen() || NULL == buffer)
		return -1;
    // Return the remaining header buffer;
    if( mHeaderBuffer ) {
        SInt64 remaining = kHeaderBufferLength - (mHeaderEnd - mHeaderBuffer);
        if( remaining > byteCount ) {
            memcpy(buffer, mHeaderEnd, byteCount);
            mHeaderEnd += byteCount;
            return byteCount;
        } else {
            memcpy(buffer, mHeaderEnd, remaining);
            free(mHeaderBuffer);
            mHeaderBuffer = 0;
            return remaining;
        }
    }
    
    // Set timeout
    struct timeval tv;			
    tv.tv_sec = (int)5;
    tv.tv_usec = 0;
    setsockopt (mSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof tv);
    
    int recved = recv(mSocket, buffer, byteCount, 0);
    
    if (recved < 0) {
		log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("org.sbooth.AudioEngine.InputSource.HTTP");
        if( errno == 35 ) {
            LOG4CXX_WARN(logger, "Socket timeout");
        } else {
            LOG4CXX_WARN(logger, "Socket error");
        }
        close(mSocket);
        return 0;
    }

    mOffset += recved;
    return recved;
}

bool HTTPInputSource::SeekToOffset(SInt64 offset)
{
    return false;
}
