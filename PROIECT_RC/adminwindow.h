#ifndef ADMINWINDOW_H
#define ADMINWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QScreen>
#include <QTimer> 
#include <stdio.h>

extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
}

class AdminWindow : public QWidget
{
    Q_OBJECT

signals:
    void returnToMain();

private:
    QTableWidget *usersTable;
    QLineEdit *commandInput;
    QPushButton *refreshBtn;

public:
    AdminWindow(QWidget *parent = nullptr) : QWidget(parent)
    {
        printf("ADMINWINDOW constructor\n");
        fflush(stdout);
        
        setWindowTitle("Panou Administrator");
        resize(900, 600);

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *titleLabel = new QLabel("<h2>Panou de Control Administrator</h2>");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "color: white; "
            "padding: 15px; "
            "border-radius: 8px;"
        );
        mainLayout->addWidget(titleLabel);

        usersTable = new QTableWidget();
        usersTable->setColumnCount(7);
        usersTable->setHorizontalHeaderLabels({"ID", "Username", "Tip", "Profil", "Status", "Postari", "Prieteni"});
        usersTable->horizontalHeader()->setStretchLastSection(true);
        usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        usersTable->setAlternatingRowColors(true);
        usersTable->setStyleSheet(
            "QTableWidget { "
            "  background-color: white; "
            "  gridline-color: #504848ff; "
            "} "
            "QHeaderView::section { "
            "  background-color: #eff5faff; "
            "  color: black; "
            "  padding: 8px; "
            "  font-weight: bold; "
            "}"
        );
        mainLayout->addWidget(usersTable);

        QFrame *infoFrame = new QFrame();
        infoFrame->setStyleSheet(
            "QFrame { "
            "  background-color: #fff3cd; "
            "  border: 2px solid #ffc107; "
            "  border-radius: 5px; "
            "  padding: 10px; "
            "}"
        );
        QVBoxLayout *infoLayout = new QVBoxLayout(infoFrame);
        
        QLabel *infoLabel = new QLabel(
            "<b>Comenzi disponibile:</b><br>"
            "• DELETE_USER|username<br>"
            "• BAN_USER|username<br>"
            "• DELETE_POST|post_id<br>"
            "• EXIT"
        );
        infoLabel->setWordWrap(true);
        infoLayout->addWidget(infoLabel);
        
        mainLayout->addWidget(infoFrame);

        QFrame *commandFrame = new QFrame();
        commandFrame->setFrameStyle(QFrame::StyledPanel);
        commandFrame->setStyleSheet(
            "QFrame { "
            "  background-color: #f0f0f0; "
            "  border-radius: 5px; "
            "  padding: 10px; "
            "}"
        );
        
        QHBoxLayout *commandLayout = new QHBoxLayout(commandFrame);
        
        QLabel *cmdLabel = new QLabel("<b>Comanda:</b>");
        commandLayout->addWidget(cmdLabel);
        
        commandInput = new QLineEdit();
        commandInput->setStyleSheet("padding: 8px; border: 1px solid #999; border-radius: 4px;");
        commandLayout->addWidget(commandInput);
        
