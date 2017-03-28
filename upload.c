#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define DATA_SIZE 1024*1024
#define BUFSIZE 128

void error_handling(char * _message)
{
    fputs(_message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int socket_connect(char *_addr, int _port)
{
    int sock = -1;
    struct sockaddr_in serv_addr;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(_addr);
    serv_addr.sin_port = htons(_port);
    if( connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    return sock;
}

unsigned char* file_read(char *_fname, unsigned char *_file_Buf)
{
    int file_size = get_file_size(_fname);
    FILE *fp = fopen(_fname, "rb");
    if(fp == NULL)
        error_handling("file can't read\n");

    fseek(fp, 0, SEEK_SET);
    if(fread(_file_Buf, sizeof(char), file_size, fp) == -1)
        error_handling("fread error\n");
    fclose(fp);
    return _file_Buf;
}

int get_file_size(char *_fname)
{
    FILE *fp = fopen(_fname, "rb");
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    fclose(fp);
    
    return file_size;
}

char * get_sendtime(char *_time_buffer)
{
    time_t rawtime;
	struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(_time_buffer, 80, "%Y%m%d%H%M%S", timeinfo);
    
    return _time_buffer;
}

int get_header_length(char *_header_line)
{
    int header_len;
    header_len = strlen(_header_line);

    return header_len;
}

char *make_http_body(char *_body_line, char *_fname, int *_body_len)
{
    int file_size;
    char pre_body[1024], post_body[1024];
    char boundary[] = "----FormBoundary15aab5b730a";
    char time_buffer[80];
    unsigned char *file_Buf = calloc(sizeof(char), DATA_SIZE);
    
    file_Buf = file_read(_fname, file_Buf);
    file_size = get_file_size(_fname);
    memset(time_buffer, 0x00, sizeof(time_buffer));
    memcpy(time_buffer, get_sendtime(time_buffer), strlen(time_buffer));

    memset(pre_body, 0x00, sizeof(pre_body));
    sprintf(pre_body,   "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                        "Content-Type:application/x-gzip\r\n\r\n"
                        , boundary, _fname);

    memset(post_body, 0x00, sizeof(post_body));
    sprintf(post_body,  "\r\n--%s\r\nContent-Disposition: form-data; name=\"json\"\r\n"
                        "Content-Type: application/json\r\n\r\n"
                        "{\"sendTime\":\"%s\",\"uuid\":\"BBOXF430E48FOSP\"}"
                        "\r\n--%s--\r\n", boundary, time_buffer, boundary);

    int pre_len = strlen(pre_body);
    int post_len = strlen(post_body);
    *_body_len = pre_len + file_size + post_len;
    
    memcpy(_body_line, pre_body, pre_len);
    memcpy(_body_line + pre_len, file_Buf, file_size);
    memcpy(_body_line + pre_len + file_size, post_body, post_len);

    free(file_Buf);

    return _body_line;
    
}

char * make_http_header(char *_header_line, int *_body_len)
{
    char boundary[] = "----FormBoundary15aab5b730a";
    int header_len;
    memset(_header_line, 0x00, sizeof(_header_line));
    sprintf(_header_line, "POST /bb/drive/logfile HTTP/1.1\r\n"
                      "HOST: 52.79.169.203:19090\r\n"
                      "Content-Type: multipart/form-data; boundary=%s\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n", boundary, *_body_len);

    return _header_line;
}

int make_packet(char *_packet, char *_fname, int *_body_len)
{
    int header_len;
    int packet_len;
    char *body_line = calloc(sizeof(char), DATA_SIZE);
    char *header_line = calloc(sizeof(char), DATA_SIZE);
    
    body_line = make_http_body(body_line, _fname, _body_len);
    header_line = make_http_header(header_line, _body_len);
    header_len = get_header_length(header_line);
    
    memcpy(_packet, header_line, header_len);
    memcpy(_packet + header_len, body_line, *_body_len);

    packet_len = header_len + *_body_len;
    
    free(header_line);
    free(body_line);
    return packet_len;
}

void get_response_message(char *_packet, int *_sock)
{
    int total, received, bytes;
    char response[4096];
    memset(response, 0x00, sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(*_sock, response+received, total-received);
        if (bytes < 0)
            error_handling("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if(received == total)
        error_handling("ERROR storing complete response from socket");
    
    printf("Response: \n%s \n", response);
}

int main(int argc, char* argv[])
{
    int i;
	int packet_len;
    int sock;
    int body_len;
    char *packet = calloc(sizeof(char), DATA_SIZE);
	
	if(argc == 2)
	{	
	    sock = socket_connect("52.79.169.203", 19090);
        packet_len = make_packet(packet , argv[1] , &body_len);
        
		// packet print
		for(i=0;i<packet_len;i++)
		{
			printf("%c", packet[i]);
		}
        
		write(sock, packet, packet_len);
        get_response_message(packet , &sock);
		free(packet);
		close(sock);
	}
	else
	{
		fprintf(stderr, "parameter error\n");
		exit(1);
	}
	return 0;
}