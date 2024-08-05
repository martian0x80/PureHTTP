#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 50
#define BUFFER_SIZE 1024
#define READ_FILE_PATH "/tmp/data/"
#define WRITE_FILE_PATH "/tmp/data/"
#define send_response(fd, msg) send(fd, msg, strlen(msg), 0)

enum request_method
{
  UNKNOWN = -1,
  GET,
  POST,
  PUT,
  PATCH,
  DELETE,
  UPDATE,
  OPTIONS
};

struct request_method_hash
{
  enum request_method method;
  char *str;
} methods[] = {{GET, "GET"}, {POST, "POST"}, {PUT, "PUT"}, {PATCH, "PATCH"}, {DELETE, "DELETE"}, {UPDATE, "UPDATE"}, {OPTIONS, "OPTIONS"}};

struct http_request_line
{
  enum request_method method;
  char *request_target;
  char *type;
  char *http_version;
};

struct http_request_header
{
  char *name;
  char *value;
  struct http_request_header *next;
};

struct http_request
{
  struct http_request_line request_line;
  struct http_request_header *request_headers;
  int content_length;
  char *request_body;
};

void parseRequestLine(char *message, struct http_request_line *request_line)
{
  char *tok = strtok(message, "\r\n");
  tok = strtok(tok, " ");
  enum request_method i;
  for (i = 0; i < sizeof(methods); i++)
  {
    if (strcmp(tok, methods[i].str) == 0)
    {
      request_line->method = methods[i].method;
      break;
    }
  }

  tok = strtok(NULL, " ");
  if (tok != NULL)
  {
    request_line->request_target = tok;
  }
  else
  {
    printf("Error tokenizing message");
  }

  tok = strtok(NULL, " ");
  tok = strtok(tok, "/");
  if (tok != NULL)
  {
    request_line->type = tok;
    tok = strtok(NULL, "/");
    if (tok != NULL)
    {
      request_line->http_version = tok;
    }
    else
    {
      printf("Error tokenizing message");
    }
  }
  else
  {
    printf("Error tokenizing message");
  }
}

void parseRequestHeader(char *message,
                        struct http_request_header *request_headers)
{
  if (message == NULL)
  {
    return;
  }
  char *name = strtok(message, ": ");
  if (name == NULL)
  {
    return;
  }
  char *value = strtok(NULL, " ");
  if (value == NULL)
  {
    return;
  }
  request_headers->name = name;
  request_headers->value = value;
  request_headers->next = NULL;
}

void parseRequest(char *message, struct http_request *request)
{
  char *tok = strtok(message, "\r\n");

  /* Why use this?
   * Since, we are using strtok to tokenize the whole line first (request line,
   * by CRLF) and then we are using strtok to tokenize the tokenized string by
   * the delimiter ": " strtok will overwrite the delimiters with a null
   * character, making the message appear as if it has ended early. So, we
   * manually move the pointer to the next character after the CRLF or first
   * tokenization.
   */

  int request_line_len = strlen(tok) + 2;

  parseRequestLine(tok, &request->request_line);
  message += request_line_len;

  /* Linked list of request headers
   * We are using a linked list to store the request headers
   * because we don't know how many headers there will be.
   * We can't use an array because we don't know the size of the array.
   * We can't use a struct because we don't know how many structs we need.
   * So, we use a linked list to store the headers.
   */

  tok = strtok(message, "\r\n");

  struct http_request_header *temp = NULL;
  struct http_request_header *head = NULL;

  while (tok != NULL)
  {
    if (strstr(tok, ":") == NULL)
    {
      break;
    }
    int header_len = strlen(tok) + 2;
    struct http_request_header *new_header =
        (struct http_request_header *)malloc(
            sizeof(struct http_request_header));
    parseRequestHeader(tok, new_header);
    new_header->next = NULL;

    if (head == NULL)
    {
      head = new_header;
    }
    else
    {
      temp->next = new_header;
    }

    temp = new_header;
    message += header_len;
    tok = strtok(message, "\r\n");
  }
  request->request_headers = head;

  // Parse the request body
  if (tok != NULL)
  {
    request->content_length = strlen(tok);
    request->request_body = tok;
  }
  else
  {
    request->content_length = 0;
    request->request_body = NULL;
  }
}

char *getHeaderValue(struct http_request *request, char *header_name)
{
  struct http_request_header *temp = request->request_headers;
  while (temp != NULL)
  {
    if (strcmp(temp->name, header_name) == 0)
    {
      return temp->value;
    }
    temp = temp->next;
  }
  return NULL;
}

