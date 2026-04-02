#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#define PORT 2908

#define SALT_SIZE 16
#define HASH_SIZE 32
#define ITERATIONS 100000

extern int errno;  

typedef struct threadInfo
{
    int idThread;
	int cl;
} threadInfo;

pthread_mutex_t users_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct sesiuneClienti 
{
    int socket_fd;
    char username[50];
    int user_id;  
    int autentificat;
    char user_type[20];
} sesiuneClienti;

sesiuneClienti sesiuni_active[100];
int contor_sesiuni = 0;
pthread_mutex_t mutex_sesiune = PTHREAD_MUTEX_INITIALIZER;

void raspunde(void *arg);
static void *treat(void *arg);

void trim_string(char* str)
{
    int len = strlen(str);
    
    while(len > 0 && (str[len-1] == '\n'))
    {
        str[len-1] = '\0';
        len--;
    }
}

void log_activity(const char* username, const char* action, const char* details)
{
    time_t now;
    struct tm *timeinfo;
    char timestamp[80];
    char filename[100];
    
    time(&now);
    timeinfo = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    strftime(filename, sizeof(filename), "logs/server_%Y-%m-%d.log", timeinfo);
    
    struct stat st = {0};
    if (stat("logs", &st) == -1) 
    {
        mkdir("logs", 0755);
    }
    
    FILE *log_file = fopen(filename, "a");
    if(log_file)
    {
        if(username && strlen(username) > 0)
        {
            fprintf(log_file, "[%s] %-20s | %-30s | %s\n", timestamp, username, action, details ? details : "");
        }
        else
        {
            fprintf(log_file, "[%s] %-20s | %-30s | %s\n", timestamp, "SYSTEM", action, details ? details : "");
        }

        fclose(log_file);
    }
    
    fflush(stdout);
}

sqlite3* db_connect()   
{
    sqlite3 *db;
    int rc = sqlite3_open("virtualsoc.db", &db);
    
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Eroare la baza de date");
        sqlite3_close(db);
        return NULL;
    }

    return db; 
}

void bytes_to_hex(const unsigned char* bytes, int len, char* hex)   
{
    for(int i = 0; i < len; i++)
    {
        sprintf(hex + i*2, "%02x", bytes[i]);
    }
    
    hex[len*2] = '\0';
}

void hex_to_bytes(const char* hex, unsigned char* bytes, int len)  
{
    for(int i = 0; i < len; i++)
    {
        sscanf(hex + i*2, "%2hhx", &bytes[i]);
    }
}

char* hash_password(const char* password)
{
    static char result[256];
    unsigned char salt[SALT_SIZE];
    unsigned char hash[HASH_SIZE];
    char salt_hex[SALT_SIZE*2 + 1];
    char hash_hex[HASH_SIZE*2 + 1];
    
    if (!RAND_bytes(salt, SALT_SIZE))
    {
        fprintf(stderr, "Eroare la generare salt\n");
        return NULL;
    }
    
    if (!PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE, ITERATIONS, EVP_sha256(), HASH_SIZE, hash))
    {
        fprintf(stderr, "Eroare la hash parola\n");
        return NULL;
    }
    
    bytes_to_hex(salt, SALT_SIZE, salt_hex);
    bytes_to_hex(hash, HASH_SIZE, hash_hex);
    
    snprintf(result, sizeof(result), "$pbkdf2$%d$%s$%s", ITERATIONS, salt_hex, hash_hex);
    
    return result;
}

int verify_password(const char* password, const char* stored_hash)
{
    int iterations;
    char salt_hex[SALT_SIZE*2 + 1];
    char hash_hex[HASH_SIZE*2 + 1];
    unsigned char salt[SALT_SIZE];
    unsigned char stored_hash_bytes[HASH_SIZE];
    unsigned char computed_hash[HASH_SIZE];
    
    if (sscanf(stored_hash, "$pbkdf2$%d$%32[^$]$%64s", &iterations, salt_hex, hash_hex) != 3)
    {
        fprintf(stderr, "Format hash invalid\n");
        return -1;
    }
    
    hex_to_bytes(salt_hex, salt, SALT_SIZE);
    hex_to_bytes(hash_hex, stored_hash_bytes, HASH_SIZE);
    
    if (!PKCS5_PBKDF2_HMAC(password, strlen(password), salt, SALT_SIZE, iterations, EVP_sha256(), HASH_SIZE, computed_hash))
    {
        fprintf(stderr, "Eroare la calcul hash\n");
        return -1;
    }

    int result = 0;
    for(int i = 0; i < HASH_SIZE; i++)
    {
        result |= (computed_hash[i] ^ stored_hash_bytes[i]);
    }
    
    return (result == 0) ? 0 : -1;
}

sesiuneClienti* get_sesiune(int fd)
{
    pthread_mutex_lock(&mutex_sesiune);

    for(int i = 0; i < contor_sesiuni; i++)
    {
        if(sesiuni_active[i].socket_fd == fd)
        {
            pthread_mutex_unlock(&mutex_sesiune);
            return &sesiuni_active[i];
        } 
    }
    pthread_mutex_unlock(&mutex_sesiune);
    return NULL;
}

int add_sesiune(int fd, const char* username, int user_id, const char* user_type)
{
    pthread_mutex_lock(&mutex_sesiune);
    if(contor_sesiuni >= 100)
    {
        pthread_mutex_unlock(&mutex_sesiune);
        return -1;
    }
    
    sesiuni_active[contor_sesiuni].socket_fd = fd;
    strcpy(sesiuni_active[contor_sesiuni].username, username);
    sesiuni_active[contor_sesiuni].user_id = user_id;
    sesiuni_active[contor_sesiuni].autentificat = 1;
    strcpy(sesiuni_active[contor_sesiuni].user_type, user_type);
    contor_sesiuni++;

    pthread_mutex_unlock(&mutex_sesiune);
    return 0;
}

void remove_sesiune(int fd)
{
    pthread_mutex_lock(&mutex_sesiune);
    for(int i = 0; i < contor_sesiuni; i++)
    {
        if(sesiuni_active[i].socket_fd == fd)
        {
            sesiuni_active[i] = sesiuni_active[contor_sesiuni-1];
            contor_sesiuni--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_sesiune);
}

int este_autentificat(int fd)
{
    sesiuneClienti* s = get_sesiune(fd);
    return s != NULL && s->autentificat;
}

void register1(char raspuns[], char* username, char* password, char* type)
{
    log_activity(username, "REGISTER_ATTEMPT", username);

    char rasp[150];

    if(strlen(username) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Usernameul nu poate fi gol\n");
        log_activity(username, "REGISTER_FAILED", "Empty username");
        strcpy(raspuns, rasp);
        return;
    }
    for(int i = 0; i < strlen(username); i++)
    {
        if(username[i] == ' ')
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Usernameul nu poate contine spatii\n");
            strcpy(raspuns, rasp);
            log_activity(username, "REGISTER_FAILED", "Wrong username");
            return;
        }
    }
    sqlite3 *conn = db_connect();
    if (!conn) 
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(username, "REGISTER_FAILED", "Database error");
    }
    else
    {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username=?", -1, &stmt, NULL);
        
        if (rc != SQLITE_OK) 
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Comanda SELECT in baza de date failed\n");
            log_activity(username, "REGISTER_FAILED", "Database error");
        }
        else
        {
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            
            if (rc == SQLITE_ROW) 
            {
                snprintf(rasp, sizeof(rasp), "ERROR|User %s already exists\n", username);
                log_activity(username, "REGISTER_FAILED", "Username already exists");
            }
            else
            {
                sqlite3_finalize(stmt);
                char* hashed = hash_password(password);
                
                if(!hashed)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Criptare parola failed\n");
                    log_activity(username, "REGISTER_FAILED", "ECriprae parola failed");
                    sqlite3_close(conn);
                    strcpy(raspuns, rasp);
                    return;
                }
                const char *user_type = strcmp(type, "ADMIN") == 0 ? "admin" : "user";
                rc = sqlite3_prepare_v2(conn, "INSERT INTO users (username, password, user_type) VALUES (?,?,?)", -1, &stmt, NULL);
                
                if (rc != SQLITE_OK) 
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Register failed\n");
                    log_activity(username, "REGISTER_FAILED", "Database error");
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, hashed, -1, SQLITE_STATIC);  
                    sqlite3_bind_text(stmt, 3, user_type, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
                    
                    if (rc != SQLITE_DONE) 
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Register failed\n");
                        log_activity(username, "REGISTER_FAILED", "Database error");
                    } 
                    else 
                    {
                        snprintf(rasp, sizeof(rasp), "SUCCESS|User %s registered\n", username);
                        char log_msg[100];
                        snprintf(log_msg, sizeof(log_msg), "Type: %s", type);
                        log_activity(username, "REGISTER_SUCCESS", log_msg);
                        printf("User %s inregistrat cu parola criptata (PBKDF2-SHA256, %d iterations)\n", username, ITERATIONS);
                    }
                }
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_close(conn);
    }

    strcpy(raspuns, rasp);
}

void login(int socket, const char* username, const char* password, char raspuns[])
{
    char rasp[150];
    log_activity(username, "LOGIN_ATTEMPT", username);
    
    if(este_autentificat(socket))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sunteti deja autentificat\n");
        log_activity(username, "LOGIN_FAILED", "Already authenticated");
    }
    else
    {
        sqlite3 *conn = db_connect();
        
        if (!conn)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
            log_activity(username, "LOGIN_FAILED", "Database error");
        }
        else
        {
            sqlite3_stmt *stmt;
            int rc = sqlite3_prepare_v2(conn, "SELECT id, username, user_type, password, banned FROM users WHERE username = ?", -1, &stmt, NULL);
            
            if(rc != SQLITE_OK)
            {
                fprintf(stderr, "Eroare la interogare sql\n");
                snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                log_activity(username, "LOGIN_FAILED", "Database error");
                sqlite3_close(conn);
            }
            else
            {
                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                rc = sqlite3_step(stmt);
                
                if(rc != SQLITE_ROW)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Utilizator inexistent\n");
                    log_activity(username, "LOGIN_FAILED", "User not found");
                    sqlite3_finalize(stmt);
                    sqlite3_close(conn);
                }
                else
                {
                    int user_id = sqlite3_column_int(stmt, 0);
                    const char *db_username = (const char*)sqlite3_column_text(stmt, 1);
                    const char *user_type   = (const char*)sqlite3_column_text(stmt, 2);
                    const char *db_password_hash = (const char*)sqlite3_column_text(stmt, 3);  
                    int banned = sqlite3_column_int(stmt, 4);
                    
                    if (verify_password(password, db_password_hash) == 0)  
                    {
                        if(banned)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Contul tau a fost banat de un administrator\n");
                            log_activity(username, "LOGIN_FAILED", "User is banned");
                            printf("User banat a incercat sa se autentifice: %s\n", username);
                        }
                        else
                        {
                            add_sesiune(socket, db_username, user_id, user_type);
                            snprintf(rasp, sizeof(rasp), "SUCCESS|Autentificat: %s : %s\n", db_username, user_type);
                            printf("User %s autentificat de tip: %s\n", db_username, user_type);
                            char log_msg[100];
                            snprintf(log_msg, sizeof(log_msg), "Type: %s | Socket: %d", user_type, socket);
                            log_activity(username, "LOGIN_SUCCESS", log_msg);
                        }
                    }
                    else
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Parola incorecta\n");
                        log_activity(username, "LOGIN_FAILED", "Wrong password");
                        printf("login failed pentru %s: parola gresita\n", username);
                    }
                    sqlite3_finalize(stmt);
                    sqlite3_close(conn);
                }
            }
        }
    }

    strcpy(raspuns, rasp);
}

