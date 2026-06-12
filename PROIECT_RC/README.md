# VirtualSoc

Aplicatie client/server ce simuleaza functionalitatea unei retele sociale, dezvoltata ca proiect pentru cursul de Retele de Calculatoare.

Serverul este scris in C (sockets POSIX + pthreads + SQLite + OpenSSL), iar clientul este o aplicatie desktop Qt (C++/Widgets) cu un nucleu de comunicatie scris in C.

## Functionalitati

* Inregistrare si autentificare utilizatori (parole hashed cu PBKDF2-HMAC-SHA256 + salt unic per utilizator).
* Doua tipuri de cont: `user` si `admin`.
* Profil si postari publice sau private.
* Vizualizarea postarilor publice fara autentificare.
* Lista de prieteni cu categorii: `friend` si `close_friend`.
* Postari catre prieteni sau doar catre prietenii apropiati.
* Mesagerie privata 1 la 1.
* Grupuri de discutie cu mai multi membri, mesaje de grup si tracking pentru mesajele citite.
* Atasarea de imagini la postari.
* Comenzi de administrare: vizualizare utilizatori, banare, stergere conturi.
* Logare a activitatii in fisiere zilnice (`logs/server_YYYY-MM-DD.log`).
* Notificari pentru mesaje noi (private si de grup).

## Structura proiectului

```
PROIECT_RC/
├── src/
│   ├── server.c          # server multi-thread (un thread per client)
│   ├── client.c          # nucleul de comunicatie al clientului
│   ├── main.cpp          # entry point Qt
│   └── mainwindow.cpp    # implementare fereastra principala
├── headers/              # headere Qt pentru ferestre (profile, posts, chat, admin, etc.)
├── database/
│   ├── bd.sql            # schema bazei de date
│   └── virtualsoc.db     # baza de date SQLite (generata la prima rulare)
├── sqlite3/              # amalgamation SQLite (sursa + libsqlite3.a)
├── server_images/        # imagini incarcate de utilizatori
├── logs/                 # loguri de activitate ale serverului
├── docs/cerinta.txt      # cerinta originala a proiectului
├── bin/                  # binare compilate (server, client)
├── client.pro            # proiect qmake pentru client
└── build.sh              # script de compilare
```

## Dependinte

Pentru compilare ai nevoie de:

* `gcc`
* `qmake` si `make` (Qt 5 sau Qt 6, modulele `core`, `gui`, `widgets`)
* `libsqlite3-dev`
* `libssl-dev` (OpenSSL)
* `pthread` (inclus in glibc)

Pe Debian/Ubuntu:

```bash
sudo apt install build-essential qtbase5-dev qt5-qmake libsqlite3-dev libssl-dev
```

## Compilare

```bash
./build.sh
```

Scriptul compileaza serverul si clientul si pune binarele in `bin/`.

Manual:

```bash
# server
gcc src/server.c -o bin/server -lpthread -lsqlite3 -lcrypto -lssl

# client
qmake client.pro
make
```

## Initializare baza de date

La prima rulare creeaza baza de date din schema:

```bash
sqlite3 virtualsoc.db < database/bd.sql
```

## Rulare

In doua terminale separate:

```bash
# Terminal 1: serverul (asculta pe portul 2908)
./bin/server

# Terminal 2: clientul Qt
./bin/client 127.0.0.1 2908
```

Daca pornesti clientul fara argumente, va folosi `127.0.0.1:2908` ca default.

## Protocol de comunicare

Comunicarea peste TCP se face prin mesaje text in format `COMANDA|param1|param2|...`. Cateva comenzi suportate:

| Comanda | Format | Descriere |
| --- | --- | --- |
| `REGISTER` | `REGISTER\|user\|pass\|TYPE` | Inregistreaza un cont nou (`USER` sau `ADMIN`). |
| `LOGIN` | `LOGIN\|user\|pass` | Autentificare. |
| `LOGOUT` | `LOGOUT` | Inchide sesiunea curenta. |
| `SET_PROFILE_TYPE` | `SET_PROFILE_TYPE\|PUBLIC\|PRIVATE` | Schimba vizibilitatea profilului. |
| `ADD_FRIEND` | `ADD_FRIEND\|user\|FRIEND\|CLOSE_FRIEND` | Adauga un prieten. |
| `DELETE_FRIEND` | `DELETE_FRIEND\|user` | Sterge un prieten. |
| `POST` | `POST\|image_path\|text\|TYPE` | Posteaza catre `friends` sau `close_friends`. |
| `VIEW_POSTS` | `VIEW_POSTS\|TYPE` | Returneaza postari de un anumit tip. |
| `VIEW_FEED` | `VIEW_FEED\|user` | Feedul unui utilizator. |
| `SEND_MESSAGE` | `SEND_MESSAGE\|user\|text` | Mesaj privat. |
| `GET_MESSAGES` | `GET_MESSAGES\|user` | Istoricul mesajelor cu un utilizator. |
| `GET_CONVERSATIONS` | `GET_CONVERSATIONS` | Lista conversatiilor active. |
| `CREATE_GROUP` | `CREATE_GROUP\|nume\|u1 u2 u3` | Creeaza un grup cu membri. |
| `SEND_GROUP_MESSAGE` | `SEND_GROUP_MESSAGE\|grup\|text` | Mesaj catre grup. |
| `VIEW_ALL_USERS` | `VIEW_ALL_USERS` | (admin) Lista utilizatorilor. |
| `BAN_USER` | `BAN_USER\|user` | (admin) Interzice accesul unui cont. |
| `DELETE_USER` | `DELETE_USER\|user` | (admin) Sterge un cont. |

Server-ul raspunde cu mesaje text prefixate cu `OK|...` sau `ERROR|...`.

## Schema bazei de date

Tabelele principale (SQLite):

* `users` (id, username, password, user_type, profile_public, banned)
* `posts` (id, user_id, image_path, text_content, post_type, created_at)
* `relations` (user_id, friend_id, friend_type)
* `messages` (id, sender_id, receiver_id, message_text, sent_at, read_status)
* `groups` (id, group_name, creator_id, created_at)
* `group_members` (group_id, user_id, joined_at)
* `group_messages` (id, group_id, sender_id, message_text, sent_at, read_by)

Cheile straine au `ON DELETE CASCADE`, iar pe `messages` si `group_messages` exista indecsi pe sender/receiver pentru cautari rapide in conversatii.

## Securitate

* Parolele nu sunt stocate in clar. Sunt derivate cu PBKDF2-HMAC-SHA256, 100000 de iteratii, salt de 16 octeti generat aleator cu `RAND_bytes` din OpenSSL.
* Comenzile sensibile sunt validate server-side pe baza sesiunii autentificate.
* Comenzile administrative sunt restrictionate la conturile cu `user_type = 'admin'`.

## Arhitectura

* Serverul foloseste un thread per conexiune (`pthread_create` la fiecare `accept`).
* Sesiunile active sunt tinute intr-un vector global `sesiuni_active[100]` protejat de un mutex.
* Accesul la lista de utilizatori este protejat de `users_lock`.
* Clientul Qt ruleaza pe thread-ul de UI si comunica sincron cu serverul prin functii C din `client.c`.

## Autori

Proiect realizat in cadrul cursului de Retele de Calculatoare.
