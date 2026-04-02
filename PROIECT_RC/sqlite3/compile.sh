gcc -c sqlite3.c -O2 -lpthread -ldl -lm -o sqlite3.o
ar rcs libsqlite3.a sqlite3.o