void set_profile(int fd, char raspuns[], char* type)
{
    char rasp[150];
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "SET_PROFILE_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
        
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "SET_PROFILE_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "SET_PROFILE_ATTEMPT", type);

            if(strcmp(type, "PUBLIC") != 0 && strcmp(type, "PRIVATE") != 0)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Tip profil invalid (PUBLIC sau PRIVATE)\n");
                log_activity(sesiune->username, "SET_PROFILE_FAILED", "Invalid type");
            }
            else
            {
                sqlite3 *conn = db_connect();
                
                if (!conn)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                    log_activity(sesiune->username, "SET_PROFILE_FAILED", "Database error");
                }
                else
                {
                    int tip = (strcmp(type, "PUBLIC") == 0) ? 1 : 0;
                    sqlite3_stmt *stmt;
                    int rc = sqlite3_prepare_v2(conn, "UPDATE users SET profile_public=? WHERE id=?", -1, &stmt, NULL);
                    
                    if(rc != SQLITE_OK)
                    {
                        fprintf(stderr, "Eroare la interogare");
                        snprintf(rasp, sizeof(rasp), "ERROR|Actualizare profil failed\n");
                        log_activity(sesiune->username, "SET_PROFILE_FAILED", "Database error");
                    }
                    else
                    {
                        sqlite3_bind_int(stmt, 1, tip);
                        sqlite3_bind_int(stmt, 2, sesiune->user_id);
                        rc = sqlite3_step(stmt);
                        
                        if(rc != SQLITE_DONE)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Actualizare profil failed\n");
                            log_activity(sesiune->username, "SET_PROFILE_FAILED", "Database step error");
                        }
                        else
                        {
                            snprintf(rasp, sizeof(rasp), "SUCCESS|Profil setat ca %s pentru utilizatorul %s\n", type, sesiune->username);
                            log_activity(sesiune->username, "SET_PROFILE_SUCCESS", type);
                            printf("User %s a schimbat profilul in %s\n", sesiune->username, type);
                        }
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(conn);
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void add_friend(int fd, char raspuns[], char* friend_username, char* friend_type)
{
    char rasp[2000];
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "ADD_FRIEND_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
        
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "ADD_FRIEND_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "ADD_FRIEND_ATTEMPT", friend_username);

            if(strcmp(friend_type, "FRIEND") != 0 && strcmp(friend_type, "CLOSE_FRIEND") != 0)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Tip relatie invalid (FRIEND sau CLOSE_FRIEND)\n");
                log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Invalid relation type");
            }
            else
            {
                sqlite3 *conn = db_connect();
                
                if (!conn)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                    log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                }
                else
                {
                    sqlite3_stmt *stmt;
                    int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username=?", -1, &stmt, NULL);
                    
                    if(rc != SQLITE_OK)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                        log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                        sqlite3_close(conn);
                    }
                    else
                    {
                        sqlite3_bind_text(stmt, 1, friend_username, -1, SQLITE_STATIC);
                        rc = sqlite3_step(stmt);
                        
                        if(rc != SQLITE_ROW)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", friend_username);
                            log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Friend user not found");
                            sqlite3_finalize(stmt);
                            sqlite3_close(conn);
                        }
                        else
                        {
                            int friend_id = sqlite3_column_int(stmt, 0);
                            sqlite3_finalize(stmt);
                            
                            if(friend_id == sesiune->user_id)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Nu puteti sa va adaugati pe voi insiva ca prieten\n");
                                log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Cannot add self");
                                sqlite3_close(conn);
                            }
                            else
                            {
                                const char *db_friend_type = (strcmp(friend_type, "CLOSE_FRIEND") == 0) ? "close_friend" : "friend";
                                rc = sqlite3_prepare_v2(conn, "SELECT friend_type FROM relations WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                                
                                if(rc != SQLITE_OK)
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Verificare relatie failed\n");
                                    log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                                    sqlite3_close(conn);
                                }
                                else
                                {
                                    sqlite3_bind_int(stmt, 1, sesiune->user_id);
                                    sqlite3_bind_int(stmt, 2, friend_id);
                                    rc = sqlite3_step(stmt);
                                    
                                    if(rc == SQLITE_ROW)
                                    {
                                        sqlite3_finalize(stmt);
                                        rc = sqlite3_prepare_v2(conn, "UPDATE relations SET friend_type=? WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                                        
                                        if(rc != SQLITE_OK)
                                        {
                                            snprintf(rasp, sizeof(rasp), "ERROR|Actualizare relatie failed\n");
                                            log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                                            sqlite3_close(conn);
                                        }
                                        else
                                        {
                                            sqlite3_bind_text(stmt, 1, db_friend_type, -1, SQLITE_STATIC);
                                            sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                            sqlite3_bind_int(stmt, 3, friend_id);
                                            rc = sqlite3_step(stmt);
                                            sqlite3_finalize(stmt);
                                            rc = sqlite3_prepare_v2(conn, "UPDATE relations SET friend_type=? WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                                            
                                            if(rc == SQLITE_OK)
                                            {
                                                sqlite3_bind_text(stmt, 1, db_friend_type, -1, SQLITE_STATIC);
                                                sqlite3_bind_int(stmt, 2, friend_id);
                                                sqlite3_bind_int(stmt, 3, sesiune->user_id);
                                                sqlite3_step(stmt);
                                                sqlite3_finalize(stmt);
                                            }
                                            snprintf(rasp, sizeof(rasp), "SUCCESS|Relatie cu %s actualizata la %s\n", friend_username, friend_type);
                                            log_activity(sesiune->username, "ADD_FRIEND_SUCCESS", friend_username);
                                            printf("User %s a actualizat relatia cu %s la %s\n", sesiune->username, friend_username, friend_type);
                                            sqlite3_close(conn);
                                        }
                                    }
                                    else
                                    {
                                        sqlite3_finalize(stmt);
                                        rc = sqlite3_prepare_v2(conn, "INSERT INTO relations (user_id, friend_id, friend_type) VALUES (?,?,?)", -1, &stmt, NULL);
                                        
                                        if(rc != SQLITE_OK)
                                        {
                                            snprintf(rasp, sizeof(rasp), "ERROR|Adaugare prieten failed\n");
                                            log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                                            sqlite3_close(conn);
                                        }
                                        else
                                        {
                                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                                            sqlite3_bind_int(stmt, 2, friend_id);
                                            sqlite3_bind_text(stmt, 3, db_friend_type, -1, SQLITE_STATIC);
                                            rc = sqlite3_step(stmt);
                                            sqlite3_finalize(stmt);
                                            
                                            if(rc != SQLITE_DONE)
                                            {
                                                snprintf(rasp, sizeof(rasp), "ERROR|Adaugare prieten failed\n");
                                                log_activity(sesiune->username, "ADD_FRIEND_FAILED", "Database error");
                                                sqlite3_close(conn);
                                            }
                                            else
                                            {
                                                rc = sqlite3_prepare_v2(conn, "INSERT INTO relations (user_id, friend_id, friend_type) VALUES (?,?,?)", -1, &stmt, NULL);
                                                
                                                if(rc == SQLITE_OK)
                                                {
                                                    sqlite3_bind_int(stmt, 1, friend_id);
                                                    sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                                    sqlite3_bind_text(stmt, 3, db_friend_type, -1, SQLITE_STATIC);
                                                    sqlite3_step(stmt);
                                                    sqlite3_finalize(stmt);
                                                }
                                                snprintf(rasp, sizeof(rasp), "SUCCESS|%s adaugat ca %s\n", friend_username, friend_type);
                                                log_activity(sesiune->username, "ADD_FRIEND_SUCCESS", friend_username);
                                                printf("User %s a adaugat pe %s ca %s\n", sesiune->username, friend_username, friend_type);
                                                sqlite3_close(conn);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void delete_friend(int fd, char raspuns[], char* friend_username)
{
    char rasp[2000];
    
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "DELETE_FRIEND_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
        
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "DELETE_FRIEND_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "DELETE_FRIEND_ATTEMPT", friend_username);

            sqlite3 *conn = db_connect();
            
            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username=?", -1, &stmt, NULL);
                
                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, friend_username, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
                    
                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", friend_username);
                        log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Friend user not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int friend_id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);
                        rc = sqlite3_prepare_v2(conn, "SELECT friend_type FROM relations WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                        
                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Verificare relatie failed\n");
                            log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Database error");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_int(stmt, 2, friend_id);
                            rc = sqlite3_step(stmt);
                            
                            if(rc != SQLITE_ROW)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|%s nu este in lista ta de prieteni\n", friend_username);
                                log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Not in friend list");
                                sqlite3_finalize(stmt);
                                sqlite3_close(conn);
                            }
                            else
                            {
                                const char *friend_type = (const char*)sqlite3_column_text(stmt, 0);
                                char friend_type_display[20];
                                strcpy(friend_type_display, strcmp(friend_type, "close_friend") == 0 ? "CLOSE_FRIEND" : "FRIEND");
                                sqlite3_finalize(stmt);
                                rc = sqlite3_prepare_v2(conn, "DELETE FROM relations WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                            
                                if(rc != SQLITE_OK)
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Stergere prieten failed\n");
                                    log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Database error");
                                    sqlite3_close(conn);
                                }
                                else
                                {
                                    sqlite3_bind_int(stmt, 1, sesiune->user_id);
                                    sqlite3_bind_int(stmt, 2, friend_id);
                                    rc = sqlite3_step(stmt);
                                    sqlite3_finalize(stmt);
                            
                                    if(rc != SQLITE_DONE)
                                    {
                                        snprintf(rasp, sizeof(rasp), "ERROR|Stergere prieten failed\n");
                                        log_activity(sesiune->username, "DELETE_FRIEND_FAILED", "Database step error");
                                        sqlite3_close(conn);
                                    }
                                    else
                                    {
                                        rc = sqlite3_prepare_v2(conn, "DELETE FROM relations WHERE user_id=? AND friend_id=?", -1, &stmt, NULL);
                            
                                        if(rc == SQLITE_OK)
                                        {
                                            sqlite3_bind_int(stmt, 1, friend_id);
                                            sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                            sqlite3_step(stmt);
                                            sqlite3_finalize(stmt);
                                        }
                                        snprintf(rasp, sizeof(rasp), "SUCCESS|%s (tip: %s) a fost sters din lista de prieteni\n", friend_username, friend_type_display);
                                        log_activity(sesiune->username, "DELETE_FRIEND_SUCCESS", friend_username);
                                        printf("User %s a sters pe %s din lista de prieteni\n", sesiune->username, friend_username);
                                        sqlite3_close(conn);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void send_message(int fd, char raspuns[], char* prieten_username, char* message_text)
{
    char rasp[2000];
    
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "SEND_MESSAGE_FAILED", "Not authenticated");
    }
    else if(strlen(message_text) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Mesajul nu poate fi gol\n");
        log_activity(NULL, "SEND_MESSAGE_FAILED", "Empty message");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "SEND_MESSAGE_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "SEND_MESSAGE_ATTEMPT", prieten_username);

            sqlite3 *conn = db_connect();
    
            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username=?", -1, &stmt, NULL);
    
                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, prieten_username, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
    
                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", prieten_username);
                        log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Receiver not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int prieten_id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);
    
                        if(prieten_id == sesiune->user_id)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Nu puteti sa va trimiteti mesaj singur\n");
                            log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Cannot message self");
                            sqlite3_close(conn);
                        }
                        rc = sqlite3_prepare_v2(conn, "INSERT INTO messages (sender_id, receiver_id, message_text) VALUES (?, ?, ?)", -1, &stmt, NULL);
    
                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Trimitere mesaj failed\n");
                            log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Database error");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_int(stmt, 2, prieten_id);
                            sqlite3_bind_text(stmt, 3, message_text, -1, SQLITE_STATIC);
                            rc = sqlite3_step(stmt);
    
                            if(rc != SQLITE_DONE)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Trimitere mesaj failed\n");
                                log_activity(sesiune->username, "SEND_MESSAGE_FAILED", "Database step error");
                            }
                            else
                            {
                                int message_id = sqlite3_last_insert_rowid(conn);
                                snprintf(rasp, sizeof(rasp), "SUCCESS|Mesaj trimis catre %s\n", prieten_username);
                                log_activity(sesiune->username, "SEND_MESSAGE_SUCCESS", prieten_username);
                                printf("User %s a trimis mesaj catre %s: %s\n", sesiune->username, prieten_username, message_text);
                            }
                            sqlite3_finalize(stmt);
                        }
                        sqlite3_close(conn);
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void get_messages(int fd, char raspuns[], char* pers_username)
{
    char rasp[10000];  
    
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
    
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        }
        else
        {
            sqlite3 *conn = db_connect();
    
            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username=?", -1, &stmt, NULL);
    
                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, pers_username, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
    
                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", pers_username);
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int pers_id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);
                        rc = sqlite3_prepare_v2(conn, "SELECT m.id, u.username, m.message_text, m.sent_at, m.read_status "
                                                      "FROM messages m "
                                                      "JOIN users u ON m.sender_id = u.id "
                                                      "WHERE (m.sender_id = ? AND m.receiver_id = ?) "
                                                      "OR (m.sender_id = ? AND m.receiver_id = ?) "
                                                      "ORDER BY m.sent_at ASC", -1, &stmt, NULL);

                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Interogare mesaje failed\n");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_int(stmt, 2, pers_id);
                            sqlite3_bind_int(stmt, 3, pers_id);
                            sqlite3_bind_int(stmt, 4, sesiune->user_id);
                            strcpy(rasp, "MESSAGES:");
                            int message_count = 0;
                           
                            while(sqlite3_step(stmt) == SQLITE_ROW)
                            {
                                int msg_id = sqlite3_column_int(stmt, 0);
                                const char *sender = (const char*)sqlite3_column_text(stmt, 1);
                                const char *text = (const char*)sqlite3_column_text(stmt, 2);
                                const char *timestamp = (const char*)sqlite3_column_text(stmt, 3);
                                int read_status = sqlite3_column_int(stmt, 4);
                                char msg_entry[1000];
                                snprintf(msg_entry, sizeof(msg_entry), "%s||%s||%s||%d#", sender, text, timestamp, read_status);
                                strcat(rasp, msg_entry);
                                message_count++;
                            }
                           
                            if(message_count == 0)
                            {
                                snprintf(rasp, sizeof(rasp), "MESSAGES|Niciun mesaj cu %s\n", pers_username);
                            }
                            else
                            {
                                strcat(rasp, "\n");
                                sqlite3_finalize(stmt);
                                rc = sqlite3_prepare_v2(conn, "UPDATE messages SET read_status = 1 "
                                                              "WHERE sender_id = ? AND receiver_id = ? AND read_status = 0", -1, &stmt, NULL);

                                if(rc == SQLITE_OK)
                                {
                                    sqlite3_bind_int(stmt, 1, pers_id);
                                    sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                    sqlite3_step(stmt);
                                }
                            }
                            sqlite3_finalize(stmt);
                        }
                        sqlite3_close(conn);
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void get_conversations(int fd, char raspuns[])
{
    char rasp[5000];
    
    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "GET_CONVERSATIONS_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);
    
        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "GET_CONVERSATIONS_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "GET_CONVERSATIONS_ATTEMPT", NULL);

            sqlite3 *conn = db_connect();
    
            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "GET_CONVERSATIONS_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "WITH conversations AS ( "
                                                "SELECT CASE "
                                                "WHEN sender_id = ? THEN receiver_id "
                                                "ELSE sender_id "
                                                "END AS other_user_id "
                                                "FROM messages "
                                                "WHERE sender_id = ? OR receiver_id = ?) "
                                                "SELECT DISTINCT u.username, "
                                                "(SELECT COUNT(*) FROM messages "
                                                "WHERE sender_id = u.id AND receiver_id = ? AND read_status = 0) AS unread_count, "
                                                "(SELECT message_text FROM messages "
                                                "WHERE (sender_id = ? AND receiver_id = u.id) "
                                                "OR (sender_id = u.id AND receiver_id = ?) "
                                                "ORDER BY sent_at DESC LIMIT 1) AS last_message, "
                                                "(SELECT sender_id FROM messages "
                                                "WHERE (sender_id = ? AND receiver_id = u.id) "
                                                "OR (sender_id = u.id AND receiver_id = ?) "
                                                "ORDER BY sent_at DESC LIMIT 1) AS last_sender_id, "
                                                "(SELECT MAX(sent_at) FROM messages "
                                                "WHERE (sender_id = ? AND receiver_id = u.id) "
                                                "OR (sender_id = u.id AND receiver_id = ?)) AS last_message_time "
                                                "FROM users u "
                                                "INNER JOIN conversations c ON u.id = c.other_user_id "
                                                "WHERE u.id != ? "
                                                "ORDER BY last_message_time DESC", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare conversatii failed\n");
                    log_activity(sesiune->username, "GET_CONVERSATIONS_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_int(stmt, 1, sesiune->user_id);   
                    sqlite3_bind_int(stmt, 2, sesiune->user_id); 
                    sqlite3_bind_int(stmt, 3, sesiune->user_id);  
                    sqlite3_bind_int(stmt, 4, sesiune->user_id);  
                    sqlite3_bind_int(stmt, 5, sesiune->user_id);  
                    sqlite3_bind_int(stmt, 6, sesiune->user_id);  
                    sqlite3_bind_int(stmt, 7, sesiune->user_id);   
                    sqlite3_bind_int(stmt, 8, sesiune->user_id);   
                    sqlite3_bind_int(stmt, 9, sesiune->user_id); 
                    sqlite3_bind_int(stmt, 10, sesiune->user_id);
                    sqlite3_bind_int(stmt, 11, sesiune->user_id); 
                    strcpy(rasp, "CONVERSATIONS:\n");
                    int conv_count = 0;

                    while(sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        const char *username = (const char*)sqlite3_column_text(stmt, 0);
                        int unread = sqlite3_column_int(stmt, 1);
                        const char *last_msg = (const char*)sqlite3_column_text(stmt, 2);
                        int last_sender_id = sqlite3_column_int(stmt, 3); 
                        const char *sender_type;

                        if(last_sender_id == sesiune->user_id) 
                        {
                            sender_type = "me";
                        }
                        else 
                        {
                            sender_type = "them";
                        }
                        char conv[500];
                        snprintf(conv, sizeof(conv), "%s~%d~%s~%s|", username, unread, last_msg ? last_msg : "", sender_type);
                        strcat(rasp, conv);
                        conv_count++;
                    }

                    if(conv_count == 0)
                    {
                        snprintf(rasp, sizeof(rasp), "CONVERSATIONS:NONE");
                    }
                    else
                    {
                        size_t len = strlen(rasp);
                        if(len > 0 && rasp[len-1] == '|') 
                        {
                            rasp[len-1] = '\n';
                        }
                    }
                    sqlite3_finalize(stmt);
                }
                sqlite3_close(conn);
            }
        }
    }
    strcpy(raspuns, rasp);
}

void create_group(int fd, char raspuns[], char* group_name, char* members_list)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "CREATE_GROUP_FAILED", "Not authenticated");
    }
    else if(strlen(group_name) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Numele grupului nu poate fi gol\n");
        log_activity(NULL, "CREATE_GROUP_FAILED", "Empty group name");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "CREATE_GROUP_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "CREATE_GROUP_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "CREATE_GROUP_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Verificare grup failed\n");
                    log_activity(sesiune->username, "CREATE_GROUP_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc == SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s exista deja\n", group_name);
                        log_activity(sesiune->username, "CREATE_GROUP_FAILED", "Group already exists");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        sqlite3_finalize(stmt);
                        rc = sqlite3_prepare_v2(conn, "INSERT INTO groups (group_name, creator_id) VALUES (?, ?)", -1, &stmt, NULL);

                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Creare grup failed\n");
                            log_activity(sesiune->username, "CREATE_GROUP_FAILED", "Database error");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                            sqlite3_bind_int(stmt, 2, sesiune->user_id);
                            rc = sqlite3_step(stmt);

                            if(rc != SQLITE_DONE)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Creare grup failed\n");
                                log_activity(sesiune->username, "CREATE_GROUP_FAILED", "Database step error");
                                sqlite3_finalize(stmt);
                                sqlite3_close(conn);
                            }
                            else
                            {
                                int group_id = sqlite3_last_insert_rowid(conn);
                                sqlite3_finalize(stmt);
                                rc = sqlite3_prepare_v2(conn, "INSERT INTO group_members (group_id, user_id) VALUES (?, ?)", -1, &stmt, NULL);

                                if(rc == SQLITE_OK)
                                {
                                    sqlite3_bind_int(stmt, 1, group_id);
                                    sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                    sqlite3_step(stmt);
                                    sqlite3_finalize(stmt);
                                }
                                int members_added = 0;
                                int members_failed = 0;

                                if(strlen(members_list) > 0)
                                {
                                    char members_copy[500];
                                    strcpy(members_copy, members_list);
                                    char *user = strtok(members_copy, " ");

                                    while(user != NULL)
                                    {
                                        trim_string(user);

                                        if(strlen(user) > 0)
                                        {
                                            rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username = ?", -1, &stmt, NULL);

                                            if(rc == SQLITE_OK)
                                            {
                                                sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
                                                rc = sqlite3_step(stmt);

                                                if(rc == SQLITE_ROW)
                                                {
                                                    int member_id = sqlite3_column_int(stmt, 0);
                                                    sqlite3_finalize(stmt);

                                                    if(member_id != sesiune->user_id)
                                                    {
                                                        rc = sqlite3_prepare_v2(conn, "INSERT OR IGNORE INTO group_members (group_id, user_id) VALUES (?, ?)", -1, &stmt, NULL);

                                                        if(rc == SQLITE_OK)
                                                        {
                                                            sqlite3_bind_int(stmt, 1, group_id);
                                                            sqlite3_bind_int(stmt, 2, member_id);
                                                            rc = sqlite3_step(stmt);

                                                            if(rc == SQLITE_DONE)
                                                            {
                                                                members_added++;
                                                            }
                                                            else
                                                            {
                                                                members_failed++;
                                                            }
                                                            sqlite3_finalize(stmt);
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    sqlite3_finalize(stmt);
                                                    members_failed++;
                                                }
                                            }
                                        }
                                        user = strtok(NULL, " ");
                                    }
                                }
                                snprintf(rasp, sizeof(rasp), "SUCCESS|Grup %s creat (ID: %d). Membri adaugati: %d, Failed: %d\n", group_name, group_id, members_added, members_failed);
                                log_activity(sesiune->username, "CREATE_GROUP_SUCCESS", group_name);
                                printf("User %s a creat grupul %s cu ID %d\n", sesiune->username, group_name, group_id);
                            }
                        }
                        sqlite3_close(conn);
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void send_group_message(int fd, char raspuns[], char* group_name, char* message_text)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "SEND_GROUP_MESSAGE_FAILED", "Not authenticated");
    }
    else if(strlen(message_text) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Mesajul nu poate fi gol\n");
        log_activity(NULL, "SEND_GROUP_MESSAGE_FAILED", "Empty message");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "SEND_GROUP_MESSAGE_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "SEND_GROUP_MESSAGE_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s nu exista\n", group_name);
                        log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Group not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);
                        rc = sqlite3_prepare_v2(conn, "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?", -1, &stmt, NULL);

                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Verificare membru failed\n");
                            log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Database error");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_int(stmt, 1, group_id);
                            sqlite3_bind_int(stmt, 2, sesiune->user_id);
                            rc = sqlite3_step(stmt);

                            if(rc != SQLITE_ROW)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Nu esti membru al grupului %s\n", group_name);
                                log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Not a member");
                                sqlite3_finalize(stmt);
                                sqlite3_close(conn);
                            }
                            else
                            {
                                sqlite3_finalize(stmt);
                                rc = sqlite3_prepare_v2(conn, "INSERT INTO group_messages (group_id, sender_id, message_text) VALUES (?, ?, ?)", -1, &stmt, NULL);

                                if(rc != SQLITE_OK)
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Trimitere mesaj failed\n");
                                    log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Database error");
                                    sqlite3_close(conn);
                                }
                                else
                                {
                                    sqlite3_bind_int(stmt, 1, group_id);
                                    sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                    sqlite3_bind_text(stmt, 3, message_text, -1, SQLITE_STATIC);
                                    rc = sqlite3_step(stmt);

                                    if(rc != SQLITE_DONE)
                                    {
                                        snprintf(rasp, sizeof(rasp), "ERROR|Trimitere mesaj failed\n");
                                        log_activity(sesiune->username, "SEND_GROUP_MESSAGE_FAILED", "Database step error");
                                    }
                                    else
                                    {
                                        int message_id = sqlite3_last_insert_rowid(conn);
                                        snprintf(rasp, sizeof(rasp), "SUCCESS|Mesaj trimis in grupul %s\n", group_name);
                                        log_activity(sesiune->username, "SEND_GROUP_MESSAGE_SUCCESS", group_name);
                                        printf("User %s a trimis mesaj in grupul %s: %s\n", sesiune->username, group_name, message_text);
                                    }
                                    sqlite3_finalize(stmt);
                                }
                                sqlite3_close(conn);
                            }
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void get_group_messages(int fd, char raspuns[], char* group_name)
{
    char rasp[10000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "GET_GROUP_MESSAGES_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "GET_GROUP_MESSAGES_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "GET_GROUP_MESSAGES_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s nu exista\n", group_name);
                        log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Group not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);
                        rc = sqlite3_prepare_v2(conn, "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?", -1, &stmt, NULL);

                        if(rc != SQLITE_OK)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Verificare membru failed\n");
                            log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Database error");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            sqlite3_bind_int(stmt, 1, group_id);
                            sqlite3_bind_int(stmt, 2, sesiune->user_id);
                            rc = sqlite3_step(stmt);

                            if(rc != SQLITE_ROW)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Nu esti membru al grupului %s\n", group_name);
                                log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Not a member");
                                sqlite3_finalize(stmt);
                                sqlite3_close(conn);
                            }
                            else
                            {
                                sqlite3_finalize(stmt);
                                rc = sqlite3_prepare_v2(conn, "SELECT gm.id, u.username, gm.message_text, gm.sent_at "
                                                              "FROM group_messages gm "
                                                              "JOIN users u ON gm.sender_id = u.id "
                                                              "WHERE gm.group_id = ? "
                                                              "ORDER BY gm.sent_at ASC", -1, &stmt, NULL);

                                if(rc != SQLITE_OK)
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare mesaje failed\n");
                                    log_activity(sesiune->username, "GET_GROUP_MESSAGES_FAILED", "Database error");
                                    sqlite3_close(conn);
                                }
                                else
                                {
                                    sqlite3_bind_int(stmt, 1, group_id);
                                    strcpy(rasp, "GROUP_MESSAGES:");
                                    int message_count = 0;

                                    while(sqlite3_step(stmt) == SQLITE_ROW)
                                    {
                                        int msg_id = sqlite3_column_int(stmt, 0);
                                        const char *sender = (const char*)sqlite3_column_text(stmt, 1);
                                        const char *text = (const char*)sqlite3_column_text(stmt, 2);
                                        const char *timestamp = (const char*)sqlite3_column_text(stmt, 3);
                                        char msg_entry[1000];
                                        snprintf(msg_entry, sizeof(msg_entry), "%s: %s\n[%s]|",sender, text, timestamp);
                                        strcat(rasp, msg_entry);
                                        message_count++;
                                    }

                                    if(message_count == 0)
                                    {
                                        snprintf(rasp, sizeof(rasp), "GROUP_MESSAGES:Niciun mesaj in grupul %s\n", group_name);
                                    }
                                    else
                                    {
                                        strcat(rasp, "\n");
                                    }
                                    sqlite3_finalize(stmt);
                                }

                                char read_marker[20];
                                snprintf(read_marker, sizeof(read_marker), "%d", sesiune->user_id);
                                rc = sqlite3_prepare_v2(conn, "UPDATE group_messages SET read_by = "
                                                            "CASE WHEN read_by = '' THEN ? "
                                    "  WHEN read_by NOT LIKE '%' || ? || '%' THEN read_by || ',' || ? "
                                    "  ELSE read_by END WHERE group_id = ? AND sender_id != ?", -1, &stmt, NULL);
                                    
                                if(rc == SQLITE_OK)
                                {
                                    sqlite3_bind_text(stmt, 1, read_marker, -1, SQLITE_STATIC);
                                    sqlite3_bind_text(stmt, 2, read_marker, -1, SQLITE_STATIC);
                                    sqlite3_bind_text(stmt, 3, read_marker, -1, SQLITE_STATIC);
                                    sqlite3_bind_int(stmt, 4, group_id);
                                    sqlite3_bind_int(stmt, 5, sesiune->user_id);
                                    sqlite3_step(stmt);
                                    sqlite3_finalize(stmt);
                                }
                                sqlite3_close(conn);
                            }
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void get_my_groups(int fd, char raspuns[])
{
    char rasp[5000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        }
        else
        {
            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
            }
            else
            {
                sqlite3_stmt *stmt;
                char user_id_str[20];
                snprintf(user_id_str, sizeof(user_id_str), "%d", sesiune->user_id);
                int rc = sqlite3_prepare_v2(conn, 
                    "SELECT g.id, g.group_name, u.username as creator, "
                    "(SELECT COUNT(*) FROM group_members WHERE group_id = g.id) as member_count, "
                    "COALESCE((SELECT gm2.message_text FROM group_messages gm2 "
                    "WHERE gm2.group_id = g.id ORDER BY gm2.sent_at DESC LIMIT 1), '') as last_message, "
                    "(SELECT COUNT(*) FROM group_messages gm3 "
                    "WHERE gm3.group_id = g.id "
                    "AND gm3.sender_id != ? "
                    "AND (gm3.read_by NOT LIKE '%' || ? || '%' OR gm3.read_by IS NULL OR gm3.read_by = '')) as unread_count "
                    "FROM groups g "
                    "JOIN users u ON g.creator_id = u.id "
                    "JOIN group_members gm ON g.id = gm.group_id "
                    "WHERE gm.user_id = ? "
                    "ORDER BY g.created_at DESC", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    printf("[GET_MY_GROUPS] EROARE prepare: %s\n", sqlite3_errmsg(conn));
                    fflush(stdout);
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare grupuri failed\n");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_int(stmt, 1, sesiune->user_id); 
                    sqlite3_bind_text(stmt, 2, user_id_str, -1, SQLITE_STATIC); 
                    sqlite3_bind_int(stmt, 3, sesiune->user_id);     
                    strcpy(rasp, "MY_GROUPS:");
                    int group_count = 0;

                    while(sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        const char *group_name = (const char*)sqlite3_column_text(stmt, 1);
                        const char *creator = (const char*)sqlite3_column_text(stmt, 2);  
                        int member_count = sqlite3_column_int(stmt, 3);
                        const char *last_msg = (const char*)sqlite3_column_text(stmt, 4);
                        int unread_count = sqlite3_column_int(stmt, 5); 
                        char group_entry[500];

                        if(last_msg && strlen(last_msg) > 0)
                        {
                            snprintf(group_entry, sizeof(group_entry), "%s:%s:%d:%s|", group_name, creator, unread_count, last_msg);
                        }
                        else
                        {
                            snprintf(group_entry, sizeof(group_entry), "%s:%s:%d:Niciun mesaj|", group_name, creator, unread_count);
                        }

                        strcat(rasp, group_entry);
                        group_count++;
                        printf("[GET_MY_GROUPS] Grup: %s, Creator: %s, Unread: %d, Membri: %d\n", group_name, creator, unread_count, member_count);
                        fflush(stdout);
                    }

                    if(group_count == 0)
                    {
                        snprintf(rasp, sizeof(rasp), "MY_GROUPS:Nu faceti parte din niciun grup\n");
                    }
                    else
                    {
                        size_t len = strlen(rasp);

                        if(len > 0 && rasp[len-1] == '|') 
                        {
                            rasp[len-1] = '\n';
                        }
                        else
                        {
                            strcat(rasp, "\n");
                        }
                        printf("[GET_MY_GROUPS] Total grupuri: %d\n", group_count);
                        fflush(stdout);
                    }
                    sqlite3_finalize(stmt);
                }
                sqlite3_close(conn);
            }
        }
    }
    strcpy(raspuns, rasp);
}

