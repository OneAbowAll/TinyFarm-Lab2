#include "xsocket.h"
#include "xerrori.h"

#define HOST "127.0.0.1"
#define PORT 65434

int connect_to_collector(){
  // Connessione al server
  struct sockaddr_in serv;
  int skt = 0; // File descript. del socket

  if ((skt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    termina("Errore creazione socket");

  serv.sin_family = AF_INET; // assegna indirizzo
  serv.sin_port = htons(PORT);
  serv.sin_addr.s_addr = inet_addr(HOST);

  if (connect(skt, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    return -1;

  return skt;
}

void send_long(int sktFD, long n){
    int tmp;

    tmp = htonl(n >> 32);
    writen(sktFD, &tmp, 4);
    tmp = htonl(n);
    writen(sktFD, &tmp, 4);
}

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n)
{
  size_t nleft;
  ssize_t nread;

  nleft = n;
  while (nleft > 0)
  {
    if ((nread = read(fd, ptr, nleft)) < 0)
    {
      if (nleft == n)
        return -1; /* error, return -1 */
      else
        break; /* error, return amount read so far */
    }
    else if (nread == 0)
      break; /* EOF */
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft); /* return >= 0 */
}

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;

  nleft = n;
  while (nleft > 0)
  {
    if ((nwritten = write(fd, ptr, nleft)) < 0)
    {
      if (nleft == n)
        return -1; /* error, return -1 */
      else
        break; /* error, return amount written so far */
    }
    else if (nwritten == 0)
      break;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n - nleft); /* return >= 0 */
}