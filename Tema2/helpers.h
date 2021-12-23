#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

#define BUFLEN 1600	  // dimensiunea maxima a buffer-ului de date
#define MAX_CLIENTS 5 // numarul maxim de clienti in asteptare

#endif

typedef struct mesaj_TCP
{
	int SF;
	char type;
	char topic[50];
} mesaj_TCP;

typedef struct mesaj_UDP
{
	char topic[50];
	char type;
	char payload[1501];
	char ip[20];
	int port;
} mesaj_UDP;

typedef struct saved_message
{
	char topic[50];
	char type;
	char payload[1501];
	struct saved_message *next;

} saved_message;

typedef struct queue
{
	struct saved_message *head, *tail;
} queue;

typedef struct client
{
	int socket;
	char id[11];
	int sf;
	int online;
	struct client *next;
	struct queue *to_send;

} client;

typedef struct subscription
{
	char topic[50];
	struct client *subscribers;
	struct subscription *next;

} subscription;
