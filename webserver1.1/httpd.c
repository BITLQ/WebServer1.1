#include"httpd.h"
//print syslog
extern int epollfd;
//void print_log(const char* errorstr,int status)
//{
//	char* err[] = {
//		"NOMAL",
//		"WARING",
//		"FALAT"
//		};
//
//	openlog("httpd",LOG_PID|LOG_CONS,LOG_USER);
//
//	// /var/log/messages
//	syslog(LOG_INFO,errorstr,err[status]);
//
//	closelog();
//}

void print_log(const char* destion, int level)
{
    //打印日志
}

//listen sock work function
int startup(char* local_ip,int local_port)
{
	int listen_sock = socket(AF_INET,SOCK_STREAM,0);
	if(listen_sock < 0)
	{
		print_log("socket failed!\n",WARING);
		exit(1);
	}

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(local_port);
	local.sin_addr.s_addr = inet_addr(local_ip);
	int opt = 1;
	if(setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) < 0)
	{
		print_log("setsockopt is failed1\n",WARING);
		exit(1);
	}
	
	int val = 1;	
	socklen_t len = sizeof(val);
	setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,(void*)&val,len);	

	if(bind(listen_sock,(struct sockaddr*)&local,sizeof(local)) < 0)
	{
		print_log("bind falied!\n",WARING);
		exit(1);
	}

	if(listen(listen_sock,5) < 0)
	{
		print_log("liten falied!\n",WARING);
		exit(1);
	}

	return listen_sock;
}


//from sock read request line
int readline(int sock,char buff[],int len)
{
	assert(buff);
	int i = 0;
	char c = 0;

	while(i < len && c != '\n')
	{
		ssize_t s = recv(sock,&c,1,0);
		if(s>0 && c == '\r')
		{
			recv(sock,&c,1,MSG_PEEK);
			if(c == '\n')
			{
				recv(sock,&c,1,0);
			}
			c = '\n';
		}
		buff[i++] = c;
	}
	buff[i] = '\0';
	//printf("DEBUG7: read line buff is %s\n",buff);
	//printf("DEBUG7: read line i is %d\n",i);
	return i;
}

void clear_header(int sock)
{
		int s = -1;
		int ret = 0;
		char buff[SIZE];
		memset(buff,'\0',sizeof(buff));
		do
		{
			s = readline(sock,buff,sizeof(buff));
			//printf("DEBUG: si xunhuan \n");

		}while(s != 1 && strcmp(buff,"\n"));  //bug!!!
}
//error response
static void send_error(int sock,const char* rep)
{	
	clear_header(sock);
	send(sock,rep,strlen(rep),0);
	const char* content_type = "Content-Type:text/html;Charset=ISO-8859-1\r\n\r\n";
	send(sock,content_type,strlen(content_type),0);
}

//return request error status
int echo_error(int sock,int retcode)
{
	char *response = NULL;
	switch(retcode)
	{
		case 400:
			response = "HTTP/1.0 400 ERROR REQUEST\r\n";
			send_error(sock,response);
			break;
		case 403:
			response = "HTTP/1.0 403 SERVER REFUSE REQUEST\r\n";
			send_error(sock,response);
			break;
		case 404:
			response = "HTTP/1.0 404 NOT FOUND\r\n";
			send_error(sock,response);
			break;
		case 500:
			response = "HTTP/1.0 500 SERVER HAVE A ERROR\r\n";
			send_error(sock,response);
			break;
		case 503:
			response = "HTTP/1.0 503 SERVER NOT TO BE USEING\r\n";
			send_error(sock,response);
			break;
		default:
			break;
	}

	send(sock,"<html><h2>",strlen("<html><h2>"),0);
	send(sock,response,strlen(response)-2,0);
	send(sock,"</h2></html>",strlen("</h2></html>"),0);
}


//发送响应
int send_response(int sock,const char* line,int fd,struct stat st,const char* responseline)
{
        int ret = 0;
		clear_header(sock);
    	const char* status_line = "HTTP/1.1 200 OK\r\n";
    	send(sock,status_line,strlen(status_line),0);
        const char* connection = "Connection:keep-alive\r\n";
        send(sock,connection,strlen(connection),0);
        
        char *content_lenth =(char*)malloc(100);
        sprintf(content_lenth,"Content-length:%d\r\n", st.st_size);
        send(sock,content_lenth,strlen(content_lenth),0);

        const char* content_type = "Content-Type:text/html;Charset=utf-8\r\n";
        send(sock,content_type,strlen(content_type),0);
        const char* lang = "Accept-Language: zh-cn\r\n";
        send(sock,lang,strlen(lang),0);
        send(sock,"\r\n",strlen("\r\n"),0);

        if(sendfile(sock,fd,NULL,st.st_size) < 0){
			print_log("respose file failed!\n",WARING);
			ret = -2;
		}

		printf("dubg: send ok!\n");
		return ret;
}