void add_group_member(int fd, char raspuns[], char* group_name, char* username)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "ADD_GROUP_MEMBER_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "ADD_GROUP_MEMBER_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "ADD_GROUP_MEMBER_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id, creator_id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s nu exista\n", group_name);
                        log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Group not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        int creator_id = sqlite3_column_int(stmt, 1);
                        sqlite3_finalize(stmt);

                        if(creator_id != sesiune->user_id)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Doar adminul grupului poate adauga membri\n");
                            log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Not group admin");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username = ?", -1, &stmt, NULL);

                            if(rc != SQLITE_OK)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                                log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Database error");
                                sqlite3_close(conn);
                            }
                            else
                            {
                                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
                                rc = sqlite3_step(stmt);

                                if(rc != SQLITE_ROW)
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", username);
                                    log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "User not found");
                                    sqlite3_finalize(stmt);
                                    sqlite3_close(conn);
                                }
                                else
                                {
                                    int new_member_id = sqlite3_column_int(stmt, 0);
                                    sqlite3_finalize(stmt);
                                    rc = sqlite3_prepare_v2(conn, "INSERT OR IGNORE INTO group_members (group_id, user_id) VALUES (?, ?)", -1, &stmt, NULL);

                                    if(rc != SQLITE_OK)
                                    {
                                        snprintf(rasp, sizeof(rasp), "ERROR|Adaugare membru failed\n");
                                        log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Database error");
                                        sqlite3_close(conn);
                                    }
                                    else
                                    {
                                        sqlite3_bind_int(stmt, 1, group_id);
                                        sqlite3_bind_int(stmt, 2, new_member_id);
                                        rc = sqlite3_step(stmt);

                                        if(rc == SQLITE_DONE)
                                        {
                                            snprintf(rasp, sizeof(rasp), "SUCCESS|%s adaugat in grupul %s\n", username, group_name);
                                            log_activity(sesiune->username, "ADD_GROUP_MEMBER_SUCCESS", username);
                                            printf("User %s a adaugat pe %s in grupul %s\n", sesiune->username, username, group_name);
                                        }
                                        else
                                        {
                                            snprintf(rasp, sizeof(rasp), "ERROR|%s este deja membru\n", username);
                                            log_activity(sesiune->username, "ADD_GROUP_MEMBER_FAILED", "Already a member");
                                        }
                                        sqlite3_finalize(stmt);
                                    }
                                    sqlite3_close(conn);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void delete_group(int fd, char raspuns[], char* group_name)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "DELETE_GROUP_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "DELETE_GROUP_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "DELETE_GROUP_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id, creator_id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s nu exista\n", group_name);
                        log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Group not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        int creator_id = sqlite3_column_int(stmt, 1);
                        sqlite3_finalize(stmt);

                        if(creator_id != sesiune->user_id)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Doar creatorul poate sterge grupul\n");
                            log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Not group creator");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            rc = sqlite3_prepare_v2(conn, "DELETE FROM group_messages WHERE group_id = ?", -1, &stmt, NULL);

                            if(rc == SQLITE_OK)
                            {
                                sqlite3_bind_int(stmt, 1, group_id);
                                sqlite3_step(stmt);
                                sqlite3_finalize(stmt);
                            }
                            rc = sqlite3_prepare_v2(conn, "DELETE FROM group_members WHERE group_id = ?", -1, &stmt, NULL);

                            if(rc == SQLITE_OK)
                            {
                                sqlite3_bind_int(stmt, 1, group_id);
                                sqlite3_step(stmt);
                                sqlite3_finalize(stmt);
                            }
                            rc = sqlite3_prepare_v2(conn, "DELETE FROM groups WHERE id = ?", -1, &stmt, NULL);

                            if(rc != SQLITE_OK)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Stergere grup failed\n");
                                log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Database error");
                                sqlite3_close(conn);
                            }
                            else
                            {
                                sqlite3_bind_int(stmt, 1, group_id);
                                rc = sqlite3_step(stmt);

                                if(rc == SQLITE_DONE)
                                {
                                    snprintf(rasp, sizeof(rasp), "SUCCESS|Grupul %s a fost sters\n", group_name);
                                    log_activity(sesiune->username, "DELETE_GROUP_SUCCESS", group_name);
                                    printf("User %s a sters grupul %s\n", sesiune->username, group_name);
                                }
                                else
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Stergere grup failed\n");
                                    log_activity(sesiune->username, "DELETE_GROUP_FAILED", "Database step error");
                                }
                                sqlite3_finalize(stmt);
                            }
                            sqlite3_close(conn);
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void leave_group(int fd, char raspuns[], char* group_name)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "LEAVE_GROUP_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "LEAVE_GROUP_FAILED", "Session not found");
        }
        else
        {
            log_activity(sesiune->username, "LEAVE_GROUP_ATTEMPT", group_name);

            sqlite3 *conn = db_connect();

            if (!conn)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Database error");
            }
            else
            {
                sqlite3_stmt *stmt;
                int rc = sqlite3_prepare_v2(conn, "SELECT id, creator_id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

                if(rc != SQLITE_OK)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
                    log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Database error");
                    sqlite3_close(conn);
                }
                else
                {
                    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);

                    if(rc != SQLITE_ROW)
                    {
                        snprintf(rasp, sizeof(rasp), "ERROR|Grupul %s nu exista\n", group_name);
                        log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Group not found");
                        sqlite3_finalize(stmt);
                        sqlite3_close(conn);
                    }
                    else
                    {
                        int group_id = sqlite3_column_int(stmt, 0);
                        int creator_id = sqlite3_column_int(stmt, 1);
                        sqlite3_finalize(stmt);

                        if(creator_id == sesiune->user_id)
                        {
                            snprintf(rasp, sizeof(rasp), "ERROR|Creatorul nu poate parasi grupul\n");
                            log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Creator cannot leave");
                            sqlite3_close(conn);
                        }
                        else
                        {
                            rc = sqlite3_prepare_v2(conn, "DELETE FROM group_members WHERE group_id = ? AND user_id = ?", -1, &stmt, NULL);

                            if(rc != SQLITE_OK)
                            {
                                snprintf(rasp, sizeof(rasp), "ERROR|Parasire grup failed\n");
                                log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Database error");
                                sqlite3_close(conn);
                            }
                            else
                            {
                                sqlite3_bind_int(stmt, 1, group_id);
                                sqlite3_bind_int(stmt, 2, sesiune->user_id);
                                rc = sqlite3_step(stmt);

                                if(rc == SQLITE_DONE)
                                {
                                    snprintf(rasp, sizeof(rasp), "SUCCESS|Ai parasit grupul %s\n", group_name);
                                    log_activity(sesiune->username, "LEAVE_GROUP_SUCCESS", group_name);
                                    printf("User %s a parasit grupul %s\n", sesiune->username, group_name);
                                }
                                else
                                {
                                    snprintf(rasp, sizeof(rasp), "ERROR|Parasire grup failed\n");
                                    log_activity(sesiune->username, "LEAVE_GROUP_FAILED", "Database step error");
                                }
                                sqlite3_finalize(stmt);
                            }
                            sqlite3_close(conn);
                        }
                    }
                }
            }
        }
    }
    strcpy(raspuns, rasp);
}

