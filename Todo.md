init
- socket init, bind
- make session pool

start IO threads

start Accept (block)

finalize

1.  비동기 네트워크 입출력 및 다수의 클라이언트 세션 처리
Win32 API의 Overlapped I/O를 이용

2. TCP 스트림을 다루는 방법 및 TCP 스트림을 통한 패킷 시리얼라이징

 Circular Byte Buffer를 구성하고 다루는 법, NAGLE (TCP_NODELAY) 컨트롤

3. cocos2d-x 게임 클라이언트에서의 네트워킹 방법 (send/recv 및 TCP 스트림 핸들링)

SQLite3을 이용한 Database 처리 방법, C++상에서의 데이터베이스 제어, Half-sync Half-async 패턴을 이용한 데이터베이스 처리 전용 스레드의 사용법

Windows APC(Async Procedure Call) 큐의 개념 이해와 더불어 Windows의 각종 동기화 객체들 사용법

Waitable Timer를 이용한 게임 타이머 구성 방법
Producer Consumer Queue를 condition variable을 이용하여 구현하는 방법
Event 및 SRWLock 등의 사용법
서버에서의 예외 상황 발생시 minidump 생성 방법

Object Pooling을 통한 메모리 할당/해제 성능 향상 방법

Thread local storage의 개념

std::bind를 응용한 Task 스케줄링 방법

참조 카운팅 기법을 활용한 객체의 생명 주기 관리 방법

Dummy 클라이언트(Boost.Asio 사용)를 이용한 시나리오 기반 서버 부하 테스트 방법

기대했을 수 있지만, 여기에서 배울 수 없는 것
multi-thread 기반의 IOCP를 이용한 서버 만들기 방법
고성능 서버 제작을 위한 각종 튜닝 기법들
암호화, 해킹방어 등과 같은 각종 보안 기법들
게임 콘텐츠에 관련된 각종 구현 방법들


# 참고

1. https://github.com/jacking75/fixme_degiyamIOCP

코드를 수정해야 하는 ICOP 라이브러리

https://github.com/jacking75/fixme_MyFirstGameServer
2. 

중국 서버
https://github.com/jacking75/handy


Half-Sync/Half-Async 패턴과 Leader/Followers 패턴

네트워크 처리는 Async 방식으로 데이터의 처리(로직 스레드)는 Sync 방식으로 처리하는 방식. Async Layer와 Sync Layer와의 통신을 위해 Message Queue가 필요.
http://egloos.zum.com/javawork/v/1818696

https://github.com/jacking75/codes_book_onlinegameserver
온라인 게임 서버 (지은이 강정중) 책의 예제 코드를 리팩토링