int dir_deal_cgi(int sock,char* dir)
{
    char buf[SIZE];
        int content_len = -1;
        char method_env[SIZE/8];
        char query_string_env[SIZE/4];
        char dir_env[SIZE/8];
        char* path = "dir_cgi.sh";
        
        clear_header(sock);
        int input[2];
        int output[2];
        
        pipe(input);
        pipe(output);

        pid_t id = fork();
        if(id < 0)
        {
            print_log("fork",FALAT);
            echo_error(sock,500);   
            return 11;
        }else if(id == 0){
            close(input[1]);
            close(output[0]);
                    
            dup2(input[0],0);
            dup2(output[1],1);
            close(sock);
            sprintf(dir_env,"DIR_ENV=%s",dir);
            putenv(dir_env);

            execl(path,path,NULL);
            exit(1);
        }

    else
        {
            close(input[0]);
            close(output[1]);
            char* arr = (char*)malloc(1024*1024);
            memset(arr,'\0',sizeof(char)*1024*1024);
            
            char ch = '\0';
            int i = 0;
            while(read(output[0],&ch,1) > 0)
            {
                arr[i++]=ch;
            }

            const char* status_line = "HTTP/1.1 200 OK\r\n";
            send(sock,status_line,strlen(status_line),0);
            const char* connection = "Connection:keep-alive\r\n";
            send(sock,connection,strlen(connection),0);

            char *content_lenth =(char*)malloc(100);
            sprintf(content_lenth,"Content-length:%d\r\n", i);
            send(sock,content_lenth,strlen(content_lenth),0);

            const char* content_type = "Content-Type:text/html;Charset=utf-8\r\n";
            send(sock,content_type,strlen(content_type),0);
            const char* lang = "Accept-Language: zh-cn\r\n";
            send(sock,lang,strlen(lang),0);
            send(sock,"\r\n",strlen("\r\n"),0);

            send(sock,arr,i,0);
            waitpid(id,NULL,0);
            print_log("father is go done..", FALAT);
    }
    return 0; 
}

int dir_deal(int sock, struct stat st, char* pathline)
{

    int ret = 0;
	clear_header(sock);
    int size = 0;
    char* result=(char*)malloc(1024*1024*1024);
    memset(result, '\0', 1024*1024*1024);

    printf("runing dir_deal\n");
    struct dirent *p_dirent;
    DIR *p_Dir ;  //定义一个DIR类的指针
    p_Dir=opendir(pathline); //  opendir方法打开path目录，并将地址付给pDir指针
     
     char* filename = (char*)malloc(100);
     while((p_dirent=readdir(p_Dir)))
     {
        if(strcmp(p_dirent->d_name,".")==0 || strcmp(p_dirent->d_name,"..")==0)     
         {
             continue;
         }
         memset(filename,'\0',100);
         sprintf(filename,"<a href=\"%s/%s\">%s</a><br>",pathline,p_dirent->d_name,p_dirent->d_name);
         printf("%s\n",filename);
         strcat(result,filename);
     }
     size = strlen(result);
     closedir(p_Dir);
    
    const char* status_line = "HTTP/1.1 200 OK\r\n";
    send(sock,status_line,strlen(status_line),0);
    const char* connection = "Connection:keep-alive\r\n";
    send(sock,connection,strlen(connection),0);
    
    char *content_lenth =(char*)malloc(100);
    sprintf(content_lenth,"Content-length:%d\r\n", size);
    send(sock,content_lenth,strlen(content_lenth),0);
    
    const char* content_type = "Content-Type:text/html;Charset=utf-8\r\n";
    send(sock,content_type,strlen(content_type),0);
    const char* lang = "Accept-Language: zh-cn\r\n";
    send(sock,lang,strlen(lang),0);
    send(sock,"\r\n",strlen("\r\n"),0);
    
    send(sock,result,size,0);
    return ret;
}