        refreshBtn = new QPushButton("Refresh");
        refreshBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #3498db; "
            "  color: white; "
            "  padding: 8px 15px; "
            "  border: none; "
            "  border-radius: 4px; "
            "  font-weight: bold; "
            "} "
            "QPushButton:hover { background-color: #2980b9; }"
        );
        commandLayout->addWidget(refreshBtn);
        mainLayout->addWidget(commandFrame);

        connect(commandInput, &QLineEdit::returnPressed, this, &AdminWindow::onSendCommand);
        connect(refreshBtn, &QPushButton::clicked, this, &AdminWindow::onRefresh);

        QTimer *refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &AdminWindow::onRefresh);
        refreshTimer->start(20000); 
        
        printf("ADMINWINDOW constructor terminat\n");
        fflush(stdout);
    }

    void displayUsers(const QString& response)
    {
        printf("ADMINWINDOW displayUsers() apelat\n");
        fflush(stdout);

        usersTable->setRowCount(0);

        QString cleanResponse = response.trimmed();
        
        if(cleanResponse.startsWith("ERROR|")) 
        {
            QString errorMsg = cleanResponse.mid(6);
            QMessageBox::critical(this, "Eroare", errorMsg);
            return;
        }
        
        if(!cleanResponse.startsWith("ALL_USERS:")) 
        {
            QMessageBox::warning(this, "Eroare", "Format invalid de raspuns");
            return;
        }

        QString data = cleanResponse.mid(10);
        
        if(data.startsWith("NO_USERS")) 
        {
            QMessageBox::information(this, "Info", "Niciun utilizator in baza de date");
            return;
        }

        QStringList users = data.split('|', Qt::SkipEmptyParts);
        
        printf("ADMINWINDOW numar utilizatori: %d\n", users.size());
        fflush(stdout);

        for(const QString& user : users) 
        {
            QStringList fields = user.split('~');
            
            if(fields.size() < 7) continue;
            
            QString userId = fields[0];
            QString username = fields[1];
            QString userType = fields[2];
            QString profileStatus = fields[3];
            QString bannedStatus = fields[4];
            QString postCount = fields[5];
            QString friendCount = fields[6];
            
            int row = usersTable->rowCount();
            usersTable->insertRow(row);
            
            usersTable->setItem(row, 0, new QTableWidgetItem(userId));
            usersTable->setItem(row, 1, new QTableWidgetItem(username));
            
            QTableWidgetItem *typeItem = new QTableWidgetItem(userType.toUpper());
            if(userType == "admin") 
            {
                typeItem->setForeground(QColor("black"));
            }
            usersTable->setItem(row, 2, typeItem);
            
            usersTable->setItem(row, 3, new QTableWidgetItem(profileStatus));
            
            QTableWidgetItem *statusItem = new QTableWidgetItem(bannedStatus);
            if(bannedStatus == "BANNED") 
            {
                statusItem->setForeground(QColor("black"));
            } 
            else 
            {
                statusItem->setForeground(QColor("black"));
            }
            usersTable->setItem(row, 4, statusItem);
            
            usersTable->setItem(row, 5, new QTableWidgetItem(postCount));
            usersTable->setItem(row, 6, new QTableWidgetItem(friendCount));
        }
                
        printf("ADMINWINDOW %d utilizatori afisati\n", usersTable->rowCount());
        fflush(stdout);
    }

private slots:
    void onSendCommand()
    {
        QString command = commandInput->text().trimmed();
        
        if(command.isEmpty()) 
        {
            QMessageBox::warning(this, "Atentie", "Introdu o comanda");
            return;
        }

        if(command.toUpper() == "EXIT") 
        {
            onExit();
            return;
        }

        printf("ADMINWINDOW trimite comanda: %s\n", command.toUtf8().constData());
        fflush(stdout);

        if(client_send(command.toUtf8().constData()) != 0) 
        {
            QMessageBox::critical(this, "Eroare", "Nu s a putut trimite comanda");
            return;
        }

        char buffer[30000];
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            QMessageBox::critical(this, "Eroare", "Nu s a primit raspuns!");
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        
        printf("ADMINWINDOW Raspuns: %.100s\n", response.toUtf8().constData());
        fflush(stdout);

        QMessageBox::information(this, "Raspuns server", response);
        commandInput->clear();
        
        onRefresh();
    }

    void onRefresh()
    {
        printf("ADMINWINDOW refresh  VIEW_ALL_USERS\n");
        fflush(stdout);
        
        if(client_send("VIEW_ALL_USERS") != 0) 
        {
            QMessageBox::critical(this, "Eroare", "Nu s a putut trimite comanda");
            return;
        }

        char buffer[30000];
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) {
            QMessageBox::critical(this, "Eroare", "Nu s a primit raspuns");
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        displayUsers(response);
    }

    void onExit()
    {
        printf("ADMINWINDOW EXIT\n");
        fflush(stdout);
        
        emit returnToMain();
        this->close();
    }
};

#endif 