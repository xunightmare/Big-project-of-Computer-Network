// 合并.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <winsock2.h>

#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll

char nowtime[100] = ""; //全局变量，保存当前时间
char filename[100] = ""; //日志文件的名字
char* response[] = {
	"220 beta OK\r\n",
	"250 OK\r\n",
	"250 Mail OK\r\n",
	"354 End data with <CR><LF>.<CR><LF>\r\n",
	"QUIT\r\n",
	"DATA\r\n",//5
	"\r\n.\r\n",
	"554 5.5.1 Error: no valid recipients\r\n",
	"RSET\r\n",

	//后面的没有用到，暂时先不删
	"221 Bye\r\n",
	"502 Error: command not implemented\r\n",
	"334 dXNlcm5hbWU6\r\n",
	"334 UGFzc3dvcmQ6\r\n",
	"235 Authentication successful\r\n"
};
char message[10][10000];//应该对它进行初始化
//message[0] 用来储存helo命令
//message[1] 用来储存mail from命令
//message[3] 用来储存邮件内容
int num;//用来记录有多少个收件人
char recvname[10][100]; //用来储存收件人的信息
int divnum; //用来记录消息被分开的次数


void initNowtime();   //初始化当前时间
void writeLog(char * str);   //编写日志文件
void Sends(SOCKET Sock, char* str);  //向对方发送消息
char* Recvs(SOCKET Sock);//接收消息
int main_client(int i, SOCKET returnSock);  //和收件人服务器通信的程序  //i记录的是第几个收件人
                                            //returnsocket是发生错误时返回错误信息的socket
void printIP(SOCKET socket); //打印给定套接字的远程和本地ip、port
int Checkmailaddr(char email[101]); //检查邮箱地址格式是否正确
int returnError(char * temp1, SOCKET returnSock); //返回服务器的错误信息给客户端
                                 //temp1是要返回的错误信息 //returnSock是返回错误信息的地址

int main() {
	//记录收件人的个数
	printf("请输入收件人的个数：");
	scanf("%d", &num);

	//初始化日志文件
	FILE *fp;
	initNowtime();
	sprintf(filename, "log-%s.txt", nowtime);
	if ((fp = fopen(filename, "w")) == NULL) {
		printf("fopen() error.\n");
		perror("fopen");
		exit(1);
	}
	fprintf(fp, "%s\n", nowtime);
	fclose(fp);

	char request[MAXBYTE];

	//初始化 DLL
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//创建套接字
	SOCKET servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//绑定套接字
	sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
	sockAddr.sin_family = PF_INET;  //使用IPv4地址
	//sockAddr.sin_addr.s_addr = inet_addr("10.122.212.26");  //具体的IP地址 //应该填服务器端ip
	sockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
	sockAddr.sin_port = htons(25);  //端口
	bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
	//进入监听状态
	listen(servSock, 5);
	printf("服务已启动，正在监听......\n");
	SOCKADDR clntAddr;
	int nSize = sizeof(SOCKADDR);
	SOCKET clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
	printf("连接已经建立\n");

	//打印客户端和程序通信的ip和端口号
	//printf("%d\n", ntohs(sockAddr.sin_port));
	printIP(clntSock);

	Sends(clntSock, response[0]);   //返回 220 beta OK\r\n

	char*temp = Recvs(clntSock);    //接收到 helo 请求
	strcpy(message[0], temp);

	Sends(clntSock, response[1]);  //返回 250 OK\r\n

	temp = Recvs(clntSock); //接收到 MAIL FROM: 请求
	strcpy(message[1], temp);

	Sends(clntSock, response[2]); //返回 250 Mail OK\r\n

	for (int i = 0; i < num; i++) {
		temp = Recvs(clntSock); //接收到 RCPT TO: 请求
		strcpy(recvname[i], temp);
		char test[100]="";
		strncpy(test, temp+10, strlen(temp) - 13);
		if (Checkmailaddr(test) == -1) {
			Sends(clntSock, response[7]);
			printf("收件人地址错误");
			return -1;
		}
		else {
			Sends(clntSock, response[2]); //返回 250 Mail OK\r\n
		}

	}

	Recvs(clntSock); //接收到 DATA 请求

	Sends(clntSock, response[3]); //返回 End data with <CR><LF>.<CR><LF>\r\n

	divnum = 0; //初始化divnum
	//初始化email文件
	FILE *femail;
	if ((femail = fopen("email.txt", "w")) == NULL) {
		printf("fopen() error.\n");
		perror("fopen");
		exit(1);
	}

	do {
		temp = Recvs(clntSock); //接收到 请求
		fprintf(femail, "%s", temp);
		strcpy(message[3 + divnum], temp);
		divnum++;
	} while (*(temp + strlen(temp) - 1) != '\n' || *(temp + strlen(temp) - 2) != '\r' ||
		*(temp + strlen(temp) - 3) != '.' || *(temp + strlen(temp) - 4) != '\n' ||
		*(temp + strlen(temp) - 5) != '\r'); //如果消息结尾为\r\n.\r\n ,跳出循环
	//关闭email文件
		fclose(femail);

	//有多少个收件人就执行多少次
	for (int i = 0; i < num; i++) {
		main_client( i,clntSock);
	}

	Sends(clntSock, response[2]); //返回 250 Mail OK\r\n

	//Sends(clntSock, response[4]); //返回 QUIT\r\n,释放连接，之后如果不重新建立连接，使用recv方法会出错，
								  //但是不知道为什么，使用send方法不会出错，我也不知道客户端连接关闭了没有

	//returnerror();

	//打印在屏幕上的内容
	printf("当前时间：%s\n", nowtime);
	//printIP(clntSock);
//	printf("smtp程序的IP地址：127.0.0.1  端口号：1425\n");
	printf("发件人邮件地址：%s", message[1]);
	for (int i = 0; i < num; i++) {
		printf("收件人邮件地址：%s", recvname[i]);
	}

	printf("邮件长度：%d\n", strlen(message[3]));

	//关闭套接字
	closesocket(clntSock);
	closesocket(servSock);
	////终止 DLL 的使用
	WSACleanup();
	system("PAUSE");
	return 0;
}

