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
#include <string.h>
#include "parson.h"
#include <ctype.h>

#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
#define PORT 8080

char *message;
char *response;
int sockfd;
char *cookie, *token;
char* host = "ec2-3-8-116-10.eu-west-2.compute.amazonaws.com";

static int getLine (char *prmpt, char *buff, size_t sz) {
    int ch, extra;

    if (prmpt != NULL) {
        printf ("%s", prmpt);
        fflush (stdout);
    }

    if (fgets (buff, sz, stdin) == NULL)
        return NO_INPUT;

    if (buff[strlen(buff)-1] != '\n') {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? TOO_LONG : OK;
    }

    buff[strlen(buff)-1] = '\0';
    return OK;
}

int isNumeric(const char *s) {
    if(s == NULL || *s == '\0' ||  isspace(*s))
        return 0;
    char *p;
    strtod(s, &p);
    return *p == '\0';

}

void register_func() {
    char username[100];
    char password[100];
    int  sockfd;

    getLine ("username=", username, sizeof(username));
    getLine ("password=", password, sizeof(password));

    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    char* serialized_string = NULL;
    json_object_set_string(root_object, "username", username);
    json_object_set_string(root_object, "password", password);
    serialized_string = json_serialize_to_string_pretty(root_value);
    sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
    message = compute_post_request(host, "/api/v1/tema/auth/register", "application/json", &serialized_string, 1, NULL, 0);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    if(strstr(response, "error") != NULL) {
        printf("The username %s is taken!\n", username);
    }
    else {
        printf("User %s successfully created!\n", username);
    }

    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
    close_connection(sockfd);
}

void login() {
    char username[100];
    char password[100];
    char substring[500];
    char *ret, *copy;
    int sockfd;

    if(cookie != NULL) {
        printf("Already logged in!\n");
    } else {

        getLine ("username=", username, sizeof(username));
        getLine ("password=", password, sizeof(password));

        JSON_Value *root_value = json_value_init_object();
        JSON_Object *root_object = json_value_get_object(root_value);
        char* serialized_string = NULL;
        json_object_set_string(root_object, "username", username);
        json_object_set_string(root_object, "password", password);
        serialized_string = json_serialize_to_string_pretty(root_value);
        sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
        message = compute_post_request(host, "/api/v1/tema/auth/login", "application/json", &serialized_string, 1, NULL, 0);
        send_to_server(sockfd, message);
        response = receive_from_server(sockfd);
        if(strstr(response, "No account with this username!") != NULL) {
            printf("No account with this username!\n");
        }
        else if(strstr(response, "Credentials are not good!") != NULL) {
            printf("Credentials are not good!\n");
        } else {
            printf("User succesfully logged in!\n");
            ret = strstr(response, "connect.sid");
            strcpy(substring, ret);
            copy = strtok(substring, ";");
            cookie = strdup(copy);
        }

        json_free_serialized_string(serialized_string);
        json_value_free(root_value);

        close_connection(sockfd);
    }
}

void logout() {
    int sockfd;

    sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
    message = compute_get_request(host, "/api/v1/tema/auth/logout", NULL, &cookie, 1);
    send_to_server(sockfd, message);
    message = receive_from_server(sockfd);
    if(strstr(message, "You are not logged in!") != NULL) {
        printf("You are not logged in!\n");
    }
    else {
        printf("Logout successfully!\n");
        token = NULL;
        cookie = NULL;
        free(token);
        free(cookie);
    }
    close_connection(sockfd);
}

void enter_library() {
    char substring[500];
    int sockfd;
    char *ret;

    sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
    message = compute_get_request(host, "/api/v1/tema/library/access", NULL, &cookie, 1);
    send_to_server(sockfd, message);
    message = receive_from_server(sockfd);
    if(strstr(response, "You are not logged in!") != NULL || cookie == NULL) {
        printf("You are not logged in!\n");
    }
    else {
        printf("Access granted!\n");
        ret = strstr(message, "token");
        strcpy(substring, ret);
        char *copy = substring + 8;
        copy[strlen(copy)-2] = 0;
        token = strdup(copy);
    }

    close_connection(sockfd);
}

void get_books() {
    int sockfd;
    char *ret, *err;

    sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
    message = compute_get_request_with_token(host, "/api/v1/tema/library/books", NULL, &cookie, 1, token);
    send_to_server(sockfd, message);
    message = receive_from_server(sockfd);
    err = strstr(message, "error");
    if(err != NULL) {
        printf("Error when decoding token!\n");
    }
    else {
        ret = strstr(message, "[");
        printf("%s\n", ret);
    }
    close_connection(sockfd);

}

