#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"
#include "chatwindow.h"
#include <stdio.h>

extern "C" {
    int client_connect(const char* server_addr, int server_port);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const char* server_addr;
    int port;

    if(argc == 3) 
    {
        server_addr = argv[1];
        port = atoi(argv[2]);
    } 
    else 
    {
        server_addr = "127.0.0.1";
        port = 2908;
    }

    printf("main: conectare la %s:%d...\n", server_addr, port);
    fflush(stdout);
    
    if(client_connect(server_addr, port) != 0) 
    {
        QMessageBox::critical(nullptr, "Eroare", QString("Nu s a putut conecta la %1:%2").arg(server_addr).arg(port));
        return -1;
    }

    printf("main: conectat\n");
    printf("main: creez fereastra\n");
    fflush(stdout);

    MainWindow window;
    
    printf("main: fereastra creata\n");
    printf("main: afisez fereastra\n");
    fflush(stdout);
    
    window.show();

    printf("aplicatie pornita\n");
    fflush(stdout);

    return app.exec();
}