int main_client(int i,SOCKET returnSock) {
	//创建套接字
	SOCKET clientsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientsock == INVALID_SOCKET)
	{
		printf("invalid socket !");
		return 0;
	}

	sockaddr_in serAddr;

	serAddr.sin_family = PF_INET;
	serAddr.sin_port = htons(25);
	struct hostent *host;  //主机信息

	//这个是自己输入ip
	printf("请输入目的IP：");
	char ip[100];
	scanf("%s", ip);
	serAddr.sin_addr.S_un.S_addr = inet_addr(ip);

	//通过内置的域名获取ip
	//host = gethostbyname("mx1.bupt.edu.cn");
	//memcpy(&serAddr.sin_addr.S_un.S_addr, host->h_addr_list[0], host->h_length); //将获取的主机IP地址复制到客户端网络地址.32位无符号IPV4地址 

	if (connect(clientsock, (sockaddr *)&serAddr, sizeof(serAddr)) != 0)
	{
		printf("connect error !");
		closesocket(clientsock);
		return 0;
	}
	printf("服务器已启动，正在发送邮件\n");
	printIP(clientsock);

	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	Sends(clientsock, message[0]);//发送EHLO 通知发信人的邮件服务器域名
	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	Sends(clientsock, message[1]);//MAIL FROM
	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	Sends(clientsock, recvname[i]);//RCPT TO

	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};
	
	Sends(clientsock, response[5]);//DATA
	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	for (int i = 0; i < divnum; i++) {
		Sends(clientsock, message[3+i]);//邮件内容
	}


	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	Sends(clientsock, response[4]);//quit
	if (returnError(Recvs(clientsock), returnSock) == -1) {
		return -1;
	};

	closesocket(clientsock);
	//WSACleanup();

	return 0;

}

int returnError(char * temp1,SOCKET returnSock) {
	char temp2[2];
	strncpy(temp2, temp1, 1);
	temp2[1] = '\0';
	if (strcmp(temp2, "4") == 0|| strcmp(temp2, "5") == 0) {
		Sends(returnSock, temp1);
		printf("%s\n", temp1);
		return -1;
	}
	else {
		return 0;
	}
}

