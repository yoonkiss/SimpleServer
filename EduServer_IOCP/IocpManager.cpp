#include "stdafx.h"
#include "Exception.h"
#include "IocpManager.h"
#include "EduServer_IOCP.h"
#include "ClientSession.h"
#include "SessionManager.h"

#define GQCS_TIMEOUT	INFINITE //20

__declspec(thread) int LIoThreadId = 0;
IocpManager* GIocpManager = nullptr;

LPFN_DISCONNECTEX IocpManager::mFnDisconnectEx = nullptr;
LPFN_ACCEPTEX IocpManager::mFnAcceptEx = nullptr;

char IocpManager::mAcceptBuf[64] = { 0, };

//TODO AcceptEx DisconnectEx 함수 사용할 수 있도록 구현.

BOOL DisconnectEx(SOCKET hSocket, LPOVERLAPPED lpOverlapped, DWORD dwFlags, DWORD reserved)
{
	//return ...
	return 0;
}

/* 참고: 최신 버전의 Windows SDK에서는 그냥 구현되어 있음
BOOL AcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
	DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
{
	return 0;
}
*/
IocpManager::IocpManager() : mCompletionPort(NULL), mIoThreadCount(2), mListenSocket(NULL)
{	
}


IocpManager::~IocpManager()
{
}

bool IocpManager::Initialize()
{
	/// set num of I/O threads
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	mIoThreadCount = si.dwNumberOfProcessors;

	/// winsock initializing
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	/// Create I/O Completion Port
	mCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (mCompletionPort == NULL)
		return false;

	/// create TCP socket
	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (mListenSocket == INVALID_SOCKET)
		return false;

    // IO 장치와 IOCP 연결
    // 내부적으로 IOCP와 연결된 여러 장치들을 관리하기 위한 장치 리스트에 새로운 레코드 추가됨
    // 삭제는 해당 장치의 핸들이 닫혔을 때
	HANDLE handle = CreateIoCompletionPort((HANDLE)mListenSocket, mCompletionPort, 0, 0);
	if (handle != mCompletionPort)
	{
		printf_s("[DEBUG] listen socket IOCP register error: %d\n", GetLastError());
		return false;
	}

	int opt = 1;
	setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));

	/// bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(LISTEN_PORT);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind(mListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr)))
		return false;

	// TODO : WSAIoctl을 이용하여 AcceptEx, DisconnectEx 함수 사용가능하도록 하는 작업..
    GUID guidDisconnectEx = WSAID_DISCONNECTEX;
    DWORD bytes = 0;
    if (SOCKET_ERROR == WSAIoctl(mListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidDisconnectEx, sizeof(GUID), &mFnDisconnectEx, sizeof(LPFN_DISCONNECTEX), &bytes, NULL, NULL))
        return false;

    GUID guidAcceptEx = WSAID_ACCEPTEX;
    if (SOCKET_ERROR == WSAIoctl(mListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx, sizeof(GUID), &mFnAcceptEx, sizeof(LPFN_ACCEPTEX), &bytes, NULL, NULL))
        return false;


	/// make session pool
	GSessionManager->PrepareSessions();

	return true;
}


bool IocpManager::StartIoThreads()
{
	/// I/O Thread
    // 장치와 IOCP 연결 후 IO 처리 할 풀을 생성. 프로세서 개수의 2배 정도 할당
    // 각 worker thread는 IO Completion Queue (IOCP와 연결한 장치의 IO작업이 끝났음을 알려주는 큐)에서 작업거리를 꺼내 처리 수행
	for (int i = 0; i < mIoThreadCount; ++i)
	{
		DWORD dwThreadId;
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, IoWorkerThread, (LPVOID)(i+1), 0, (unsigned int*)&dwThreadId);
		if (hThread == NULL)
			return false;
	}

	return true;
}


void IocpManager::StartAccept()
{
	/// listen
	if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN))
	{
		printf_s("[DEBUG] listen error\n");
		return;
	}
		
	while (GSessionManager->AcceptSessions())
	{
		Sleep(100);
	}

}