//deal cgi  read line get / http/1.0
int cgi_deal(int sock,char* method, char* path, char* ptr)
{
	char buf[SIZE];
	int content_len = -1;
	char method_env[SIZE/8];
	char query_string_env[SIZE/4];
	char content_len_env[SIZE/8];

	if(strcasecmp(method,"GET") == 0)//get
	{
		clear_header(sock);
	}else{ //post
		int ret = 0;
		do{
			ret = readline(sock,buf,sizeof(buf));
			printf("DEBUG5: post read buff is %s\n",buf);
			if(ret > 0 && \
				strncasecmp(buf,"Content-Length: ",16) == 0){
					content_len = atoi(buf+16);
					printf("DEBUG6: content_len is %d\n",content_len);
				}
		}while(ret != 1 && strcmp(buf,"\n") != 0);
		
		printf("DEBUG4: content-length: %d",content_len);
		//1.maybe get content_len
		//2.must clean header
		//Content_Lenth 代表数据长度 
		//处理粘包问题，1.定行，空行，Content_Lenth  指定长度
		if(content_len < 0)
		{
			printf("DEBUG2: content-length: %d",content_len);
			echo_error(sock,403);
			return 10;
		}//fi
	}//else
	//Get 和 Post 的请求行和请求包头和空行都读取了；
	//tell Mozila this is a html;

	//child proc
	int input[2];
	int output[2];
	
	pipe(input);
	pipe(output);

	pid_t id = fork();
	if(id < 0)
	{
		print_log("fork",FALAT);
		echo_error(sock,500);    // server fault;
		return 11;
	}else if(id == 0){ 		//child ->exrc bin<cgi
		close(input[1]);
		close(output[0]);
		
		//进行文件描述符的重定向；
		dup2(input[0],0);
		dup2(output[1],1);
		close(sock);		//child proc close the sock;

		//程序替换只替换目标代码和数据，不会改变环境变量
		
		//arguments through env val pass;
		sprintf(method_env,"METHOD=%s",method);
		putenv(method_env);

		if(strcasecmp(method,"get") == 0)
		{	 //get
			sprintf(query_string_env,"QUERY_STRING=%s",ptr);
			putenv(query_string_env);
		}else//post
		{
			sprintf(content_len_env,"CONTENT_LENGTH=%d",content_len);
			putenv(content_len_env);
		}//else

		execl(path,path,NULL);
		exit(1);
	}//else if

	else//father
	{
		close(input[0]);
		close(output[1]);

		//  ???  !!!
		if(strcasecmp(method,"POST") == 0)
		{
			int i = 0;
			char ch = '\0';

			for(; i < content_len; ++i)
			{
				recv(sock,&ch,1,0);
				write(input[1],&ch,1);
			}
		}
		
        char* arr = (char*)malloc(1024*1024);
        memset(arr,'\0',sizeof(char)*1024*1024);

        //int ret = read(output[0], arr, sizeof(arr));
		char ch = '\0';
        int i = 0;
		while(read(output[0],&ch,1) > 0)
		{
			//send(sock,&ch,1,0);
            arr[i++]=ch;
		}
        
    	const char* status_line = "HTTP/1.1 200 OK\r\n";
    	send(sock,status_line,strlen(status_line),0);
        const char* connection = "Connection:keep-alive\r\n";
        send(sock,connection,strlen(connection),0);
        
        char *content_lenth =(char*)malloc(100);
        sprintf(content_lenth,"Content-length:%d\r\n", i);
        send(sock,content_lenth,strlen(content_lenth),0);

        const char* content_type = "Content-Type:text/html;Charset=utf-8\r\n";
        send(sock,content_type,strlen(content_type),0);
        const char* lang = "Accept-Language: zh-cn\r\n";
        send(sock,lang,strlen(lang),0);
        send(sock,"\r\n",strlen("\r\n"),0);

        send(sock,arr,i,0);
		waitpid(id,NULL,0);
        print_log("father is go done..", FALAT);
	}
    return 0; 
}

//判断请求路径的有效,并处里
int deal_path(int sock,struct stat st,char* method,char *path)
{
    printf("DEBUG: running deal_path");
	int ret = 0;   //return status;
	
	int fd = open(path,O_RDONLY);
	if(fd < 0)
	{
		print_log("open file failed\n",WARING);
		ret = 6;
	}else{
		const char* response = "HTTP/1.0 200 OK\r\n";
        print_log("send_response is running\n", FALAT);
	 	ret = send_response(sock,path,fd,st,response);
	}
	
	close(fd);
	return ret;
}//end