void director_imagini()
{
    struct stat st = {0};

    if(stat("server_images", &st) == -1)
    {
        mkdir("server_images", 0755);
    }
}

void post(int fd, char raspuns[], char* image_path, char* text_content, char* visibility)
{
    char rasp[2000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "POST_FAILED", "Not authenticated");
    }
    else if(strlen(image_path) == 0 && strlen(text_content) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Trebuie sa adaugati text sau imagine\n");
        log_activity(NULL, "POST_FAILED", "Empty post");
    }
    else
    {
        if(strcmp(visibility, "FRIENDS") != 0 && strcmp(visibility, "CLOSE_FRIENDS") != 0)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Vizibilitate invalida (FRIENDS, CLOSE_FRIENDS)\n");
            log_activity(NULL, "POST_FAILED", "Invalid visibility");
        }
        else
        {
            sesiuneClienti* sesiune = get_sesiune(fd);

            if(sesiune == NULL)
            {
                snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
                log_activity(NULL, "POST_FAILED", "Session not found");
            }
            else
            {
                log_activity(sesiune->username, "POST_ATTEMPT", visibility);

                char db_visibility[50];

                if(strcmp(visibility, "FRIENDS") == 0)
                {
                    strcpy(db_visibility, "friends");
                }
                else
                {
                    strcpy(db_visibility, "close_friends");
                }
                char temp_image_path[300] = "";
                char *extensie = NULL;

                if(strlen(image_path) > 0)
                {
                    director_imagini();
                    extensie = strrchr(image_path, '.');

                    if(!extensie)
                    {
                        extensie = ".jpg"; 
                    }
                    snprintf(temp_image_path, sizeof(temp_image_path), "server_images/temp_%d%s", sesiune->user_id, extensie);
                    FILE *p1 = fopen(image_path, "rb");

                    if(p1)
                    {
                        FILE *p2 = fopen(temp_image_path, "wb");
                        if(p2)
                        {
                            char buf[4096];
                            size_t n;
                            while((n = fread(buf, 1, sizeof(buf), p1)) > 0)
                            {
                                fwrite(buf, 1, n, p2);
                            }
                            fclose(p2);
                            printf("Imagine temporara copiata: %s\n", temp_image_path);
                        }
                        else
                        {
                            printf("ATENTIE: Nu pot crea %s\n", temp_image_path);
                            temp_image_path[0] = '\0';
                        }
                        fclose(p1);
                    }
                    else
                    {
                        printf("ATENTIE: Nu pot deschide imaginea: %s\n", image_path);
                        temp_image_path[0] = '\0';
                    }
                }

                sqlite3 *conn = db_connect();

                if (!conn)
                {
                    snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
                    log_activity(sesiune->username, "POST_FAILED", "Database error");
                }
                else
                {
                    sqlite3_stmt *stmt;
                    int rc;

                    if(strlen(temp_image_path) > 0 && strlen(text_content) > 0)
                    {
                        rc = sqlite3_prepare_v2(conn, "INSERT INTO posts (user_id, image_path, text_content, post_type) VALUES (?, 'temp', ?, ?)", -1, &stmt, NULL);

                        if(rc == SQLITE_OK)
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_text(stmt, 2, text_content, -1, SQLITE_STATIC);
                            sqlite3_bind_text(stmt, 3, db_visibility, -1, SQLITE_STATIC);
                        }
                    }
                    else if(strlen(temp_image_path) > 0)
                    {
                        rc = sqlite3_prepare_v2(conn, "INSERT INTO posts (user_id, image_path, post_type) VALUES (?, 'temp', ?)", -1, &stmt, NULL);

                        if(rc == SQLITE_OK)
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_text(stmt, 2, db_visibility, -1, SQLITE_STATIC);
                        }
                    }
                    else
                    {
                        rc = sqlite3_prepare_v2(conn, "INSERT INTO posts (user_id, text_content, post_type) VALUES (?, ?, ?)", -1, &stmt, NULL);

                        if(rc == SQLITE_OK)
                        {
                            sqlite3_bind_int(stmt, 1, sesiune->user_id);
                            sqlite3_bind_text(stmt, 2, text_content, -1, SQLITE_STATIC);
                            sqlite3_bind_text(stmt, 3, db_visibility, -1, SQLITE_STATIC);
                        }
                    }

                    if(rc != SQLITE_OK)
                    {
                        fprintf(stderr, "Eroare SQL: %s\n", sqlite3_errmsg(conn));
                        snprintf(rasp, sizeof(rasp), "ERROR|Creare post failed\n");
                        log_activity(sesiune->username, "POST_FAILED", "Database error");
                    }
                    else
                    {
                        rc = sqlite3_step(stmt);

                        if(rc != SQLITE_DONE)
                        {
                            fprintf(stderr, "Eroare SQL step: %s\n", sqlite3_errmsg(conn));
                            snprintf(rasp, sizeof(rasp), "ERROR|Creare post failed\n");
                            log_activity(sesiune->username, "POST_FAILED", "Database step error");
                        }
                        else
                        {
                            int post_id = (int)sqlite3_last_insert_rowid(conn);
                            printf("Post creat cu ID: %d\n", post_id);

                            fflush(stdout);

                            if(strlen(temp_image_path) > 0)
                            {
                                char final_image_path[300];
                                snprintf(final_image_path, sizeof(final_image_path), "server_images/%d%s", post_id, extensie);

                                printf("\n=== IMAGE DEBUG ===\n");
                                printf("Post ID: %d\n", post_id);
                                printf("Extension: %s\n", extensie);
                                printf("Temp path: %s\n", temp_image_path);
                                printf("Final path: %s\n", final_image_path);
                                fflush(stdout);

                                int rename_result = rename(temp_image_path, final_image_path);
                                
                                if(rename_result == 0)
                                {
                                    printf("Rename SUCCESS: %s\n", final_image_path);
                                    fflush(stdout);
                                    
                                    sqlite3_stmt *update_stmt;
                                    rc = sqlite3_prepare_v2(conn, "UPDATE posts SET image_path = ? WHERE id = ?", -1, &update_stmt, NULL);

                                    if(rc != SQLITE_OK)
                                    {
                                        printf("UPDATE PREPARE FAILED: %s\n", sqlite3_errmsg(conn));
                                        fflush(stdout);
                                    }
                                    else
                                    {
                                        sqlite3_bind_text(update_stmt, 1, final_image_path, -1, SQLITE_STATIC);
                                        sqlite3_bind_int(update_stmt, 2, post_id);
                                        
                                        printf("Executing UPDATE: SET image_path='%s' WHERE id=%d\n", final_image_path, post_id);
                                        fflush(stdout);
                                        
                                        rc = sqlite3_step(update_stmt);
                                        
                                        if(rc != SQLITE_DONE)
                                        {
                                            printf("UPDATE STEP FAILED: %s\n", sqlite3_errmsg(conn));
                                            fflush(stdout);
                                        }
                                        else
                                        {
                                            int rows = sqlite3_changes(conn);
                                            printf("UPDATE SUCCESS - Rows changed: %d\n", rows);
                                            fflush(stdout);
                                            
                                            if(rows == 0)
                                            {
                                                printf("WARNING: No rows updated! Post id=%d might not exist!\n", post_id);
                                                fflush(stdout);
                                            }
                                        }
                                        
                                        sqlite3_finalize(update_stmt);
                                    }
                                }
                                else
                                {
                                    printf("Rename FAILED: %s -> %s (errno: %d - %s)\n", 
                                        temp_image_path, final_image_path, errno, strerror(errno));
                                    fflush(stdout);
                                }
                                
                                printf("=== END DEBUG ===\n\n");
                                fflush(stdout);
                            }

                            printf("User %s a creat postarea %s (ID: %d)\n", sesiune->username, visibility, post_id);
                            log_activity(sesiune->username, "POST_SUCCESS", visibility);
                            snprintf(rasp, sizeof(rasp), "SUCCESS|Post creat cu ID %d\n", post_id);
                        }
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(conn);
                }
            }
        }
    }
    size_t rasp_len = strlen(rasp);
    memcpy(raspuns, rasp, rasp_len);
    raspuns[rasp_len] = '\0';
}

