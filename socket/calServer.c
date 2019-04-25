//server �����


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//��Ʈ��ũ
#include <arpa/inet.h> // ���ϻ��� �ʿ�
#include <sys/socket.h> 


//#define DEBUG


struct calcul
{
	int opnd_cnt; //�ǿ����� ����
	int opnds[4]; //�ǿ�����
	char oper; //������
};


//error �޽��� ����Լ�
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



	if (argc != 2) //2���� ���ڸ� ������
	{
		// �޾ƾ��� ����
		// ./helloClient [PORT_NUM]
		printf("Usage : %s <PORT>\n", argv[0]);
		exit(1);
	}

	//STEP 1. socket�� �����Ѵ�. file�� open�� ����

	// PF_INET : IP v4
	// SOCK_STREAM : TCP
	//IPPROTO_TCP : TCP ��� ������ �տ� SCOK_STREAM���� TCP�� ���������Ƿ� 0�� ��� �ȴ�
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");


	//STEP 2. ������ ������ IP�ּ� , ��Ʈ��ȣ, ���������� ����
	//struct sockaddr_in ���

	//����ü �ʱ�ȭ
	memset(&serv_addr, 0, sizeof(serv_addr));

	//ip v4 address family
	serv_addr.sin_family = AF_INET;

	//�ּ� ����
	//htonl (�ڱ��ڽ��� ip�ּҸ� �޾ƿ�)
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);


	//��Ʈ ��ȣ ����
	//���ڿ� ��Ʈ��ȣ�� ������ ��ȯ��
	//htons = host to network �� port��ȣ ��ȯ�ϴµ� short�� ��ȯ
	serv_addr.sin_port = htons(atoi(argv[1]));

	//STEP 3. BIND ���������� �̰����� �߰���
	//bind : �ڱ� �������� ip�� port�� set up �صδ� �۾��̴�

	if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error");



	//STEP 4. listen
	// serv_sock : ���� ������ fd
	// 5: ���(��������)�Ҽ� �ִ� client �� ���� buffer�� ũ�� (5���� Ŭ���̾�Ʈ�� ���Ӱ����Ѱ� �ƴ�)

	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	//clnt_addr �� ũ�� Ȯ��
	clnt_addr_size = sizeof(clnt_addr);
	clnt_sock = accept(serv_sock, (struct sockaddr *) & clnt_addr, &clnt_addr_size);

	if (clnt_sock == -1)
		error_handling("accept() error");


	//STEP 5. ������ ����
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


	//������ ������ ���� ���� ���bind()������ �߻��� �� �ִ�.
	//Ŭ���̾�Ʈ���� �ʰ� ������ �����ϱ� ���� sleep!
	sleep(2);

	//STEP 5. ���� ����
	close(clnt_sock);
	close(serv_sock);

	return 0;



}