//sock deal funcation
int headler_sock(int sock)
{
	int ret = 0;
	//获取GET/POST 和 url
	//判断是否带有参数,利用指针
	
	char buff[102400];
	char path[SIZE];    //保存路径
	char method[64] = {0};
	char* uri_point = NULL;
	
	//CGI
	int cgi = 0;

	int i = 0;
	int j = 0;
	char c = buff[i];
	//GET /path?a=b&b=c HTTP/1.1

	if(readline(sock,buff,sizeof(buff)) < 0)
	{
		ret = 1;
		goto end;
	}

	//printf("DEBUG: request line is : %s\n",buff);
	uri_point = buff;

	while(i < strlen(buff) && c != ' ')
	{
		c = buff[i];
		if(c != ' ')
		{
			method[j] = buff[i];
			uri_point++;
		}
		if(c == ' ')
		{
			method[i] = '\0';
			uri_point++;
			break;
		}
		i++;
		j++;
	}

	//GET / HTTP/1.0
	while(*uri_point == ' ')
	{
		uri_point++;
	}

	if(!strcasecmp(method,"GET") && !strcasecmp(method,"POST"))
	{
		echo_error(sock,403);
		goto end;
	}

	
	//path = / ; method = GET/POST
	i = 0;
	if(strcasecmp(method,"GET") == 0)
	{
		while( *uri_point != ' ')
		{
			if(*uri_point == '?')
			{
				cgi = 1;
				uri_point++;//point argument
				break;
			}else
			{
				path[i++] = *uri_point++;
			}
		}
		path[i] = '\0';
		
		char* tmp = uri_point;
		while(*tmp != ' ')
		{
			tmp++;
		}
	
		*tmp = '\0';
	}
	

	//method is post?
	if(strcasecmp(method,"POST") == 0)
	{
		int i = 0;
		while(*uri_point != ' ')
		{
			path[i++] = *uri_point++;
		}

		path[i] = '\0';
		printf("DEBUG: POST path is %s\n",path);
	}
	

	char pathline[SIZE]; //keep path
	memset(pathline,'\0',sizeof(pathline));
	sprintf(pathline,"wwwroot%s",path);

	struct stat st;   //存储文件信息 
	i = strlen(pathline);
	if(pathline[i - 1] == '/')
	{
		// wwwroot/ > return index.html
		strcat(pathline,"index.html");
	}

    printf("path is %s\n", pathline);
	if(stat(pathline,&st) < 0)
	{
        printf("path is error 404\n");
		echo_error(sock,404);
		ret = 2;
		return ret;
	}
    else if(S_ISDIR(st.st_mode)){
        printf("%s ISDIR is running dir_deal\n", pathline);
        dir_deal_cgi(sock, pathline);
        return 0;
	}
    else if(S_ISREG(st.st_mode))
	{
		 if(st.st_mode&S_IXUSR || st.st_mode&S_IXGRP || st.st_mode&S_IXOTH){
			//fork + exec
			cgi = 1;
		 }
       // print_log("deal_path is running\n", FALAT);
	   // ret = deal_path(sock,st,method,pathline);
	   // if(ret != 0)
	   // {
	   // 	print_log("deal_path is failed!\n",FALAT);
	   //     goto end;
	   // }
       // print_log("deal is overd\n", FALAT);
       // return 0;
	}
	else{  
		echo_error(sock,400);
		ret = 3;
		return ret;
	}

    if(cgi != 1)
    {
        //非动态请求
	    ret = deal_path(sock,st,method,pathline);
        if(ret < 0)
        {
            print_log("deal_path is fault!\n",FALAT);
        }
    }
    else
    {
        //动态请求
        ret = cgi_deal(sock,method,pathline, uri_point);
        if(ret < 0)
        {
            print_log("cgi_deal is fault\n", FALAT);
        }
    }
end:
	return ret; 
}



//pthread deal funcation
int deal_accept_request(int connfd)
{
	if(headler_sock(connfd) != 0)
	{
		print_log("headler_sock is fault!\n",WARING);
        return 1;
	}
    printf("deal_accept_request is over... ....\n");
    modfd(epollfd, connfd);
    printf("epollfd is %d\n",epollfd);
    return 0;
}