void view_posts(int fd, char raspuns[], char* type)
{
    char rasp[20000];  
    memset(rasp, 0, sizeof(rasp));
    sesiuneClienti* sesiune = get_sesiune(fd);
    int autentif = (sesiune != NULL && sesiune->autentificat);

    if(autentif)
    {
        log_activity(sesiune->username, "VIEW_POSTS_ATTEMPT", type);
    }
    else
    {
        log_activity(NULL, "VIEW_POSTS_ATTEMPT", type);
    }

    if(strcmp(type, "ALL") != 0 && strcmp(type, "CLOSE_FRIENDS") != 0 && strcmp(type, "FRIENDS") != 0 && strcmp(type, "MY_POSTS") != 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Type invalida (FRIENDS, CLOSE_FRIENDS, ALL, MY_POSTS)\n");
        strcpy(raspuns, rasp);
        return;
    }

    if(!autentif && (strcmp(type, "FRIENDS") == 0 || strcmp(type, "CLOSE_FRIENDS") == 0 || strcmp(type, "MY_POSTS") == 0)) 
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Trebuie sa fiti autentificat\n");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        strcpy(raspuns, rasp); 
        return;
    }

    sqlite3_stmt *stmt;
    int rc;

    if(!autentif)
    {
        rc = sqlite3_prepare_v2(conn, 
            "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
            "FROM posts p "
            "JOIN users u ON p.user_id = u.id "
            "WHERE p.post_type = 'friends' AND u.profile_public = 1 "
            "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);
    }
    else if(strcmp(type, "ALL") == 0)
    {
        rc = sqlite3_prepare_v2(conn, 
            "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
            "FROM posts p JOIN users u ON p.user_id = u.id "
            "WHERE p.user_id != ? AND ("
            "(p.post_type = 'friends' AND ("
            "u.profile_public = 1 OR EXISTS (SELECT 1 FROM relations "
            "WHERE user_id = p.user_id AND friend_id = ? "
            "AND friend_type IN ('friend', 'close_friend')))) "
            "OR (p.post_type = 'close_friends' AND "
            "EXISTS (SELECT 1 FROM relations "
            "WHERE user_id = p.user_id AND friend_id = ? "
            "AND friend_type = 'close_friend'))) "
            "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);

        if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, sesiune->user_id);
            sqlite3_bind_int(stmt, 2, sesiune->user_id);
            sqlite3_bind_int(stmt, 3, sesiune->user_id);
        }
    }
    else if(strcmp(type, "MY_POSTS") == 0)
    {
        rc = sqlite3_prepare_v2(conn, 
            "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
            "FROM posts p JOIN users u ON p.user_id = u.id "
            "WHERE p.user_id = ? ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);
        if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, sesiune->user_id);
        }
    }
    else if(strcmp(type, "FRIENDS") == 0)
    {
        rc = sqlite3_prepare_v2(conn, 
            "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
            "FROM posts p JOIN users u ON p.user_id = u.id "
            "WHERE p.user_id != ? "
            "AND p.post_type = 'friends' "
            "AND EXISTS (SELECT 1 FROM relations WHERE user_id = p.user_id AND friend_id = ?) "
            "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);

            if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, sesiune->user_id);
            sqlite3_bind_int(stmt, 2, sesiune->user_id);
        }
    }
    else if(strcmp(type, "CLOSE_FRIENDS") == 0)
    {
        rc = sqlite3_prepare_v2(conn, 
            "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
            "FROM posts p JOIN users u ON p.user_id = u.id "
            "WHERE p.user_id != ? AND p.post_type = 'close_friends' "
            "AND EXISTS (SELECT 1 FROM relations WHERE user_id = p.user_id AND friend_id = ? AND friend_type = 'close_friend') "
            "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);
        if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, sesiune->user_id);
            sqlite3_bind_int(stmt, 2, sesiune->user_id);
        }
    }
    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Eroare SQL: %s\n", sqlite3_errmsg(conn));
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        sqlite3_close(conn);
        strcpy(raspuns, rasp); 
        return;
    }

    strcpy(rasp, "POSTS:");
    int post_count = 0;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int post_id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const unsigned char *img = sqlite3_column_text(stmt, 2);
        char image_path[300];

        if(img)
        {
            snprintf(image_path, sizeof(image_path), "%s", img);
        }
        else
        {
            strcpy(image_path, "");
        }
        const char *text = (const char*)sqlite3_column_text(stmt, 3);
        const char *post_type = (const char*)sqlite3_column_text(stmt, 4);
        const char *timestamp = (const char*)sqlite3_column_text(stmt, 5);
        char post_entry[2000];
        snprintf(post_entry, sizeof(post_entry), "%d~%s~%s~%s~%s~%s|", post_id, username, image_path, text ? text : "", post_type, timestamp);
        strcat(rasp, post_entry);
        post_count++;
    }

    if(post_count == 0)
    {
        snprintf(rasp, sizeof(rasp), "POSTS:NO_POSTS|Nicio postare disponibila\n");
    }
    else
    {
        strcat(rasp, "\n");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    strcpy(raspuns, rasp);
}

