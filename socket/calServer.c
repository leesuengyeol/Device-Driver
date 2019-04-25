//server 만들기


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//네트워크
#include <arpa/inet.h> // 소켓사용시 필요
#include <sys/socket.h> 


//#define DEBUG


struct calcul
{
	int opnd_cnt; //피연산자 갯수
	int opnds[4]; //피연산자
	char oper; //연산자
};


//error 메시지 출력함수
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int calculate(int opnum, int opnds[], char oper)
{
	int i,result=opnds[0];

	switch (oper)
	{
		case '+': for (i = 1; i < opnum; i++) 
						result += opnds[i];
				  break;
		case '-': for (i = 1; i < opnum ; i++)
						result -= opnds[i];
				  break;
		case '*': for (i = 1; i < opnum ; i++)
						result *= opnds[i];
				  break;
	}
	return result;
}

int main(int argc, char* argv[])
{
	int serv_sock;
	int clnt_sock;
	int str_len;
	int result;

	struct calcul cal;
	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_size;



	if (argc != 2) //2개의 인자를 받을것
	{
		// 받아야할 인자
		// ./helloClient [PORT_NUM]
		printf("Usage : %s <PORT>\n", argv[0]);
		exit(1);
	}

	//STEP 1. socket을 생성한다. file의 open과 동일

	// PF_INET : IP v4
	// SOCK_STREAM : TCP
	//IPPROTO_TCP : TCP 사용 하지만 앞에 SCOK_STREAM으로 TCP를 정의했으므로 0을 적어도 된다
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");


	//STEP 2. 접속할 서버의 IP주소 , 포트번호, 프로토콜을 정의
	//struct sockaddr_in 사용

	//구조체 초기화
	memset(&serv_addr, 0, sizeof(serv_addr));

	//ip v4 address family
	serv_addr.sin_family = AF_INET;

	//주소 지정
	//htonl (자기자신의 ip주소를 받아옴)
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);


	//포트 번호 지정
	//문자열 포트번호를 정수로 변환후
	//htons = host to network 로 port번호 변환하는데 short로 변환
	serv_addr.sin_port = htons(atoi(argv[1]));

	//STEP 3. BIND 서버에서는 이과정이 추가됨
	//bind : 자기 스스로의 ip와 port를 set up 해두는 작업이다

	if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error");



	//STEP 4. listen
	// serv_sock : 현재 서버의 fd
	// 5: 대기(동시접속)할수 있는 client 의 갯수 buffer의 크기 (5개의 클라이언트만 접속가능한게 아님)

	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	//clnt_addr 의 크기 확인
	clnt_addr_size = sizeof(clnt_addr);
	clnt_sock = accept(serv_sock, (struct sockaddr *) & clnt_addr, &clnt_addr_size);

	if (clnt_sock == -1)
		error_handling("accept() error");


	//STEP 5. 데이터 수신
	str_len = read(clnt_sock, &cal, sizeof(cal));
	if (str_len == -1)
		error_handling("read() error!");

#ifdef DEBUG
	printf("opnd_cnt:%d\n", cal.opnd_cnt);
	printf("opnd[0]:%d\n", cal.opnds[0]);
	printf("opnd[1]:%d\n", cal.opnds[1]);
	printf("opnd[2]:%d\n", cal.opnds[2]);
	printf("oper:%c\n", cal.oper);
#endif

	result = calculate(cal.opnd_cnt, cal.opnds, cal.oper);
	printf("result=%d\n", result);

	write(clnt_sock, &result, sizeof(result));


	//서버의 소켓을 먼저 닫을 경우bind()에러가 발생할 수 있다.
	//클라이언트보다 늦게 소켓을 종료하기 위해 sleep!
	sleep(2);

	//STEP 5. 소켓 종료
	close(clnt_sock);
	close(serv_sock);

	return 0;



}