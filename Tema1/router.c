#include <queue.h>
#include "skel.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/if_ether.h>

#define IP_OFF (sizeof(struct ether_header))

struct route_table_entry
{
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} __attribute__((packed));

struct arp_entry
{
	__u32 ip;
	uint8_t mac[6];
};

struct route_table_entry *rtable;
int rtable_size;

struct arp_entry *arp_table;
int arp_table_len = 0;

//Functie care citeste linie cu linie un fisier si
//construieste tabela de rutare
void read_table(char *filename)
{
	FILE *f = fopen(filename, "r");
	int k = 0;
	if (f == NULL)
	{
		perror("Eroare la deschidere");
	}
	else
	{
		char buffer[50];
		char prefix[20];
		char nexthop[20];
		char mask[50];
		int interface;
		//Cat timp se poate citi din fisier
		while (fgets(buffer, 50, f))
		{
			//Parsez linia citita
			sscanf(buffer, "%s%s%s%d", prefix, nexthop, mask, &interface);
			//Setez campurile elementelor din tabela
			rtable[k].prefix = inet_addr(prefix);
			rtable[k].next_hop = inet_addr(nexthop);
			rtable[k].mask = inet_addr(mask);
			rtable[k].interface = interface;
			k++;
		}
	}
	fclose(f);
}

//Functie care intoarce numarul de linii din fisier
//Folosita pentru a aloca tabela de rutare si pentru a
//seta size-ul tabelei
int numberoflines()
{
	FILE *f = fopen("rtable0.txt", "r");
	int k = 0;
	int c;
	if (f == NULL)
		return k;
	else
	{
		while (!feof(f))
		{
			c = fgetc(f);
			if (c == '\n')
			{
				k++;
			}
		}
	}
	fclose(f);
	return k;
}

//Comparator folosit pentru qsort
//Sortare crescatoare dupa prefix
//In cazul in care 2 intrari din tabela
//au prefixele egale, atunci se sorteaza
//descrescator dupa masca
int comparator(const void *a, const void *b)
{
	struct route_table_entry *r1 = (struct route_table_entry *)a;
	struct route_table_entry *r2 = (struct route_table_entry *)b;
	if (r1->prefix == r2->prefix)
	{
		return (int)r2->mask - r1->mask;
	}
	else
	{
		return (int)r1->prefix - r2->prefix;
	}
}

//Functie care intoarce intrarea din tabela ARP
//ce are ip-ul dat ca parametru
struct arp_entry *get_arp_entry(__u32 ip)
{
	for (int i = 0; i < arp_table_len; i++)
	{
		if (arp_table[i].ip == ip)
			return &arp_table[i];
	}
	return NULL;
}

//Functie care adauga in tabela ARP
//o intrare care are ip-ul si mac-ul
//date ca parametri
void add_arp_entry(uint32_t ip, uint8_t mac[6])
{

	if (arp_table == NULL)
	{
		arp_table_len++;
		arp_table = realloc(arp_table, sizeof(struct arp_entry) * arp_table_len);
		arp_table[0].ip = ip;
		memcpy(arp_table[0].mac, mac, 6);
	}
	else
	{
		//Verific daca exista deja in tabela, pentru a nu avea duplicate
		struct arp_entry *a = get_arp_entry(ip);
		if (a == NULL)
		{
			arp_table_len++;
			arp_table = realloc(arp_table, sizeof(struct arp_entry) * arp_table_len);
			arp_table[arp_table_len - 1].ip = ip;
			memcpy(arp_table[arp_table_len - 1].mac, mac, 6);
		}
	}
}