void get_book() {
    char book_id[100];
    int sockfd;
    char *ret;

    if(token == NULL) {
        printf("Error: You may not have access!\n");
    } else {
        getLine ("id=", book_id, sizeof(book_id));
        if(isNumeric(book_id) == 0) {
            printf("Invalid data!\n");
        }
        else {
            char url[45] = "/api/v1/tema/library/books/";
            strcat(url, book_id);
            sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request_with_token(host, url, NULL, &cookie, 1, token);
            send_to_server(sockfd, message);
            message = receive_from_server(sockfd);
            if(strstr(message,"No book was found!") != NULL) {
                printf("No book was found!\n");
            }
            else {
                ret = strstr(message, "[");
                printf("%s\n", ret);
            }

            close_connection(sockfd);
        }

    }
}

void add_book() {
    char title[100];
    char author[100];
    char genre[100];
    char page_count[100];
    int pg_count;
    char publisher[100];
    int sockfd;

    if(token == NULL) {
        printf("Error: You may not have access!\n");
    }
    else {
        getLine ("title=", title, sizeof(title));
        getLine ("author=", author, sizeof(author));
        getLine ("genre=", genre, sizeof(genre));
        getLine ("page_Count=", page_count, sizeof(page_count));
        getLine ("publisher=", publisher, sizeof(publisher));
        if(isNumeric(page_count) == 0) {
            printf("Invalid data!\n");
        }
        else {
            pg_count = atoi(page_count);
            if(pg_count < 0) {
                printf("Invalid data!\n");
            }
            else {
                JSON_Value *root_value = json_value_init_object();
                JSON_Object *root_object = json_value_get_object(root_value);
                char* serialized_string = NULL;
                json_object_set_string(root_object, "title", title);
                json_object_set_string(root_object, "author", author);
                json_object_set_string(root_object, "genre", genre);
                json_object_set_number(root_object, "page_count", pg_count);
                json_object_set_string(root_object, "publisher", publisher);
                serialized_string = json_serialize_to_string_pretty(root_value);
                sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
                if(token == NULL) {
                    printf("Error: no access!\n");
                }
                else {
                    message = compute_post_request_with_token(host, "/api/v1/tema/library/books", "application/json", &serialized_string, 1, NULL, 0, token);
                    send_to_server(sockfd, message);
                    response = receive_from_server(sockfd);
                    printf("Book successfully added!\n");
                }
                json_free_serialized_string(serialized_string);
                json_value_free(root_value);

                close_connection(sockfd);
            }
        }
    }
}

void delete_book() {
    char book_id[100];
    int sockfd;
    if(token == NULL) {
        printf("Error: You may not have access!\n");
    } else {
        getLine ("id=", book_id, sizeof(book_id));
        if(isNumeric(book_id) == 0) {
            printf("Invalid data!\n");
        }
        else {
            char url[45] = "/api/v1/tema/library/books/";
            strcat(url, book_id);
            sockfd = open_connection("3.8.116.10", PORT, AF_INET, SOCK_STREAM, 0);
            message = compute_delete_request_with_token(host, url, NULL, &cookie, 1, token);
            send_to_server(sockfd, message);
            message = receive_from_server(sockfd);
            if(strstr(message,"No book was deleted!") != NULL) {
                printf("No book was deleted!\n");
            }
            else {
                printf("Book successfully deleted!\n");
            }

            close_connection(sockfd);
        }
    }
}

int main(int argc, char *argv[]) {

    while (1) {
        char buff[100];

        getLine ("", buff, sizeof(buff));

        if(strcmp(buff, "register") == 0) {
            register_func();
        }
        if(strcmp(buff, "login") == 0) {
            login();
        }
        if(strcmp(buff, "enter_library") == 0) {
            enter_library();
        }
        if(strcmp(buff, "get_books") == 0) {
            get_books();
        }
        if(strcmp(buff, "get_book") == 0) {
            get_book();
        }
        if(strcmp(buff, "add_book") == 0) {
            add_book();
        }
        if(strcmp(buff, "delete_book") == 0) {
            delete_book();
        }
        if(strcmp(buff, "logout") == 0) {
            logout();
        }
        if(strcmp(buff, "exit") == 0) {
            return 0;
        }
    }
    free(message);
    free(response);
    free(cookie);
    free(host);
    free(token);
    return 0;
}
