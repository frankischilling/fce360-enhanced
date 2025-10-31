#include "fceusupport.h"
#include "fceux/utils/crc32.h"
#include "fceux/utils/md5.h"
#include <xtl.h>

#ifndef socklen_t
#define socklen_t int
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

typedef struct hostent {
  char FAR      *h_name;
  char FAR  FAR **h_aliases;
  short         h_addrtype;
  short         h_length;
  char FAR  FAR **h_addr_list;
} HOSTENT, *PHOSTENT, FAR *LPHOSTENT;


int FCEUDnetplay=0;

static int s_Socket = -1;

static void en32(uint8 *buf, uint32 morp)
{
    buf[0] = morp;
    buf[1] = morp >> 8;
    buf[2] = morp >> 16;
    buf[3] = morp >> 24;
}

static uint32 de32(uint8 *morp)
{
	return(morp[0]|(morp[1]<<8)|(morp[2]<<16)|(morp[3]<<24));
}

// Netplay
int FCEUD_SendData(void *data, unsigned int len)
{
	int check = 0, error = 0;
    if(!error && check) {
        char buf[1024];
        char *f;
        fgets(buf, 1024, stdin);
        if((f=strrchr(buf,'\n'))) {
            *f = 0;
        }
        FCEUI_NetplayText((uint8 *)buf);
    }
    send(s_Socket, (char*)data, len ,0);
    return 1;
}

int FCEUD_RecvData(void *data, unsigned int len)
{
	int size;
    //NoWaiting &= ~2;

    for(;;) {
        fd_set funfun;
        struct timeval popeye;

        popeye.tv_sec=0;
        popeye.tv_usec=100000;

        FD_ZERO(&funfun);
        FD_SET(s_Socket, &funfun);

        switch(select(s_Socket + 1,&funfun,0,0,&popeye)) {
        case 0: continue;
        case -1:return(0);
        }

        if(FD_ISSET(s_Socket,&funfun)) {
            size = recv(s_Socket, (char*)data, len, 0);

            if(size == len) {
                //unsigned long beefie;

                FD_ZERO(&funfun);
                FD_SET(s_Socket, &funfun);

                popeye.tv_sec = popeye.tv_usec = 0;
                if(select(s_Socket + 1, &funfun, 0, 0, &popeye) == 1)
                    //if(!ioctl(s_Socket,FIONREAD,&beefie))
                    // if(beefie)
                    {
                        //NoWaiting|=2;
                        //puts("Yaya");
                    }
                return(1);
            } else {
                return(0);
            }
        }
    }

    return 0;
}

void FCEUD_NetworkClose(void)
{
	if(s_Socket > 0) {
        closesocket(s_Socket);
    }
    s_Socket = -1;

    if(FCEUDnetplay) {
        FCEUI_NetplayStop();
    }
    FCEUDnetplay = 0;
}

int
FCEUD_NetworkConnect(void)
{
    struct sockaddr_in sockin;
    struct hostent *phostentb;
    unsigned long hadr;
    int TSocket, tcpopt, error;
    int netdivisor;

    // get any required configuration variables
    int port, localPlayers;
    std::string server, username, password, key;
	server = "192.168.0.12";
	username = "xb";
	password = "";
	key = "";
	port = 4046;
	localPlayers = 2;

    // only initialize if remote server is specified
    if(!server.size()) {
        return 0;
    }

	XNetStartupParams xnsp;
	memset(&xnsp, 0, sizeof(xnsp));
	xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
	xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;

	xnsp.cfgSockDefaultRecvBufsizeInK = 16; // default = 16
	xnsp.cfgSockDefaultSendBufsizeInK = 16; // default = 16

	INT iResult = XNetStartup( &xnsp );

	WORD wVersionRequested;
	WSADATA wsaData;
	 
	wVersionRequested = MAKEWORD( 2, 2 );
	 
	WSAStartup( wVersionRequested, &wsaData );

    TSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(TSocket < 0) {
		DWORD err = GetLastError();

		char* s = "Error creating stream socket.";
        puts(s);
        FCEU_DispMessage(s,0);
        FCEUD_NetworkClose();
        return 0;
    }

    // try to setup TCP_NODELAY to avoid network jitters
    tcpopt = 1;
    error = setsockopt(TSocket, SOL_TCP, TCP_NODELAY, (char*)&tcpopt, sizeof(int));
    if(error) {
        puts("Nodelay fail");
    }

    memset(&sockin, 0, sizeof(sockin));
    sockin.sin_family = AF_INET;

    hadr = inet_addr(server.c_str());
    if(hadr != INADDR_NONE) {
        sockin.sin_addr.s_addr = hadr;
    } else {
#if 0 //marche pas
        puts("*** Looking up host name...");
        //phostentb = gethostbyname(server.c_str());

        if(!phostentb) {
            puts("Error getting host network information.");
            FCEU_DispMessage("Error getting host info",0);
            closesocket(TSocket);
            FCEUD_NetworkClose();
            return(0);
        }
        memcpy(&sockin.sin_addr, phostentb->h_addr, phostentb->h_length);
#endif
    }

    sockin.sin_port = htons(port);

    puts("*** Connecting to remote host...");
    error = connect(TSocket, (struct sockaddr *)&sockin, sizeof(sockin));
    if(error < 0) {
        puts("Error connecting to remote host.");
        FCEU_DispMessage("Error connecting to server",0);
        closesocket(TSocket);
        FCEUD_NetworkClose();
        return 0;
    }

    s_Socket = TSocket;

    puts("*** Sending initialization data to server...");
    uint8 *sendbuf;
    uint8 buf[5];
    uint32 sblen;

    sblen = 4 + 16 + 16 + 64 + 1 + username.size();
    sendbuf = (uint8 *)malloc(sblen);
    memset(sendbuf, 0, sblen);

    // XXX soules - should use htons instead of en32() from above!
    //uint32 data = htons(sblen - 4);
    //memcpy(sendbuf, &data, sizeof(data));
    en32(sendbuf, sblen - 4);

    if(key.size()) {
        struct md5_context md5;
        uint8 md5out[16];

        md5_starts(&md5);
        md5_update(&md5, (uint8*)&GameInfo->MD5.data, 16);
        md5_update(&md5, (uint8 *)key.c_str(), key.size());
        md5_finish(&md5, md5out);
        memcpy(sendbuf + 4, md5out, 16);
    } else {
        memcpy(sendbuf + 4, (uint8*)&GameInfo->MD5.data, 16);
    }

    if(password.size()) {
        struct md5_context md5;
        uint8 md5out[16];

        md5_starts(&md5);
        md5_update(&md5, (uint8 *)password.c_str(), password.size());
        md5_finish(&md5, md5out);
        memcpy(sendbuf + 4 + 16, md5out, 16);
    }

    memset(sendbuf + 4 + 16 + 16, 0, 64);

    sendbuf[4 + 16 + 16 + 64] = (uint8)localPlayers;

    if(username.size()) {
        memcpy(sendbuf + 4 + 16 + 16 + 64 + 1,
               username.c_str(), username.size());
    }

    send(s_Socket, (char*)sendbuf, sblen, 0);
    free(sendbuf);

    recv(s_Socket, (char*)buf, 1, 0);
    netdivisor = buf[0];

    puts("*** Connection established.");
    FCEU_DispMessage("Connection established.",0);

    FCEUDnetplay = 1;
    FCEUI_NetplayStart(localPlayers, netdivisor);

    return 1;
}



void FCEUD_NetworkOpen(void)
{
}