//Cauta ruta cea mai specifica in tabela de rutare.
//Cautare binara - O(logn)
int get_best_route(__u32 dest_ip, int left, int right)
{

	if (right >= left)
	{
		int mid = (left + right) / 2;

		//Daca s-a gasit ruta care face match pe dest_ip
		if ((dest_ip & rtable[mid].mask) == rtable[mid].prefix)
		{
			//Se cauta ruta cea mai specifica cu masca cea mai mare
			uint32_t prefix = rtable[mid].prefix;
			if (mid != 0)
			{
				while (rtable[mid].prefix == prefix)
				{
					mid--;
				}
				return mid + 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			if ((dest_ip & rtable[mid].mask) < rtable[mid].prefix)
			{
				return get_best_route(dest_ip, left, mid - 1);
			}
			else
			{
				return get_best_route(dest_ip, mid + 1, right);
			}
		}
	}
	//Daca nu s-a gasit nico ruta, se intoarce -1
	return -1;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	packet m;
	int rc;

	init(argc - 2, argv + 2);
	//Intializare rtable
	rtable_size = numberoflines();
	rtable = malloc(sizeof(struct route_table_entry) * rtable_size);
	read_table(argv[1]);
	//Sortare tabela de rutare - O(nlogn)
	qsort(rtable, rtable_size, sizeof(struct route_table_entry), comparator);

	//Initializare arp_table
	arp_table = NULL;
	//Intializare coada de pachete
	queue q = queue_create();

	while (1)
	{
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		/* Students will write code here */

		struct ether_header *eth_hdr = (struct ether_header *)m.payload;
		struct iphdr *ip_hdr = (struct iphdr *)(m.payload + IP_OFF);

		//Verificare primire pachet IP
		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP)
		{
			struct icmphdr *icmp_hdr = parse_icmp(m.payload);
			//Verificare primire pachet ICMP
			if (icmp_hdr != NULL)
			{
				//Verificare primire pachet ICMP_ECHO_REQUEST si daca
				//pachetul este destinat router-ului
				if ((icmp_hdr->type == ICMP_ECHO) && (ip_hdr->daddr == inet_addr(get_interface_ip(m.interface))))
				{
					send_icmp(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, 0, 0, m.interface, icmp_hdr->un.echo.id, icmp_hdr->un.echo.sequence);
					continue;
				}
			}

			//Verificare checksum
			if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0)
			{
				//Discard packet
				continue;
			}

			//Verificare TIMEOUT
			if (ip_hdr->ttl <= 1)
			{
				//Trimitere ICMP_ERROR = ICMP_TIME_EXCEEDED sender-ului
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, m.interface);
				continue;
			}

			//Update ttl si checksum
			ip_hdr->ttl--;
			ip_hdr->check = 0;
			ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));

			//Cautare ruta
			int index = get_best_route(ip_hdr->daddr, 0, rtable_size - 1);

			//Ruta nu a fost gasita
			if (index == -1)
			{
				//Trimitere ICMP_ERROR = ICMP_DEST_UNREACH sender-ului
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ICMP_DEST_UNREACH, 0, m.interface);
				continue;
			}

			struct route_table_entry *best_route = &rtable[index];
			struct arp_entry *arp = get_arp_entry(best_route->next_hop);

			//Nu exista arp_entry pentru best_route->next_hop
			if (arp == NULL)
			{
				//Am trimis un ARP REQUEST pe broadcast
				//pentru a identifica mac-ul best_route->next_hop-ului

				//Am facut o copie a pachetului primit, caruia i-am setat
				//interface la best_route->interface si l-am pus in
				//coada de pachete
				packet temp;
				temp.len = m.len;
				temp.interface = best_route->interface;
				memcpy(temp.payload, m.payload, sizeof(m.payload));
				eth_hdr->ether_type = htons(ETHERTYPE_ARP);

				for (int i = 0; i < 6; i++)
				{
					eth_hdr->ether_dhost[i] = 255;
				}
				get_interface_mac(best_route->interface, eth_hdr->ether_shost);
				send_arp(best_route->next_hop, inet_addr(get_interface_ip(best_route->interface)), eth_hdr, best_route->interface, htons(ARPOP_REQUEST));

				//Se pune in coada pachetul
				queue_enq(q, &temp);
				continue;
			}

			//Forward packet
			get_interface_mac(best_route->interface, eth_hdr->ether_shost);
			memcpy(eth_hdr->ether_dhost, arp->mac, sizeof(arp->mac));
			send_packet(best_route->interface, &m);
		}
		else
		{
			//Verificare primire pachet ARP
			struct arp_header *arp_hdr = parse_arp(m.payload);
			if (arp_hdr != NULL)
			{
				//ARP REQUEST
				if (ntohs(arp_hdr->op) == ARPOP_REQUEST)
				{
					//Daca pachetul este destinat router-ului
					if (inet_addr(get_interface_ip(m.interface)) == arp_hdr->tpa)
					{
						//Se trimite ARP REPLY
						memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
						get_interface_mac(m.interface, eth_hdr->ether_shost);
						send_arp(arp_hdr->spa, arp_hdr->tpa, eth_hdr, m.interface, htons(ARPOP_REPLY));
						continue;
					}
				}

				//ARP REPLY
				if (ntohs(arp_hdr->op) == ARPOP_REPLY)
				{
					//Se adauga in tabela ARP o noua intrare
					add_arp_entry(arp_hdr->spa, arp_hdr->sha);

					//Se trmit pachetele din coada
					while (!queue_empty(q))
					{
						packet *temp = (packet *)queue_deq(q);
						struct ether_header *eth_hdr1 = (struct ether_header *)temp->payload;
						get_interface_mac(temp->interface, eth_hdr1->ether_shost);
						memcpy(eth_hdr1->ether_dhost, arp_hdr->sha, 6);
						send_packet(temp->interface, temp);
					}

					continue;
				}
			}
		}
	}
}