void IocpManager::Finalize()
{
	CloseHandle(mCompletionPort);

	/// winsock finalizing
	WSACleanup();

}

// IO Completion Queue
// wating thread queue: GetQueuedCompletionStatus 호출로 wait상태 진입
// Release Thread list
// Pause Thread list
// 처음엔 wating queue에 생성한 만큼 스레드 진입, IOCQ 에 2개의  IO작업이 완료되었다면 waiting queue에서 2개 꺼내 release thread에 LIFO순으로 넣음.
// thread가 release 상태일 때 내부에서 어떤 함수 호출 시 Pause list에 들어간다.
unsigned int WINAPI IocpManager::IoWorkerThread(LPVOID lpParam)
{
	LThreadType = THREAD_IO_WORKER;

	LIoThreadId = reinterpret_cast<int>(lpParam);
	HANDLE hComletionPort = GIocpManager->GetComletionPort();

	while (true)
	{
		DWORD dwTransferred = 0;
		OverlappedIOContext* context = nullptr;
		ULONG_PTR completionKey = 0;
        
        // 지정된 IOCP 에서 IO 완료 패킷을 dequeue(enqueue 삽입, dequeue 삭제) 를 시도한다.
        // dequeue 할 완료 패킷이 존재하지 않으면 완료 패킷이 발생할 때까지 대기한다.

		int ret = GetQueuedCompletionStatus(hComletionPort, &dwTransferred, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&context, GQCS_TIMEOUT);

		ClientSession* theClient = context ? context->mSessionObject : nullptr ;
		
		if (ret == 0 || dwTransferred == 0)
		{
			int gle = GetLastError();

			//TODO: check time out first ... GQCS 타임 아웃의 경우는 어떻게?
			
		
			if (context->mIoType == IO_RECV || context->mIoType == IO_SEND )
			{
				CRASH_ASSERT(nullptr != theClient);
			
				theClient->DisconnectRequest(DR_COMPLETION_ERROR);

				DeleteIoContext(context);

				continue;
			}
		}

		CRASH_ASSERT(nullptr != theClient);
	
		bool completionOk = false;
		switch (context->mIoType)
		{
		case IO_DISCONNECT:
			theClient->DisconnectCompletion(static_cast<OverlappedDisconnectContext*>(context)->mDisconnectReason);
			completionOk = true;
			break;

		case IO_ACCEPT:
			theClient->AcceptCompletion();
			completionOk = true;
			break;

		case IO_RECV_ZERO:
			completionOk = PreReceiveCompletion(theClient, static_cast<OverlappedPreRecvContext*>(context), dwTransferred);
			break;

		case IO_SEND:
			completionOk = SendCompletion(theClient, static_cast<OverlappedSendContext*>(context), dwTransferred);
			break;

		case IO_RECV:
			completionOk = ReceiveCompletion(theClient, static_cast<OverlappedRecvContext*>(context), dwTransferred);
			break;

		default:
			printf_s("Unknown I/O Type: %d\n", context->mIoType);
			CRASH_ASSERT(false);
			break;
		}

		if ( !completionOk )
		{
			/// connection closing
			theClient->DisconnectRequest(DR_IO_REQUEST_ERROR);
		}

		DeleteIoContext(context);
	}

	return 0;
}

bool IocpManager::PreReceiveCompletion(ClientSession* client, OverlappedPreRecvContext* context, DWORD dwTransferred)
{
	/// real receive...
	return client->PreRecv();
}

bool IocpManager::ReceiveCompletion(ClientSession* client, OverlappedRecvContext* context, DWORD dwTransferred)
{
	client->RecvCompletion(dwTransferred);

	/// echo back
	return client->PostSend();
}

bool IocpManager::SendCompletion(ClientSession* client, OverlappedSendContext* context, DWORD dwTransferred)
{
	client->SendCompletion(dwTransferred);

	if (context->mWsaBuf.len != dwTransferred)
	{
		printf_s("Partial SendCompletion requested [%d], sent [%d]\n", context->mWsaBuf.len, dwTransferred) ;
		return false;
	}
	
	/// zero receive
	return client->PreRecv();
}