void view_user_feed(int fd, char raspuns[], char* user)
{
    char rasp[20000];
    memset(rasp, 0, sizeof(rasp));
    sesiuneClienti* sesiune = get_sesiune(fd);
    int autentif = (sesiune != NULL && sesiune->autentificat);

    if(!autentif)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Trebuie sa fiti autentificat\n");
        log_activity(NULL, "VIEW_USER_FEED_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "VIEW_USER_FEED_ATTEMPT", user);
    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "VIEW_USER_FEED_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT id, profile_public FROM users WHERE username = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "VIEW_USER_FEED_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", user);
        log_activity(sesiune->username, "VIEW_USER_FEED_FAILED", "User not found");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    int profile_public = sqlite3_column_int(stmt, 1);
    sqlite3_finalize(stmt);

    if(user_id == sesiune->user_id)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Pentru profilul tau foloseste VIEW_MY_PROFILE\n");
        log_activity(sesiune->username, "VIEW_USER_FEED_FAILED", "Use VIEW_MY_PROFILE for own profile");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    char relation_type[50] = "none";
    rc = sqlite3_prepare_v2(conn, "SELECT friend_type FROM relations WHERE user_id = ? AND friend_id = ?", -1, &stmt, NULL);

    if(rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, sesiune->user_id);
        sqlite3_bind_int(stmt, 2, user_id);
        if(sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *ft = (const char*)sqlite3_column_text(stmt, 0);

            if(strcmp(ft, "close_friend") == 0)
            {
                strcpy(relation_type, "CLOSE_FRIEND");
            }
            else
            {
                strcpy(relation_type, "FRIEND");
            }
        }
        sqlite3_finalize(stmt);
    }

    printf("VIEW_FEED: user=%s, user_id=%d, my_id=%d, relation=%s, profile_public=%d\n", user, user_id, sesiune->user_id, relation_type, profile_public);
    fflush(stdout);

    if(strcmp(relation_type, "CLOSE_FRIEND") == 0)
    {
        rc = sqlite3_prepare_v2(conn, "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
                                      "FROM posts p JOIN users u ON p.user_id = u.id "
                                      "WHERE p.user_id = ? ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);
                                      
        if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, user_id);
        }
    }
    else if(strcmp(relation_type, "FRIEND") == 0)
    {
        rc = sqlite3_prepare_v2(conn, "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
                                      "FROM posts p JOIN users u ON p.user_id = u.id "
                                      "WHERE p.user_id = ? AND p.post_type = 'friends' "
                                      "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);
                                      
        if(rc == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, user_id);
        }
    }
    else
    {
        if(profile_public)
        {
            rc = sqlite3_prepare_v2(conn, "SELECT p.id, u.username, p.image_path, p.text_content, p.post_type, p.created_at "
                                          "FROM posts p JOIN users u ON p.user_id = u.id "
                                          "WHERE p.user_id = ? AND p.post_type = 'friends' "
                                          "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);

            if(rc == SQLITE_OK)
            {
                sqlite3_bind_int(stmt, 1, user_id);
            }
        }
        else
        {
            snprintf(rasp, sizeof(rasp), "USER_FEED:%s|%s|PRIVATE|NO_POSTS\n", user, relation_type);
            sqlite3_close(conn);
            strcpy(raspuns, rasp);
            printf("Profil privat, user nu este prieten - nicio postare\n");
            return;
        }
    }

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "Eroare SQL: %s\n", sqlite3_errmsg(conn));
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare posturi failed: %s\n", sqlite3_errmsg(conn));
        log_activity(sesiune->username, "VIEW_USER_FEED_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    const char* profile_status = profile_public ? "PUBLIC" : "PRIVATE";
    const char* viewer_type = (sesiune && strcmp(sesiune->user_type, "admin") == 0) ? "ADMIN" : "USER";
    snprintf(rasp, sizeof(rasp), "USER_FEED:%s|%s|%s|%s|", user, relation_type, profile_status, viewer_type);
    int post_count = 0;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int post_id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const unsigned char *img = sqlite3_column_text(stmt, 2);
        char image_path[300];

        if(img)
        {
            snprintf(image_path, sizeof(image_path), "%s", img);
        }
        else
        {
            strcpy(image_path, "");
        }

        const char *text = (const char*)sqlite3_column_text(stmt, 3);
        const char *post_type = (const char*)sqlite3_column_text(stmt, 4);
        const char *timestamp = (const char*)sqlite3_column_text(stmt, 5);
        char post_entry[2000];
        snprintf(post_entry, sizeof(post_entry),"%d~%s~%s~%s~%s~%s|", post_id, username, image_path, text ? text : "", post_type, timestamp);
        strcat(rasp, post_entry);
        post_count++;
    }

    if(post_count == 0)
    {
        snprintf(rasp, sizeof(rasp), "USER_FEED:%s|%s|%s|NO_POSTS", user, relation_type, profile_status);
    }

    strcat(rasp, "\n");
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    strcpy(raspuns, rasp);
    printf("VIEW_FEED finalizat: %s are %d postari (relatie: %s, profil: %s)\n", user, post_count, relation_type, profile_status);
    fflush(stdout);
}

void view_my_profile(int fd, char raspuns[])
{
    char rasp[20000];
    memset(rasp, 0, sizeof(rasp));
    sesiuneClienti* sesiune = get_sesiune(fd);

    if(!sesiune || !sesiune->autentificat)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Trebuie sa fiti autentificat\n");
        log_activity(NULL, "VIEW_MY_PROFILE_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "VIEW_MY_PROFILE_ATTEMPT", NULL);
    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "VIEW_MY_PROFILE_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT username, user_type, profile_public FROM users WHERE id = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "VIEW_MY_PROFILE_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, sesiune->user_id);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizator negasit\n");
        log_activity(sesiune->username, "VIEW_MY_PROFILE_FAILED", "User not found");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    const char *username = (const char*)sqlite3_column_text(stmt, 0);
    const char *user_type = (const char*)sqlite3_column_text(stmt, 1);
    int profile_public = sqlite3_column_int(stmt, 2);
    snprintf(rasp, sizeof(rasp), "MY_PROFILE:%s|%s|%s|", username, user_type, profile_public ? "PUBLIC" : "PRIVATE");
    sqlite3_finalize(stmt);
    rc = sqlite3_prepare_v2(conn, "SELECT u.username, r.friend_type "
                                  "FROM relations r JOIN users u ON r.friend_id = u.id "
                                  "WHERE r.user_id = ? ORDER BY r.friend_type, u.username", -1, &stmt, NULL);

    if(rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, sesiune->user_id);
        int friend_count = 0;

        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *friend_name = (const char*)sqlite3_column_text(stmt, 0);
            const char *friend_type = (const char*)sqlite3_column_text(stmt, 1);
            char friend_entry[100];
            snprintf(friend_entry, sizeof(friend_entry), "%s:%s;", friend_name, strcmp(friend_type, "close_friend") == 0 ? "CLOSE" : "FRIEND");
            strcat(rasp, friend_entry);
            friend_count++;
        }

        if(friend_count == 0)
        {
            strcat(rasp, "NO_FRIENDS");
        }
        sqlite3_finalize(stmt);
    }

    strcat(rasp, "|");
    rc = sqlite3_prepare_v2(conn, "SELECT p.id, p.image_path, p.text_content, p.post_type, p.created_at "
                                  "FROM posts p WHERE p.user_id = ? " 
                                  "ORDER BY p.created_at DESC LIMIT 50", -1, &stmt, NULL);

    if(rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, sesiune->user_id);
        int post_count = 0;

        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            int post_id = sqlite3_column_int(stmt, 0);
            const unsigned char *img = sqlite3_column_text(stmt, 1);
            char image_path[300];

            if(img)
            {
                snprintf(image_path, sizeof(image_path), "%s", img);
            }
            else
            {
                strcpy(image_path, "");
            }

            const char *text = (const char*)sqlite3_column_text(stmt, 2);
            const char *post_type = (const char*)sqlite3_column_text(stmt, 3);
            const char *timestamp = (const char*)sqlite3_column_text(stmt, 4);
            char postare[2000];
            snprintf(postare, sizeof(postare), "%d~%s~%s~%s~%s~%s#", post_id, username, image_path, text ? text : "", post_type, timestamp);
            strcat(rasp, postare);
            post_count++;
        }

        if(post_count == 0)
        {
            strcat(rasp, "NO_POSTS");
        }
        sqlite3_finalize(stmt);
    }

    strcat(rasp, "\n");
    sqlite3_close(conn);
    strcpy(raspuns, rasp);
    printf("my_profile done %s\n", sesiune->username);
}

void delete_post(int fd, char raspuns[], char* post_id_str)
{
    char rasp[500];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "DELETE_POST_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        log_activity(NULL, "DELETE_POST_FAILED", "Session not found");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "DELETE_POST_ATTEMPT", post_id_str);
    int post_id = atoi(post_id_str);

    if(post_id <= 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|ID post invalid\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Invalid post ID");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT user_id, image_path FROM posts WHERE id = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, post_id);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Postarea cu ID %d nu exista\n", post_id);
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Post not found");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    int post_owner_id = sqlite3_column_int(stmt, 0);
    const char *image_path = (const char*)sqlite3_column_text(stmt, 1);
    char image_path_copy[300] = "";

    if(image_path)
    {
        strcpy(image_path_copy, image_path);
    }

    sqlite3_finalize(stmt);
    int is_admin = (strcmp(sesiune->user_type, "admin") == 0);
    int is_owner = (sesiune->user_id == post_owner_id);

    if(!is_owner && !is_admin)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu ai permisiunea sa stergi aceasta postare\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Permission denied");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    rc = sqlite3_prepare_v2(conn, "DELETE FROM posts WHERE id = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Stergere failed\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, post_id);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_DONE)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Stergere failed\n");
        log_activity(sesiune->username, "DELETE_POST_FAILED", "Database step error");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(conn);

    if(strlen(image_path_copy) > 0)
    {
        if(unlink(image_path_copy) == 0)
        {
            printf("Imagine stearsa: %s\n", image_path_copy);
        }
    }

    if(is_admin && !is_owner)
    {
        snprintf(rasp, sizeof(rasp), "SUCCESS|Admin: Postarea %d a fost stearsa\n", post_id);
        log_activity(sesiune->username, "DELETE_POST_SUCCESS", post_id_str);
        printf("Admin %s a sters postarea %d\n", sesiune->username, post_id);
    }
    else
    {
        snprintf(rasp, sizeof(rasp), "SUCCESS|Postarea %d a fost stearsa\n", post_id);
        log_activity(sesiune->username, "DELETE_POST_SUCCESS", post_id_str);
        printf("User %s si-a sters postarea %d\n", sesiune->username, post_id);
    }
    strcpy(raspuns, rasp);
}