char *readFile(char *filename)
{
  char *full_path = malloc(strlen(READ_FILE_PATH) + strlen(filename) + 1);
  strcpy(full_path, READ_FILE_PATH);
  strcat(full_path, filename);
  FILE *file = fopen(full_path, "r");
  if (file == NULL)
  {
    return NULL;
  }
  // Using STDL functions to get the size of the file
  fseek(file, 0L, SEEK_END);
  long int file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);

  // Using POSIX functions to read the file
  // struct stat file_stat;
  // stat(filename, &file_stat);
  // long int file_size_posix = file_stat.st_size;

  char *file_contents = (char *)malloc(file_size);
  if (fread(file_contents, 1, file_size, file) != file_size)
  {
    fclose(file);
    free(file_contents);
    return NULL;
  }
  fclose(file);
  free(full_path);
  return file_contents;
}

void writeFile(char *filename, char *file_contents)
{
  char *full_path = malloc(strlen(WRITE_FILE_PATH) + strlen(filename) + 1);
  strcpy(full_path, WRITE_FILE_PATH);
  strcat(full_path, filename);
  FILE *file = fopen(full_path, "w");
  if (file == NULL)
  {
    return;
  }
  fwrite(file_contents, 1, strlen(file_contents), file);
  fclose(file);
  free(full_path);
}

void handleConnection(int client_fd)
{
  char buffer[BUFFER_SIZE] = {0};
  int valread = recv(client_fd, buffer, 1024, 0);
  if (valread == -1)
  {
    printf("Read failed: %s \n", strerror(errno));
    exit(1);
  }
  else
  {
    printf("Request received\nBytes Read: %d\n", valread);
  }

  struct http_request req;

  parseRequest(buffer, &req);

  char message_buffer[1024] = "";
  const char *good_request = "HTTP/1.1 200 OK\r\n";
  const char *created = "HTTP/1.1 201 Created\r\n";
  const char *bad_request = "HTTP/1.1 400 Bad Request\r\n";
  const char *not_found = "HTTP/1.1 404 Not Found\r\n";

  if (strcmp(req.request_line.request_target, "/") == 0)
  {
    strcat(message_buffer, good_request);
    strcat(message_buffer, "\r\n");
    send_response(client_fd, message_buffer);
  }

  char *tok = strtok(req.request_line.request_target, "/");
  if (tok != NULL)
  {
    // If the request target is echo
    if (strcmp(tok, "echo") == 0)
    {
      tok = strtok(NULL, "/");

      if (tok != NULL)
      {
        strcat(message_buffer, good_request);
        strcat(message_buffer, "Content-Type: text/plain\r\n");
        strcat(message_buffer, "Content-Length: ");
        int length = snprintf(NULL, 0, "%lu", strlen(tok));
        // Gonna avoid malloc for now
        // char *len = malloc(length + 1);
        snprintf(message_buffer + strlen(message_buffer), length + 1, "%lu",
                 strlen(tok)); // or floor(log10(abs(x))) + 1 instead of
                               // snprintf(NULL, 0, "%d", x)
        // strcat(message_buffer, len);
        strcat(message_buffer, "\r\n\r\n");
        strcat(message_buffer, tok);
        send_response(client_fd, message_buffer);
      }
      else
      {
        strcat(message_buffer, good_request);
        strcat(message_buffer, "Content-Type: text/plain\r\n");
        strcat(message_buffer, "Content-Length: ");
        strcat(message_buffer, "0");
        strcat(message_buffer, "\r\n\r\n");
        send_response(client_fd, message_buffer);
      }
    }
    // If the request target is user-agent
    else if (strcmp(tok, "user-agent") == 0)
    {
      strcat(message_buffer, good_request);
      strcat(message_buffer, "Content-Type: text/plain\r\n");
      strcat(message_buffer, "Content-Length: ");
      char *useragent = getHeaderValue(&req, "User-Agent");
      int length = snprintf(NULL, 0, "%lu", strlen(useragent));
      snprintf(message_buffer + strlen(message_buffer), length + 1, "%lu",
               strlen(useragent));
      strcat(message_buffer, "\r\n\r\n");
      strcat(message_buffer, useragent);
      send_response(client_fd, message_buffer);
    }
    else if (strcmp(tok, "files") == 0)
    {
      tok = strtok(NULL, "/");

      if (tok != NULL)
      {
        char *filename = tok;
        if (req.request_line.method == GET)
        {
          char *file_contents = readFile(filename);
          if (file_contents != NULL)
          {
            strcat(message_buffer, good_request);
            strcat(message_buffer, "Content-Type: application/octet-stream\r\n");
            strcat(message_buffer, "Content-Length: ");
            int length = snprintf(NULL, 0, "%lu", strlen(file_contents));
            snprintf(message_buffer + strlen(message_buffer), length + 1, "%lu",
                     strlen(file_contents));
            strcat(message_buffer, "\r\n\r\n");
            strcat(message_buffer, file_contents);
            send_response(client_fd, message_buffer);
          }
          else
          {
            strcat(message_buffer, not_found);
            strcat(message_buffer, "\r\n");
            send_response(client_fd, message_buffer);
          }
        }
        else if (req.request_line.method == POST)
        {
          char *content_length = getHeaderValue(&req, "Content-Length");
          int length = atoi(content_length);
          char *file_contents = malloc(length + 1);
          strncpy(file_contents, req.request_body, length);
          file_contents[length] = '\0';
          writeFile(filename, file_contents);
          strcat(message_buffer, created);
          strcat(message_buffer, "\r\n");
          send_response(client_fd, message_buffer);
        }
        else
        {
          strcat(message_buffer, bad_request);
          strcat(message_buffer, "\r\n");
          send_response(client_fd, message_buffer);
        }
      }
      else
      {
        strcat(message_buffer, bad_request);
        strcat(message_buffer, "\r\n");
        send_response(client_fd, message_buffer);
      }
    }
    else
    // If the request target is not recognized
    {
      strcat(message_buffer, not_found);
      strcat(message_buffer, "\r\n");
      send_response(client_fd, message_buffer);
    }
  }
  else
  {
    strcat(message_buffer, not_found);
    strcat(message_buffer, "\r\n");
    send_response(client_fd, message_buffer);
  }

  close(client_fd);
}

void setNonBlocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
  {
    printf("fcntl failed: %s \n", strerror(errno));
    exit(1);
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    printf("fcntl failed: %s \n", strerror(errno));
    exit(1);
  }
}

int main()
{
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // Hierarchy of socket programming (=> contains or means, -> can be cast to,
  // <-> can be cast to both ways)
  // addrinfo => sockaddr (sockaddr_in <-> sockaddr(*) <-> sockaddr_in6)  <-
  // sockaddr_storage (generic struct to hold all types of sockaddr structs)
  // in_addr, in6_addr
  //
  // inet_pton, pton => presentation to network
  //
  // Converts the dots-and-numbers string to a network address structure in the
  // af address family in_addr, or in6_addr depending on the address family like
  // AF_INET or AF_INET6 sockaddr_in.sin_addr (in_addr), sockaddr_in6.sin6_addr
  // (in6_addr)
  //
  // inet_ntop, ntop => network to presentation
  // Converts a network address structure in the af address family to a
  // dots-and-numbers string in_addr, or in6_addr depending on the address
  // family like AF_INET or AF_INET6 sockaddr_in.sin_addr (in_addr),
  // sockaddr_in6.sin6_addr (in6_addr)
  //
  //  // IPv4:
  //
  // char ip4[INET_ADDRSTRLEN]; // space to hold the IPv4 string
  // struct sockaddr_in sa; // pretend this is loaded with something
  //
  // inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
  //
  // printf("The IPv4 address is: %s\n", ip4);
  //
  //
  //  // IPv6:
  //
  //  char ip6[INET6_ADDRSTRLEN]; // space to hold the IPv6 string
  //  struct sockaddr_in6 sa6; // pretend this is loaded with something
  //
  //  inet_ntop(AF_INET6, &(sa6.sin6_addr), ip6, INET6_ADDRSTRLEN);
  //
  //  printf("The address is: %s\n", ip6);
  //

  int server_fd;
  socklen_t client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1)
  {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0)
  {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
  {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  int epollfd = epoll_create1(0);
  if (epollfd == -1)
  {
    printf("epoll_create1 failed: %s \n", strerror(errno));
    return 1;
  }

  struct epoll_event event, events[MAX_EVENTS];
  event.events = EPOLLIN;
  event.data.fd = server_fd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &event) == -1)
  {
    printf("epoll_ctl failed: %s \n", strerror(errno));
    return 1;
  }

  int nfds;
  char s[INET6_ADDRSTRLEN];
  printf("Waiting for a client to connect...\n");

  while (1)
  {
    nfds = epoll_wait(epollfd, events, 10, -1);
    if (nfds == -1)
    {
      printf("epoll_wait failed: %s \n", strerror(errno));
      return 1;
    }
    for (int i = 0; i < nfds; i++)
    {
      if (events[i].data.fd == server_fd)
      {
        client_addr_len = sizeof client_addr;
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                               &client_addr_len);
        if (client_fd == -1)
        {
          printf("Accept failed: %s \n", strerror(errno));
          return 1;
        }
        inet_ntop(client_addr.sin_family, &client_addr.sin_addr, s, sizeof(s));
        printf("Client connected from %s:%d\n\n", s, client_addr.sin_port);
        setNonBlocking(client_fd);
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &event) == -1)
        {
          printf("epoll_ctl failed: %s \n", strerror(errno));
          return 1;
        }
      }
      else
      {
        handleConnection(events[i].data.fd);
      }
    }
  }

  // Cleaning up
  close(epollfd);
  close(server_fd);

  // int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
  // &client_addr_len);

  // Do I need this?
  // int val = 1;
  // if (setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0)
  // {
  //     printf("SO_KEEPALIVE failed: %s \n", strerror(errno));
  //     return 1;
  // }

  // handleConnection(client_fd);

  return 0;
}
