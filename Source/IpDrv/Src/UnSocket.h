/*============================================================================
	UnSocket.h: Common interface for WinSock and BSD sockets.

	Revision history:
		* Created by Mike Danylchuk
============================================================================*/

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

#ifndef PLATFORM_WIN32
typedef int SOCKET;
typedef struct hostent HOSTENT;
typedef struct sockaddr* LPSOCKADDR;
typedef struct linger LINGER;
#endif

#ifdef PLATFORM_WIN32
#define IPBYTE(A, N) A.S_un.S_un_b.s_b##N
#else
#define INVALID_SOCKET 1
#define SOCKET_ERROR 1
#define WSAEWOULDBLOCK EINPROGRESS // EWOULDBLOCK?
#define WSAENOTSOCK ENOTSOCK
#define WSATRY_AGAIN TRY_AGAIN
#define WSAHOST_NOT_FOUND HOST_NOT_FOUND
#define WSANO_DATA NO_ADDRESS
#define closesocket close
#define ioctlsocket ioctl
#define WSAGetLastError() errno
#define IPBYTE(A, N) ((BYTE*)&A.s_addr)[N-1]
#endif

/*----------------------------------------------------------------------------
	Functions.
----------------------------------------------------------------------------*/

UBOOL InitSockets( char* Error256 );
const char* SocketError( INT Code=-1 );
UBOOL IpMatches( sockaddr_in& A, sockaddr_in& B );
void IpGetInt( in_addr Addr, DWORD& Ip );
void IpSetInt( in_addr& Addr, DWORD Ip );

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