void printIP(SOCKET socket) {
	SOCKADDR_IN sockAddr1;
	SOCKADDR_IN sockAddr2;
	int iLen1 = sizeof(sockAddr1);
	int iLen2 = sizeof(sockAddr2);
	if (getsockname(socket, (struct sockaddr *)&sockAddr1, &iLen1) == 0) {//得到本地的IP地址和端口号
		printf("本地 IP地址：%s  端口号：%d\n", inet_ntoa(sockAddr1.sin_addr), ntohs(sockAddr1.sin_port));
	};
	//printf("本地 IP地址：%s  端口号：%d\n", inet_ntoa(sockAddr1.sin_addr), sockAddr1.sin_port);
	if (getpeername(socket, (struct sockaddr *)&sockAddr2, &iLen2) == 0) {//得到远程IP地址和端口号
		printf("远程 IP地址：%s  端口号：%d\n", inet_ntoa(sockAddr2.sin_addr), ntohs(sockAddr2.sin_port));
	}
	


}

void initNowtime() {   //初始化当前时间
	char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	time_t timep;
	struct tm *p;
	time(&timep);
	p = gmtime(&timep);
	sprintf(nowtime, "%d %d %d %s %d %d %d\0", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, wday[p->tm_wday], (p->tm_hour + 8) % 24, p->tm_min, p->tm_sec);
}

void writeLog(char * str) {   //编写日志文件
	FILE *fp;
	if ((fp = fopen(filename, "a+")) < 0) {
		printf("fopen() error.\n");
		exit(1);
	}
	fprintf(fp, "%s\n", str);
	fclose(fp);
}

void Sends(SOCKET Sock, char* str) {
	if (send(Sock, str, strlen(str), NULL) < 0) {
		printf("send() error.\n");
		exit(1);
	};
	writeLog(str);//如果发送信息成功写入日志文件
}

char* Recvs(SOCKET Sock) {
	char tBuffer[10000] = { 0 };
	int temp;
	if ((temp = recv(Sock, tBuffer, 10000, NULL))< 0) {
		printf("recv() error1.\n");
		exit(1);
	}
	//printf("%d\n", temp);
	writeLog(tBuffer);
	return tBuffer;
}

int islegal(char ch) {
	if (ch == '.' || ch == '_'||ch=='-' || ch == '!' || ch == '#' || ch == '$' ||
		ch == '%' || ch == '\'' || ch == '*' || ch == '+' || ch == '/' || ch == '='
		|| ch == '?' || ch == '^' || ch == '{' || ch == '|' || ch == '}' || ch == '~') return 1;
	else if (ch >= '0' && ch <= '9') return 1;
	else if (ch >= 'a' && ch <='z') return 1;
	else if (ch >= 'A' && ch <= 'Z') return 1;
	else if (ch == '@') return 1;
	return 0;
}

int afew(char s[], char ch) { // 返回串中有几个字符ch
	int i = 0, n = 0;
	while (s[i]) {
		if (ch == s[i]) ++n;
		++i;
	}
	return n;
}

int pos(char s[], char ch) {
	int i = 0;
	while (s[i] != ch && s[i]) ++i;
	if (s[i] == '\0') return -1;
	return  i;
}

int Checkmailaddr(char email[101]) 
{
	int i, p, flag = 0;


	flag = 1;
	for (i = 0; email[i] && islegal(email[i]); ++i);
	if (email[i] == '\0' && afew(email, '@') == 1) {
		flag = 1;
		if ((p = pos(email, '.')) > 0) {
			flag = 1;
			if (email[p - 1] != '.' && email[p + 1] != '.') {
				flag = 1;
				p = pos(email, '@');
				if (email[p - 1] != '.' && email[p + 1] != '.')
					flag = 1;
				printf("YES\n");
			}
			else flag = 0;
		}
		else flag = 0;
	}
	else flag = 0;
	if (flag == 0) return -1;

	return 0;

}