void view_all_users(int fd, char raspuns[])
{
    char rasp[10000];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "VIEW_ALL_USERS_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        log_activity(NULL, "VIEW_ALL_USERS_FAILED", "Session not found");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "VIEW_ALL_USERS_ATTEMPT", NULL);

    if(strcmp(sesiune->user_type, "admin") != 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Aceasta comanda este disponibila doar pentru administratori\n");
        log_activity(sesiune->username, "VIEW_ALL_USERS_FAILED", "Permission denied");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "VIEW_ALL_USERS_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(conn, "SELECT id, username, user_type, profile_public, banned, "
                                      "(SELECT COUNT(*) FROM posts WHERE user_id = users.id) as post_count, "
                                      "(SELECT COUNT(*) FROM relations WHERE user_id = users.id) as friend_count "
                                      "FROM users ORDER BY user_type DESC, username ASC", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "VIEW_ALL_USERS_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    strcpy(rasp, "ALL_USERS:");
    int user_count = 0;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int user_id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        const char *user_type = (const char*)sqlite3_column_text(stmt, 2);
        int profile_public = sqlite3_column_int(stmt, 3);
        int banned = sqlite3_column_int(stmt, 4);
        int post_count = sqlite3_column_int(stmt, 5);
        int friend_count = sqlite3_column_int(stmt, 6);
        char user_entry[300];
        snprintf(user_entry, sizeof(user_entry), "%d~%s~%s~%s~%s~%d~%d|", user_id, username, user_type,
                 profile_public ? "PUBLIC" : "PRIVATE", banned ? "BANNED" : "ACTIVE", post_count, friend_count);
        strcat(rasp, user_entry);
        user_count++;
    }

    if(user_count == 0)
    {
        snprintf(rasp, sizeof(rasp), "ALL_USERS|NO_USERS\n");
    }
    else
    {
        strcat(rasp, "\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    strcpy(raspuns, rasp);
    printf("Admin %s a vizualizat toti utilizatorii (%d)\n", sesiune->username, user_count);
}

void remove_sesiune_by_user_id(int user_id)
{
    pthread_mutex_lock(&mutex_sesiune);

    for(int i = 0; i < contor_sesiuni; i++)
    {
        if(sesiuni_active[i].user_id == user_id)
        {
            close(sesiuni_active[i].socket_fd);
            sesiuni_active[i] = sesiuni_active[contor_sesiuni-1];
            contor_sesiuni--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_sesiune);
}

void delete_user(int fd, char raspuns[], char* user)
{
    char rasp[500];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "DELETE_USER_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        log_activity(NULL, "DELETE_USER_FAILED", "Session not found");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "DELETE_USER_ATTEMPT", user);

    if(strcmp(sesiune->user_type, "admin") != 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Aceasta comanda este disponibila doar pentru administratori\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Permission denied");
        strcpy(raspuns, rasp);
        return;
    }

    if(strcmp(sesiune->username, user) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu poti sa te stergi pe tine insuti\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Cannot delete self");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", user);
        log_activity(sesiune->username, "DELETE_USER_FAILED", "User not found");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    rc = sqlite3_prepare_v2(conn, "DELETE FROM users WHERE id = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Stergere failed\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_DONE)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Stergere failed\n");
        log_activity(sesiune->username, "DELETE_USER_FAILED", "Database step error");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    remove_sesiune_by_user_id(user_id);
    snprintf(rasp, sizeof(rasp), "SUCCESS|Utilizatorul %s a fost sters de admin\n", user);
    log_activity(sesiune->username, "DELETE_USER_SUCCESS", user);
    printf("Admin %s a sters utilizatorul %s\n", sesiune->username, user);
    strcpy(raspuns, rasp);
}

void ban_user(int fd, char raspuns[], char* user)
{
    char rasp[500];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "BAN_USER_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }
    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        log_activity(NULL, "BAN_USER_FAILED", "Session not found");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "BAN_USER_ATTEMPT", user);

    if(strcmp(sesiune->user_type, "admin") != 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Aceasta comanda este disponibila doar pentru administratori\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Permission denied");
        strcpy(raspuns, rasp);
        return;
    }

    if(strcmp(sesiune->username, user) == 0)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu poti sa te banezi pe tine insuti\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Cannot ban self");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT id, banned FROM users WHERE username = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s nu exista\n", user);
        log_activity(sesiune->username, "BAN_USER_FAILED", "User not found");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    int already_banned = sqlite3_column_int(stmt, 1);
    sqlite3_finalize(stmt);

    if(already_banned)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizatorul %s este deja banat\n", user);
        log_activity(sesiune->username, "BAN_USER_FAILED", "Already banned");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    rc = sqlite3_prepare_v2(conn, "UPDATE users SET banned = 1 WHERE id = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Ban failed\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_DONE)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Ban failed\n");
        log_activity(sesiune->username, "BAN_USER_FAILED", "Database step error");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    remove_sesiune_by_user_id(user_id);
    snprintf(rasp, sizeof(rasp), "SUCCESS|Utilizatorul %s a fost banat de admin\n", user);
    log_activity(sesiune->username, "BAN_USER_SUCCESS", user);
    printf("Admin %s a banat utilizatorul %s\n", sesiune->username, user);
    strcpy(raspuns, rasp);
}

void logout(int fd, char raspuns[])
{
    char rasp[150];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "LOGOUT_FAILED", "Not authenticated");
    }
    else
    {
        sesiuneClienti* sesiune = get_sesiune(fd);

        if(sesiune == NULL)
        {
            snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
            log_activity(NULL, "LOGOUT_FAILED", "Session not found");
        }
        else
        {
            char username_copy[50];
            strcpy(username_copy, sesiune->username);
            log_activity(username_copy, "LOGOUT_ATTEMPT", NULL);

            remove_sesiune(fd);
            snprintf(rasp, sizeof(rasp), "SUCCESS|Utilizatorul %s s a deconectat\n", username_copy);
            log_activity(username_copy, "LOGOUT_SUCCESS", NULL);
            printf("User %s s-a deconectat\n", username_copy);
        }
    }
    strcpy(raspuns, rasp);
}

void get_unread_count(int fd, char* raspuns)
{
    char rasp[500];
    printf("GET_UNREAD_COUNT start\n");
    fflush(stdout);

    if(!este_autentificat(fd))
    {
        printf("GET_UNREAD_COUNT nu esti autentificat\n");
        fflush(stdout);
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        log_activity(NULL, "GET_UNREAD_COUNT_FAILED", "Not authenticated");
        strcpy(raspuns, rasp);
        return;
    }

    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        printf("GET_UNREAD_COUNT sesiunea nu exista\n");
        fflush(stdout);
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        log_activity(NULL, "GET_UNREAD_COUNT_FAILED", "Session not found");
        strcpy(raspuns, rasp);
        return;
    }

    log_activity(sesiune->username, "GET_UNREAD_COUNT_ATTEMPT", NULL);

    sqlite3 *conn = db_connect();

    if (!conn)
    {
        printf("GET_UNREAD_COUNT eroare conexiune la bd\n");
        fflush(stdout);
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        log_activity(sesiune->username, "GET_UNREAD_COUNT_FAILED", "Database error");
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_stmt *stmt;
    int count = 0;
    int rc = sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages WHERE receiver_id = ? AND read_status = 0", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        printf("GET_UNREAD_COUNT eroare sqlite prepare: %s\n", sqlite3_errmsg(conn));
        fflush(stdout);
        snprintf(rasp, sizeof(rasp), "ERROR|Eroare la interogare baza de date");
        log_activity(sesiune->username, "GET_UNREAD_COUNT_FAILED", "Database error");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, sesiune->user_id);

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
        printf("GET_UNREAD_COUNT mesaje necitite: %d\n", count);
        fflush(stdout);
    }

    sqlite3_finalize(stmt);
    int private_count = 0;
    int group_count = 0;
    rc = sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages WHERE receiver_id = ? AND read_status = 0", -1, &stmt, NULL);

    if(rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, sesiune->user_id);

        if(sqlite3_step(stmt) == SQLITE_ROW)
        {
            private_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", sesiune->user_id);
    rc = sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM group_messages gm "
                                "JOIN group_members gme ON gm.group_id = gme.group_id "
                                "WHERE gme.user_id = ? "
                                "AND gm.sender_id != ? "
                                "AND (gm.read_by NOT LIKE '%' || ? || '%' OR gm.read_by IS NULL)", 
                                -1, &stmt, NULL);

    if(rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, sesiune->user_id);
        sqlite3_bind_int(stmt, 2, sesiune->user_id);
        sqlite3_bind_text(stmt, 3, user_id_str, -1, SQLITE_STATIC);

        if(sqlite3_step(stmt) == SQLITE_ROW)
        {
            group_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(conn);
    int total = private_count + group_count;
    snprintf(rasp, sizeof(rasp), "UNREAD_COUNT:%d", total);
    printf("GET_UNREAD_COUNT raspuns: %s\n", rasp);
    fflush(stdout);
    strcpy(raspuns, rasp);
}

void get_new_messages_count(int fd, char raspuns[], char* partner_username)
{
    char rasp[500];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        strcpy(raspuns, rasp);
        return;
    }
    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT id FROM users WHERE username = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_bind_text(stmt, 1, partner_username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Utilizator inexistent\n");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    int partner_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    rc = sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM messages "
                                  "WHERE sender_id = ? AND receiver_id = ? AND read_status = 0",
                                  -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    sqlite3_bind_int(stmt, 1, partner_id);
    sqlite3_bind_int(stmt, 2, sesiune->user_id);
    int unread_count = 0;

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        unread_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    snprintf(rasp, sizeof(rasp), "NEW_MESSAGES_COUNT:%d", unread_count);
    strcpy(raspuns, rasp);
}

void get_new_group_messages_count(int fd, char raspuns[], char* group_name)
{
    char rasp[500];

    if(!este_autentificat(fd))
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Nu sunteti autentificat\n");
        strcpy(raspuns, rasp);
        return;
    }
    sesiuneClienti* sesiune = get_sesiune(fd);

    if(sesiune == NULL)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Sesiunea nu exista\n");
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3 *conn = db_connect();

    if (!conn)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Conectare la baza de date failed\n");
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(conn, "SELECT id FROM groups WHERE group_name = ?", -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if(rc != SQLITE_ROW)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Grup inexistent\n");
        sqlite3_finalize(stmt);
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }

    int group_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    char user_id_str[20];
    snprintf(user_id_str, sizeof(user_id_str), "%d", sesiune->user_id);
    rc = sqlite3_prepare_v2(conn, "SELECT COUNT(*) FROM group_messages "
                                  "WHERE group_id = ? AND sender_id != ? "
                                  "AND (read_by NOT LIKE '%' || ? || '%' OR read_by IS NULL OR read_by = '')",
                                  -1, &stmt, NULL);

    if(rc != SQLITE_OK)
    {
        snprintf(rasp, sizeof(rasp), "ERROR|Interogare failed\n");
        sqlite3_close(conn);
        strcpy(raspuns, rasp);
        return;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, sesiune->user_id);
    sqlite3_bind_text(stmt, 3, user_id_str, -1, SQLITE_STATIC);
    int unread_count = 0;

    if(sqlite3_step(stmt) == SQLITE_ROW)
    {
        unread_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    snprintf(rasp, sizeof(rasp), "NEW_MESSAGES_COUNT:%d", unread_count);
    strcpy(raspuns, rasp);
}

static void *treat(void * arg) 
{		
    struct threadInfo tdL; 
    tdL= *((struct threadInfo*)arg);	
    printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush (stdout);		 
    pthread_detach(pthread_self());		
    raspunde((struct threadInfo*)arg);
    close (tdL.cl);
    free(arg);
    return(NULL);	
};

void *acceptThread(void *arg)
{
    int sd = *((int*)arg);
    struct sockaddr_in from;
    socklen_t length = sizeof(from);
    pthread_t th;
    int i = 0;

    while (1)
    {
        printf("[ACCEPT] Asteptam clienti...\n");
        fflush(stdout);
        int client = accept(sd, (struct sockaddr*)&from, &length);

        if (client < 0) 
        {
            perror("[ACCEPT] Eroare la accept()");
            continue;
        }
        threadInfo *td = (threadInfo*)malloc(sizeof(threadInfo));
        td->idThread = i;
        td->cl = client;
        pthread_create(&th, NULL, treat, td);
        pthread_detach(th);
        i++;
    }
    return NULL;
}

