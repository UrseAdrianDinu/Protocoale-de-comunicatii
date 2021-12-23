#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.c"
#define HOST "34.118.48.238"
#define PORT "8080"
#define REGISTER "/api/v1/tema/auth/register"
#define LOGIN "/api/v1/tema/auth/login"
#define LOGOUT "/api/v1/tema/auth/logout"
#define ENTER "/api/v1/tema/library/access"
#define BOOKS "/api/v1/tema/library/books"

// Functie care verifica daca un string este un numar
int checkInput(char *s)
{
    if (s[0] == '0' && strlen(s) != 1)
        return 0;
    for (int i = 0; i < strlen(s); i++)
    {
        if (!isdigit(s[i]))
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    char **cookies = NULL;
    char *token = NULL;
    int sockfd;
    char username[BUFLEN];
    char password[BUFLEN];
    char buffer[BUFLEN];
    int online = 0;

    sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

    while (1)
    {
        // Citire comanda de la tastatura
        fgets(buffer, LINELEN, stdin);
        buffer[strlen(buffer) - 1] = '\0';

        // Comanda register
        if (strncmp(buffer, "register", 8) == 0)
        {
            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Citire username si password
            printf("username=");
            fgets(username, LINELEN, stdin);
            username[strlen(username) - 1] = '\0';
            printf("password=");
            fgets(password, LINELEN, stdin);
            password[strlen(password) - 1] = '\0';

            // Creare string json
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);
            serialized_string = json_serialize_to_string_pretty(root_value);

            // Creare cerere de tip POST
            message = compute_post_request(HOST, REGISTER, "application/json", serialized_string, NULL, 0, NULL);

            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Verific daca raspunsul contine un mesaj de eroare
            // Extrag stringul json
            
            char *error = basic_extract_json_response(response);

            // Daca nu exista un string json in raspuns, inseamna
            // ca inregistrarea a fost facuta cu succes
            if (error == NULL)
            {
                printf("You have successfully registered.\n");
            }
            else
            {
                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                JSON_Value *root_value = json_parse_string(error - 1);
                JSON_Object *root_object = json_value_get_object(root_value);
                printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
            }

            // Eliberare memorie
            json_free_serialized_string(serialized_string);
            json_value_free(root_value);
        }

        // Comanda login
        if (strncmp(buffer, "login", 5) == 0)
        {
            // Verific daca clientul este deja logat,
            // in acest caz afisez mesajul
            // "You are already logged in."
            if (online == 1)
            {
                printf("You are already logged in.\n");
                continue;
            }
            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Citire username si password
            printf("username=");
            fgets(username, LINELEN, stdin);
            username[strlen(username) - 1] = '\0';
            printf("password=");
            fgets(password, LINELEN, stdin);
            password[strlen(password) - 1] = '\0';

            // Creare string json
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);
            serialized_string = json_serialize_to_string_pretty(root_value);

            // Creare cerere de tip POST
            message = compute_post_request(HOST, LOGIN, "application/json", serialized_string, NULL, 0, NULL);

            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Verific daca raspunsul contine un mesaj de eroare
            // Extrag stringul json
            char *error = basic_extract_json_response(response);

            // Daca nu exista un string json in raspuns, inseamna
            // ca autentificarea a fost facuta cu succes
            if (error == NULL)
            {
                printf("You have successfully logged in.\n");

                // Extragere cookie din raspunsul primit
                char *start = strstr(response, "Set-Cookie:");
                char *p = strtok(start + 12, ";");
                cookies = malloc(sizeof(char *));
                cookies[0] = malloc((strlen(p) + 1) * sizeof(char));
                memcpy(cookies[0], start + 12, strlen(p));
                online = 1;
            }
            else
            {
                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                JSON_Value *root = json_parse_string(error - 1);
                JSON_Object *object = json_value_get_object(root);
                printf("ERROR: %s\n", json_object_get_string(object, "error"));
            }

            // Eliberare memorie
            json_free_serialized_string(serialized_string);
            json_value_free(root_value);
        }

        if (strncmp(buffer, "enter_library", 13) == 0)
        {
            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Creare cerere de tip GET
            message = compute_get_request(HOST, ENTER, NULL, cookies, 1, NULL);

            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Extrag stringul json din mesajul primit
            char *start = basic_extract_json_response(response);

            // Mesajul primit de la server contine un string json eroare
            // Parsez stringul json, extrag eroarea si
            // o afisez
            if (strstr(start, "error") != NULL)
            {
                JSON_Value *root_value = json_parse_string(start - 1);
                JSON_Object *root_object = json_value_get_object(root_value);
                printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
            }
            else
            {
                // Daca mesajul nu contine un string json eroare,
                // atunci intrarea in bibilioteca a fost facuta cu succes
                printf("You have entered the library.\n");

                // Extrag token-ul din mesajul primit
                char *p = strtok(start + 10, "}");
                token = malloc((strlen(p) + 1) * sizeof(char));
                memcpy(token, start + 10, strlen(p) - 1);
            }
        }

        if (strcmp(buffer, "get_books") == 0)
        {
            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Creare cerere de tip GET
            message = compute_get_request(HOST, BOOKS, NULL, cookies, 1, token);

            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Extrag stringul json din mesajul primit
            char *start = basic_extract_json_response(response);

            // Daca este NULL, atunci nu sunt carti in bibilioteca
            if (start == NULL)
            {
                printf("No books.\n");
            }
            else
            {
                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                if (strstr(start, "error") != NULL)
                {
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Object *root_object = json_value_get_object(root_value);
                    printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
                }
                else
                {
                    // Parsez stringul json, extrag id-ul si titlul
                    // fiecarei carti si le afisez
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Array *arr = json_value_get_array(root_value);
                    int count = json_array_get_count(arr);
                    printf("-----------------------------------\n");
                    for (int i = 0; i < count; i++)
                    {
                        JSON_Object *object = json_array_get_object(arr, i);
                        printf("Id: %.0f\n", json_object_get_number(object, "id"));
                        printf("Title: %s\n", json_object_get_string(object, "title"));
                        printf("-----------------------------------\n");
                    }
                }
            }
        }

        // Comanda get_book
        if (strcmp(buffer, "get_book") == 0)
        {
            // Citire id
            printf("id=");
            scanf("%s", buffer);

            // Verific daca id-ul citit are un format valid
            if (checkInput(buffer) != 1)
            {
                printf("Id invalid format.\n");
            }
            else
            {
                sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

                // Construiesc url-ul corespunzator id-ului citit
                char s[BUFLEN];
                int id = atoi(buffer);
                sprintf(s, "/api/v1/tema/library/books/%d", id);

                // Creare cerere de tip GET
                message = compute_get_request(HOST, s, NULL, cookies, 1, token);

                // Trimitere mesaj catre server
                send_to_server(sockfd, message);

                // Primire mesaj de la server
                response = receive_from_server(sockfd);

                // Extrag stringul json din mesajul primit
                char *start = basic_extract_json_response(response);

                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                if (strstr(start, "error") != NULL)
                {
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Object *root_object = json_value_get_object(root_value);
                    printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
                }
                else
                {
                    // Parsez stringul json, extrag titlul, autorul,
                    // publisher, genul, page count si le afisez
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Array *arr = json_value_get_array(root_value);
                    JSON_Object *object = json_array_get_object(arr, 0);
                    printf("-----------------------------------\n");
                    printf("Title: %s\n", json_object_get_string(object, "title"));
                    printf("Author: %s\n", json_object_get_string(object, "author"));
                    printf("Publisher: %s\n", json_object_get_string(object, "publisher"));
                    printf("Genre: %s\n", json_object_get_string(object, "genre"));
                    printf("Page count: %.0f\n", json_object_get_number(object, "page_count"));
                    printf("-----------------------------------\n");
                }
            }
        }

        if (strncmp(buffer, "add_book", 8) == 0)
        {
            char title[LINELEN];
            char author[LINELEN];
            char genre[LINELEN];
            char page_count_string[LINELEN];
            int page_count;
            char publisher[LINELEN];

            // Citire titlu, autor, gen, publisher, page_count
            printf("title=");
            fgets(title, LINELEN, stdin);
            title[strlen(title) - 1] = '\0';

            printf("author=");
            fgets(author, LINELEN, stdin);
            author[strlen(author) - 1] = '\0';

            printf("genre=");
            fgets(genre, LINELEN, stdin);
            genre[strlen(genre) - 1] = '\0';

            printf("publisher=");
            fgets(publisher, LINELEN, stdin);
            publisher[strlen(publisher) - 1] = '\0';

            printf("page_count=");
            fgets(page_count_string, LINELEN, stdin);
            page_count_string[strlen(page_count_string) - 1] = '\0';
            page_count = atoi(page_count_string);

            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Creare string json
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;
            json_object_set_string(root_object, "title", title);
            json_object_set_string(root_object, "author", author);
            json_object_set_string(root_object, "genre", genre);

            // Verific daca numarul de pagini este valid
            // atunci adaug in obiectul json un numar
            // Daca nu, adaug un string pentru a primi
            // un mesaj de eroare de la server. (format invalid)
            if (checkInput(page_count_string) == 1)
            {
                json_object_set_number(root_object, "page_count", page_count);
            }
            else
            {
                json_object_set_string(root_object, "page_count", page_count_string);
            }

            json_object_set_string(root_object, "publisher", publisher);
            serialized_string = json_serialize_to_string_pretty(root_value);

            // Creare cerere de tip POST
            message = compute_post_request(HOST, BOOKS, "application/json", serialized_string, cookies, 1, token);
            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Extrag stringul json din mesajul primit
            char *start = basic_extract_json_response(response);

            if (start != NULL)
            {
                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                if (strstr(start, "error") != NULL)
                {
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Object *root_object = json_value_get_object(root_value);
                    printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
                }
            }
            else
            {
                // Verific daca raspunsul este de tip Too many requests
                if (strstr(response, "Too many requests") != NULL)
                {
                    printf("Too many requests, please try again later.\n");
                }
                else
                {
                    // Adaugarea cartii a fost facuta cu succes
                    printf("You have successfully added the book.\n");
                }
            }
        }

        // Comanda delete_book
        if (strcmp(buffer, "delete_book") == 0)
        {
            // Citire id
            printf("id=");
            scanf("%s", buffer);

            // Verific daca id-ul citit are un format valid
            if (checkInput(buffer) != 1)
            {
                printf("Id invalid format.\n");
            }
            else
            {
                sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

                // Construiesc url-ul corespunzator id-ului citit
                char s[BUFLEN];
                int id = atoi(buffer);
                sprintf(s, "/api/v1/tema/library/books/%d", id);

                // Creare cerere de tip DELETE
                message = compute_delete_request(HOST, s, NULL, cookies, 1, token);

                // Trimitere mesaj catre server
                send_to_server(sockfd, message);

                // Primire mesaj de la server
                response = receive_from_server(sockfd);

                // Extrag stringul json din mesajul primit
                char *start = basic_extract_json_response(response);

                if (start != NULL)
                {
                    // Mesajul primit de la server contine un string json eroare
                    // Parsez stringul json, extrag eroarea si
                    // o afisez
                    if (strstr(start, "error") != NULL)
                    {
                        JSON_Value *root_value = json_parse_string(start - 1);
                        JSON_Object *root_object = json_value_get_object(root_value);
                        printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
                    }
                }
                else
                {
                    // Verific daca raspunsul este de tip Too many requests
                    if (strstr(response, "Too many requests") != NULL)
                    {
                        printf("Too many requests, please try again later.\n");
                    }
                    else
                    {
                        // Stergerea cartii a fost facuta cu succes
                        printf("You have successfully deleted the book.\n");
                    }
                }
            }
        }

        // Comanda logout
        if (strcmp(buffer, "logout") == 0)
        {
            sockfd = open_connection(HOST, 8080, AF_INET, SOCK_STREAM, 0);

            // Creare cerere de tip GET
            message = compute_get_request(HOST, LOGOUT, NULL, cookies, 1, token);

            // Trimitere mesaj catre server
            send_to_server(sockfd, message);

            // Primire mesaj de la server
            response = receive_from_server(sockfd);

            // Extrag stringul json din mesajul primit
            char *start = basic_extract_json_response(response);

            if (start != NULL)
            {
                // Mesajul primit de la server contine un string json eroare
                // Parsez stringul json, extrag eroarea si
                // o afisez
                if (strstr(start, "error") != NULL)
                {
                    JSON_Value *root_value = json_parse_string(start - 1);
                    JSON_Object *root_object = json_value_get_object(root_value);
                    printf("ERROR: %s\n", json_object_get_string(root_object, "error"));
                }
            }
            else
            {
                // Delogarea a fost facuta cu succes
                printf("Logged out.\n");
                online = 0;
                token = NULL;
                cookies = NULL;
                close_connection(sockfd);
            }
        }

        // Comanda exit
        if (strncmp(buffer, "exit", 4) == 0)
        {
            break;
        }
    }

    // Eliberarea memoriei
    if (cookies != NULL)
    {
        free(cookies[0]);
        free(cookies);
    }
    if (token != NULL)
    {
        free(token);
    }
    if (message != NULL && response != NULL)
    {
        free(response);
        free(message);
    }

    // Inchiderea conexiunii
    close_connection(sockfd);
    return 0;
}
