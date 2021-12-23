#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

// Functie care cauta un client dupa socketul dat ca parametru
struct client *find_Client_by_Socket(struct client *head, int socket)
{
	struct client *temp = head;
	while (temp)
	{
		if (temp->socket == socket)
		{
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

// Functie care cauta un topic in lista de topicuri si
// intoarce 1 daca il gaseste, -1 altfel
int findTopic(struct subscription *head, char *topic)
{
	subscription *temp = head;
	while (temp)
	{
		if (strcmp(temp->topic, topic) == 0)
		{
			return 1;
		}
		temp = temp->next;
	}
	return -1;
}

// Functie care creeaza un client nou si il adauga il lista de clienti
void addClientFirst(struct client **head, int socket, char *id, int sf)
{
	struct client *cli = malloc(sizeof(struct client));
	cli->next = (*head);
	cli->socket = socket;
	cli->online = 1;
	cli->to_send = malloc(sizeof(struct queue));
	cli->to_send->head = NULL;
	cli->to_send->tail = NULL;
	strcpy(cli->id, id);
	cli->sf = sf;
	(*head) = cli;
}

// Functie care cauta un client dupa id
struct client *find_Client_by_Id(struct client *head, char *id)
{
	struct client *temp = head;
	while (temp)
	{
		if (strcmp(temp->id, id) == 0)
		{
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

// Functie care adauga in coada de mesaje un mesaj
// dat ca parametru
void enqueue(struct queue *q, struct saved_message *mesaj)
{
	if (q->tail == NULL)
	{
		q->head = mesaj;
		q->tail = mesaj;
		return;
	}
	q->tail->next = mesaj;
	q->tail = mesaj;
}

// Functie care scoate din coada de mesaje, primul mesaj
void dequeue(struct queue *q)
{
	if (q->head == NULL)
		return;
	struct saved_message *aux = q->head;
	q->head = q->head->next;
	if (q->head == NULL)
		q->tail = NULL;
	free(aux);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc != 2)
	{
		usage(argv[0]);
	}
	int tcp_sockfd, udp_sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in tcp_serv_addr, udp_serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	// Lista de clienti
	struct client *clientlist = NULL;

	// Lista de topicuri
	struct subscription *subscriptions = NULL;

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	portno = atoi(argv[1]);
	DIE(portno == 0, "Error atoi function\n");

	// Initializare socket TCP
	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_sockfd < 0, "Error tcp socket\n");

	// Setare campuri
	memset((char *)&tcp_serv_addr, 0, sizeof(tcp_serv_addr));
	tcp_serv_addr.sin_family = AF_INET;
	tcp_serv_addr.sin_port = htons(portno);
	tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;

	// Asociere socket tcp cu adresa tcp a serverului
	ret = bind(tcp_sockfd, (struct sockaddr *)&tcp_serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error bind tcp\n");

	// Se asculta pe scoketul creat cereri de la clienti
	ret = listen(tcp_sockfd, MAX_CLIENTS);
	DIE(ret < 0, "Error listen tcp\n");

	// Initializare socket UDP
	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_sockfd < 0, "Error udp socket\n");

	// Setare campuri
	memset((char *)&udp_serv_addr, 0, sizeof(udp_serv_addr));
	udp_serv_addr.sin_family = AF_INET;
	udp_serv_addr.sin_port = htons(portno);
	udp_serv_addr.sin_addr.s_addr = INADDR_ANY;

	// Asociere socket udp cu adresa udp a serverului
	ret = bind(udp_sockfd, (struct sockaddr *)&udp_serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error bind udp\n");

	// se adauga noii file descriptori in multimea read_fds
	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

	fdmax = tcp_sockfd > udp_sockfd ? tcp_sockfd : udp_sockfd;

	// Dezactivare algoritm Nagle
	int yes = 1;
	int result = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
	DIE(result < 0, "Error disable Nagle's algorithm\n");

	// Reutilizarea portului
	int enable = 1;
	if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
	{
		perror("setsocketopt\n");
		exit(1);
	}

	socklen_t udp_len = sizeof(udp_serv_addr);

	while (1)
	{
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "Error select\n");

		// Citire de la tastura
		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);

			// Comanda exit
			// Inchiderea serverului si a clientilor TCP
			if (strncmp(buffer, "exit", 4) == 0)
			{
				for (int i = 1; i <= fdmax; i++)
				{

					if (FD_ISSET(i, &read_fds))
					{
						close(i);
					}
				}
				break;
			}
			else
			{
				perror("Invalid command. Only accepted command is exit.\n");
			}
		}

		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				// Serverul primeste mesaj pe socket-ul udp
				if (i == udp_sockfd)
				{
					memset(buffer, 0, BUFLEN);
					ret = recvfrom(udp_sockfd, &buffer, BUFLEN, 0, (struct sockaddr *)&udp_serv_addr, &udp_len);
					DIE(ret < 0, "Error receive udp message\n");

					// Construire mesaj UDP
					mesaj_UDP mesaj;
					memset(&mesaj, 0, sizeof(mesaj_UDP));
					memcpy(mesaj.topic, buffer, 50);
					mesaj.type = buffer[50];
					mesaj.port = ntohs(udp_serv_addr.sin_port);
					strcpy(mesaj.ip, inet_ntoa(udp_serv_addr.sin_addr));
					if (strlen(&buffer[51]) == 1500)
						strcat(buffer, "\0");
					memcpy(mesaj.payload, &buffer[51], 1501);

					// Adaugare topic in lista de topicuri in cazul in care
					// el nu exista in lista
					if (findTopic(subscriptions, mesaj.topic) == -1)
					{
						struct subscription *sub = malloc(sizeof(struct subscription));
						strcpy(sub->topic, mesaj.topic);
						sub->subscribers = NULL;
						sub->next = subscriptions;
						subscriptions = sub;
					}

					// Se parcurge lista de topicuri
					// Se trimit mesaje clientilor online, care sunt abonati
					// la topicul mesajului primit
					// Se pune mesajul in coada mesaje a
					// clientilor offline, dar care
					// au SF = 1 pentru topicul mesajului primit
					struct subscription *sub = subscriptions;
					while (sub)
					{
						if (strcmp(sub->topic, mesaj.topic) == 0)
						{
							struct client *cli = sub->subscribers;
							while (cli)
							{
								struct client *clientcurent = find_Client_by_Id(clientlist, cli->id);
								if (clientcurent->online == 1)
								{
									int socket = clientcurent->socket;
									int m = send(socket, &mesaj, sizeof(mesaj), 0);
									DIE(m < 0, "Send tcp message\n");
								}
								else
								{
									if (cli->sf == 1)
									{
										struct saved_message *m = malloc(sizeof(struct saved_message));
										m->next = NULL;
										m->type = mesaj.type;
										memcpy(m->topic, mesaj.topic, 50);
										memcpy(m->payload, mesaj.payload, 1500);
										enqueue(clientcurent->to_send, m);
									}
								}
								cli = cli->next;
							}
						}
						sub = sub->next;
					}
				}
				else
				{
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					if (i == tcp_sockfd)
					{
						memset(buffer, 0, BUFLEN);
						clilen = sizeof(cli_addr);
						newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &clilen);
						DIE(newsockfd < 0, "Error accept\n");
						ret = recv(newsockfd, &buffer, BUFLEN, 0);
						DIE(ret < 0, "Error receiving user's id\n");
						int connected = 0;
						struct client *temp = clientlist;
						while (temp)
						{
							// Daca un client nou incearca sa se conecteze cu un id
							// deja folosit de un client conectat, se trimite
							// un mesaj de eroare si se inchide conexiunea
							if (strcmp(temp->id, buffer) == 0 && temp->online == 1)
							{
								mesaj_UDP mesaj;
								memset(&mesaj, 0, sizeof(mesaj));
								strcpy(mesaj.topic, "error");
								n = send(newsockfd, &mesaj, sizeof(mesaj_UDP), 0);
								DIE(n < 0, "Error send error\n");
								close(newsockfd);
								connected = 1;
								break;
							}

							// Daca un client s-a reconectat, i se actualizeaza socket-ul
							// si i se trimit mesajele retinute, pentru topicurile la care
							// sf era setat la 1
							if (strcmp(temp->id, buffer) == 0 && temp->online == 0)
							{
								connected = 2;
								temp->online = 1;
								temp->socket = newsockfd;
								printf("New client %s connected from %s:%d\n", buffer,
									   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

								int yes = 1;
								int result = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
								DIE(result < 0, "Error disable Nagle algorithm\n");

								struct saved_message *m = temp->to_send->head;
								if (m != NULL)
								{
									while (m)
									{
										mesaj_UDP mesaj;
										memset(&mesaj, 0, sizeof(mesaj_UDP));
										memcpy(mesaj.topic, &m->topic, 50);
										mesaj.type = m->type;
										mesaj.port = ntohs(udp_serv_addr.sin_port);
										strcpy(mesaj.ip, inet_ntoa(udp_serv_addr.sin_addr));
										if (strlen(&buffer[51]) == 1500)
											strcat(buffer, "\0");
										memcpy(mesaj.payload, m->payload, 1501);
										n = send(newsockfd, &mesaj, sizeof(mesaj_UDP), 0);
										DIE(n < 0, "Error send message\n");
										m = m->next;
										dequeue(temp->to_send);
									}
								}

								// Se adauga noul socket in multimea de
								// file descriptori
								FD_SET(newsockfd, &read_fds);
								if (newsockfd > fdmax)
								{
									fdmax = newsockfd;
								}
								break;
							}
							temp = temp->next;
						}

						// Daca un client nou incearca sa se conecteze cu un id
						// deja folosit de un client conectat, serverul printeaza
						// un mesaj de eroare "Client already connected"
						if (connected == 1)
						{
							char error[BUFLEN] = {"Client "};
							strcat(error, buffer);
							strcat(error, " already connected.");
							printf("%s\n", error);
							continue;
						}
						else
						{
							// Daca se conecteaza un client cu un id unic,
							// se afiseaza mesajul de conectare si se adauga
							// in lista de clienti
							if (connected == 0)
							{
								printf("New client %s connected from %s:%d\n", buffer,
									   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
								addClientFirst(&clientlist, newsockfd, buffer, 0);
								FD_SET(newsockfd, &read_fds);

								int yes = 1;
								int result = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
								DIE(result < 0, "Error disable Nagle algorithm\n");

								if (newsockfd > fdmax)
								{
									fdmax = newsockfd;
								}
							}
						}
					}
					else
					{
						// s-au primit date pe unul din socketii de client,
						// asa ca serverul trebuie sa le receptioneze
						mesaj_TCP comanda;
						memset(&comanda, 0, sizeof(mesaj_TCP));
						n = recv(i, &comanda, sizeof(mesaj_TCP), 0);
						DIE(n < 0, "Error receiving mesaj_TCP\n");
						// Clientul s-a deconectat
						// Se seteaza campul online la 0
						// Se inchide socket-ul si se scoate din multimea
						// de file descriptori
						if (n == 0)
						{
							struct client *cl = clientlist;
							while (cl)
							{
								if (cl->socket == i)
								{
									cl->online = 0;
									printf("Client %s disconnected.\n", cl->id);
									break;
								}
								cl = cl->next;
							}
							close(i);
							FD_CLR(i, &read_fds);
						}
						else
						{
							// Clientul a trimis o cerere de subscribe
							// Daca topicul la care se aboneaza clientul
							// nu exista in lista de topicuri, atunci el
							// este adaugat in lista
							// Daca exista, atunci clientul care a facut
							// cererea este adaugat in lista de subscriberi
							// a topicului
							if (comanda.type == 's')
							{
								if (subscriptions == NULL)
								{
									struct subscription *sub = malloc(sizeof(struct subscription));
									sub->next = NULL;
									strcpy(sub->topic, comanda.topic);
									struct client *cl = find_Client_by_Socket(clientlist, i);

									struct client *newsubscriber = malloc(sizeof(struct client));
									strcpy(newsubscriber->id, cl->id);
									newsubscriber->next = NULL;
									newsubscriber->to_send = NULL;
									newsubscriber->sf = comanda.SF;
									newsubscriber->socket = cl->socket;
									newsubscriber->online = cl->online;

									sub->subscribers = newsubscriber;
									subscriptions = sub;
								}
								else
								{
									if (findTopic(subscriptions, comanda.topic) == -1)
									{
										struct subscription *sub = malloc(sizeof(struct subscription));
										strcpy(sub->topic, comanda.topic);
										struct client *cl = find_Client_by_Socket(clientlist, i);

										struct client *newsubscriber = malloc(sizeof(struct client));
										strcpy(newsubscriber->id, cl->id);
										newsubscriber->next = NULL;
										newsubscriber->to_send = NULL;
										newsubscriber->sf = comanda.SF;
										newsubscriber->socket = cl->socket;
										newsubscriber->online = cl->online;

										sub->subscribers = newsubscriber;
										sub->next = subscriptions;
										subscriptions = sub;
									}
									else
									{
										struct subscription *temp = subscriptions;
										while (temp)
										{
											if (strcmp(temp->topic, comanda.topic) == 0)
											{
												struct client *cl = find_Client_by_Socket(clientlist, i);
												struct client *newclient = malloc(sizeof(struct client));
												strcpy(newclient->id, cl->id);
												newclient->sf = comanda.SF;
												newclient->socket = cl->socket;
												newclient->to_send = cl->to_send;
												newclient->online = cl->online;
												newclient->next = temp->subscribers;
												temp->subscribers = newclient;
												break;
											}
											temp = temp->next;
										}
									}
								}
							}

							// Clientul a facut o cerere de unsubscribe
							// Clientul care a facut cererea este
							// eliminat din lista de subscriberi a topicului
							if (comanda.type == 'u')
							{
								struct subscription *sub = subscriptions;
								struct client *client = find_Client_by_Socket(clientlist, i);
								while (sub)
								{
									if (strcmp(sub->topic, comanda.topic) == 0)
									{
										struct client *cli = sub->subscribers;
										if (strcmp(cli->id, client->id) == 0)
										{
											sub->subscribers = sub->subscribers->next;
										}
										else
										{
											while (strcmp(cli->next->id, client->id) != 0)
											{
												cli = cli->next;
											}
											cli->next = cli->next->next;
										}
									}
									sub = sub->next;
								}
							}
						}
					}
				}
			}
		}
	}

	// Eliberarea memoriei pentru lista de clienti
	struct client *temp = clientlist;
	while (temp)
	{
		struct client *aux = temp;
		struct saved_message *s = temp->to_send->head;
		while (s)
		{
			struct saved_message *f = s;
			s = s->next;
			free(f);
		}
		free(temp->to_send);
		temp = temp->next;
		free(aux);
	}

	// Eliberarea memoriei pentru lista de topicuri
	struct subscription *sub = subscriptions;
	while (sub)
	{
		struct subscription *aux = sub;

		struct client *cli = sub->subscribers;
		while (cli)
		{
			struct client *temp = cli;

			cli = cli->next;
			free(temp);
		}
		sub = sub->next;
		free(aux);
	}

	// Inchiderea socketilor TCP si UDP
	close(tcp_sockfd);
	close(udp_sockfd);

	return 0;
}