void raspunde(void *arg)
{
   struct threadInfo tdL = *((struct threadInfo*)arg);
   int cl = tdL.cl;
   while(1)
   {
        char comanda[50000];
        char raspuns[30000];
        memset(comanda, 0, sizeof(comanda));
        memset(raspuns, 0, sizeof(raspuns));
        int bytes_read = read(cl, comanda, sizeof(comanda));

        if(bytes_read <= 0)
        {
            printf("Thread: Clientul s-a deconectat\n");
            remove_sesiune(cl); 
            return;
        }

        printf("Thread: Am primit mesajul:\n%s\n", comanda);
        trim_string(comanda);

        if(strncmp(comanda, "REGISTER", 8) == 0)   //REGISTER|username|password|(ADMIN/USER)
        {
            char username[20], password[20], type[20];
            raspuns[0] = 0;
            int ok_command = 1; 
            char *p, *start = comanda;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else
                {
                    strncpy(username, start, p - start);
                    username[p - start] = 0;
                    printf("%s\n", username);
                    start = p + 1;
                    p = strchr(start, '|');

                    if(!p)
                    {
                        ok_command = 0;
                    }
                    else
                    {
                        strncpy(password, start, p - start);
                        password[p - start] = 0;
                        printf("%s\n", password);
                        start = p + 1;
                        strcpy(type, start);

                        if(strcmp(type, "ADMIN") != 0 && strcmp(type, "USER") != 0)
                        {
                            ok_command = 0;
                        }
                        printf("%s\n", type);
                    }
                }
            }

            if(ok_command)
            {
                register1(raspuns, username, password, type);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "LOGIN", 5) == 0)   // LOGIN|username|password
        {
            char username[20], password[20];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else if(ok_command == 1)
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else if(ok_command == 1)
                {
                    strncpy(username, start, p - start);
                    username[p - start] = 0;
                    start = p + 1;
                    strcpy(password, start);
                }
            }

            if(ok_command)
            {
                printf("%s\n%s\n", username, password);
                snprintf(raspuns, sizeof(raspuns), "Am primit comanda LOGIN with username: %s and password: %s\n", username, password);
                login(cl, username, password, raspuns);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "SET_PROFILE_TYPE", 16) == 0)  // SET_PROFILE_TYPE|(PUBLIC/PRIVATE)
        {
            char type[20];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(type, start);
            }

            if(ok_command)
            {
                printf("%s\n", type);
                snprintf(raspuns, sizeof(raspuns), "Am primit comanda SET_PROFILE_TYPE with type: %s\n", type);
                set_profile(cl, raspuns, type);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "ADD_FRIEND", 10) == 0)  // ADD_FRIEND|username|(FRIEND/CLOSE_FRIEND)
        {
            char username[20], relation[20];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else if(ok_command == 1)
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else if(ok_command == 1)
                {
                    strncpy(username, start, p - start);
                    username[p - start] = 0;
                    start = p + 1;
                    strcpy(relation, start);
                }
            }

            if(ok_command)
            {
                printf("%s\n%s\n", username, relation);
                snprintf(raspuns, sizeof(raspuns), "Am primit comanda ADD_FRIEND with username: %s and relation: %s\n", username, relation);
                add_friend(cl, raspuns, username, relation);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "DELETE_FRIEND", 13) == 0)  // DELETE_FRIEND|username
        {
            char username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(username, start);
                trim_string(username);
            }

            if(ok_command && strlen(username) > 0)
            {
                printf("delete friend %s\n", username);
                delete_friend(cl, raspuns, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "VIEW_POSTS", 10) == 0)        // VIEW_POSTS|TYPE
        {
            char type[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(type, start);
                trim_string(type);
            }

            if(ok_command)
            {
                char raspuns1[25000];
                memset(raspuns1, 0, sizeof(raspuns1));
                view_posts(cl, raspuns1, type);
                strcpy(raspuns, raspuns1);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "SEND_MESSAGE", 12) == 0)  // SEND_MESSAGE|prieten|message
        {
            char prieten[50], message[500];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else
                {
                    strncpy(prieten, start, p - start);
                    prieten[p - start] = 0;
                    start = p + 1;
                    strcpy(message, start);
                    trim_string(message);
                }
            }

            if(ok_command)
            {
                printf("prieten: %s, mesaj: %s\n", prieten, message);
                send_message(cl, raspuns, prieten, message);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "GET_MESSAGES", 12) == 0)  // GET_MESSAGES|pers_username
        {
            char pers_user[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(pers_user, start);
                trim_string(pers_user);
            }

            if(ok_command)
            {
                printf("Get messages with: %s\n", pers_user);
                get_messages(cl, raspuns, pers_user);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "GET_CONVERSATIONS", 17) == 0 && strlen(comanda) == 17)  // GET_CONVERSATIONS
        {
            get_conversations(cl, raspuns);
        }
        else if(strncmp(comanda, "CREATE_GROUP", 12) == 0)   // CREATE_GROUP|group_name|user1 user2 user3
        {
            char group_name[100], members[500];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    strcpy(group_name, start);
                    trim_string(group_name);
                }
                else
                {
                    strncpy(group_name, start, p - start);
                    group_name[p - start] = '\0';
                    trim_string(group_name);
                    start = p + 1;
                    strcpy(members, start);
                    trim_string(members);
                }
            }
            if(ok_command && strlen(group_name) > 0)
            {
                printf("group: %s, members: %s\n", group_name, members);
                create_group(cl, raspuns, group_name, members);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "SEND_GROUP_MESSAGE", 18) == 0)   // SEND_GROUP_MESSAGE|group_name|message
        {
            char group_name[100], message[500]; 
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else
                {
                    strncpy(group_name, start, p - start);
                    group_name[p - start] = '\0';
                    trim_string(group_name);
                    start = p + 1;
                    strcpy(message, start);
                    trim_string(message);
                }
            }

            if(ok_command)
            {
                printf("group: %s, mesaje: %s\n", group_name, message);
                send_group_message(cl, raspuns, group_name, message);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "GET_GROUP_MESSAGES", 18) == 0)   // GET_GROUP_MESSAGES|group_name
        {
            char group_name[100];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(group_name, start);
                trim_string(group_name);
            }

            if(ok_command)
            {
                printf("Get messages from group: %s\n", group_name);
                get_group_messages(cl, raspuns, group_name);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "GET_MY_GROUPS", 13) == 0 && strlen(comanda) == 13)    // GET_MY_GROUPS
        {
            get_my_groups(cl, raspuns);
        }
        else if(strncmp(comanda, "ADD_GROUP_MEMBER", 16) == 0)      // ADD_GROUP_MEMBER|group_name|username
        {
            char group_name[100], username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else
                {
                    strncpy(group_name, start, p - start);
                    group_name[p - start] = '\0';
                    trim_string(group_name);
                    start = p + 1;
                    strcpy(username, start);
                    trim_string(username);
                }
            }

            if(ok_command)
            {
                printf("add %s to group %s\n", username, group_name);
                add_group_member(cl, raspuns, group_name, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "DELETE_GROUP", 12) == 0)  // DELETE_GROUP|group_name
        {
            char group_name[100];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(group_name, start);
                trim_string(group_name);
            }

            if(ok_command && strlen(group_name) > 0)
            {
                printf("delete group: %s\n", group_name);
                delete_group(cl, raspuns, group_name);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "LEAVE_GROUP", 11) == 0)   // LEAVE_GROUP|group_name
        {
            char group_name[100];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(group_name, start);
                trim_string(group_name);
            }

            if(ok_command)
            {
                printf("leave group: %s\n", group_name);
                leave_group(cl, raspuns, group_name);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "LOGOUT", 6) == 0 && strlen(comanda) == 6)   // LOGOUT
        {
            snprintf(raspuns, sizeof(raspuns), "Am primit comanda LOGOUT\n");
            logout(cl, raspuns);
        }
        else if(strncmp(comanda, "POST|", 5) == 0)       // POST|image_path|text|TYPE
        {
            char image_path[300] = "", text_content[500] = "", visibility[20] = "PUBLIC";
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                p = strchr(start, '|');

                if(!p)
                {
                    ok_command = 0;
                }
                else
                {
                    int len = p - start;

                    if(len > 0)
                    {
                        strncpy(image_path, start, len);
                        image_path[len] = '\0';
                    }
                    start = p + 1;
                    p = strchr(start, '|');

                    if(!p)
                    {
                        ok_command = 0;
                    }
                    else
                    {
                        len = p - start;

                        if(len > 0)
                        {
                            strncpy(text_content, start, len);
                            text_content[len] = '\0';
                        }
                        start = p + 1;
                        strcpy(visibility, start);
                        trim_string(visibility);
                    }
                }
            }

            if(ok_command)
            {
                printf("POST %s %s %s\n", image_path, text_content, visibility);
                fflush(stdout);
                post(cl, raspuns, image_path, text_content, visibility);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "VIEW_FEED", 9) == 0)  // VIEW_FEED|username
        {
            char username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(username, start);
                trim_string(username);
            }

            if(ok_command && strlen(username) > 0)
            {
                printf("View feed for: %s\n", username);
                view_user_feed(cl, raspuns, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "VIEW_MY_PROFILE", 15) == 0 && strlen(comanda) == 15)  // VIEW_MY_PROFILE
        {
            printf("view my profile\n");
            view_my_profile(cl, raspuns);
        }
        else if(strncmp(comanda, "DELETE_POST", 11) == 0)  // DELETE_POST|post_id
        {
            char post_id[20];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(post_id, start);
                trim_string(post_id);
            }

            if(ok_command && strlen(post_id) > 0)
            {
                printf("delete post %s\n", post_id);
                delete_post(cl, raspuns, post_id);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "VIEW_ALL_USERS", 14) == 0 && strlen(comanda) == 14)  // VIEW_ALL_USERS
        {
            printf("view all users\n");
            view_all_users(cl, raspuns);
        }
        else if(strncmp(comanda, "DELETE_USER", 11) == 0)  // DELETE_USER|username
        {
            char username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(username, start);
                trim_string(username);
            }

            if(ok_command && strlen(username) > 0)
            {
                printf("admin delete user %s\n", username);
                delete_user(cl, raspuns, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "BAN_USER", 8) == 0)  // BAN_USER|username
        {
            char username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(username, start);
                trim_string(username);
            }

            if(ok_command && strlen(username) > 0)
            {
                printf("admin ban user %s\n", username);
                ban_user(cl, raspuns, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strcmp(comanda, "GET_UNREAD_COUNT") == 0 && strlen(comanda) == 16)     // GET_UNREAD_COUNT
        {
            get_unread_count(cl, raspuns);
        }
        else if(strncmp(comanda, "CHECK_NEW_MESSAGES", 18) == 0)  // CHECK_NEW_MESSAGES|username
        {
            char username[50];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(username, start);
                trim_string(username);
            }

            if(ok_command && strlen(username) > 0)
            {
                get_new_messages_count(cl, raspuns, username);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else if(strncmp(comanda, "CHECK_NEW_GROUP_MESSAGES", 24) == 0)  // CHECK_NEW_GROUP_MESSAGES|group_name
        {
            char group_name[100];
            char *p, *start = comanda;
            int ok_command = 1;
            p = strchr(comanda, '|');

            if(!p)
            {
                ok_command = 0;
            }
            else
            {
                start = p + 1;
                strcpy(group_name, start);
                trim_string(group_name);
            }

            if(ok_command && strlen(group_name) > 0)
            {
                get_new_group_messages_count(cl, raspuns, group_name);
            }
            else
            {
                snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
            }
        }
        else
        {
            snprintf(raspuns, sizeof(raspuns), "ERROR|Unknown command\n");
        }

        size_t total_len = strlen(raspuns);
        size_t sent = 0;

        while(sent < total_len)
        {
            ssize_t n = write(cl, raspuns + sent, total_len - sent);
            if(n <= 0)
            {
                perror("Thread: Eroare la write in client");
                break;
            }
            sent += n;
        }
        printf("Thread: Am trimis raspunsul\n");
    }
}

int main ()
{
    struct sockaddr_in server;
    struct sockaddr_in from;	
    int sd;
    int pid;
    pthread_t th[100]; 
    int i=0;

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }

    int on=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
    
    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);
    
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }
    
    if (listen (sd, 100) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }
    
    pthread_t accept_th;
    pthread_create(&accept_th, NULL, acceptThread, &sd);
    pthread_join(accept_th, NULL);
}

