#ifndef CONVERSATIONSWINDOW_H
#define CONVERSATIONSWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QInputDialog>
#include <QEventLoop>
#include <stdio.h>
#include "chatwindow.h"

extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
}

class ConversationsWindow : public QWidget
{
    Q_OBJECT

signals:
    void returnToMain();

private:
    QListWidget *conversationsList;
    QListWidget *groupsList;
    QPushButton *refreshBtn;
    QPushButton *exitBtn;
    QLabel *unreadLabel;
    QPushButton *createGroupBtn;
    QTimer *refreshTimer;
    ChatWindow *chatWindow;
    bool isRefreshing;
    QString currentUsername;   
    int privateUnreadCount;   

public:
    ConversationsWindow(const QString& username, QWidget *parent = nullptr) : 
                QWidget(parent), isRefreshing(false), currentUsername(username), privateUnreadCount(0)
    {
        printf("CONVERSATIONS constructor\n");
        fflush(stdout);
        
        setWindowTitle("Mesajele Mele");
        resize(600, 700);

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);

        chatWindow = nullptr;

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *headerLabel = new QLabel("Mesajele Mele");
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setStyleSheet(
            "QLabel { "
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "    stop:0 #667eea, stop:1 #764ba2); "
            "  color: white; "
            "  font-size: 20px; "
            "  font-weight: bold; "
            "  padding: 15px; "
            "  border-radius: 8px; "
            "}"
        );
        mainLayout->addWidget(headerLabel);

        unreadLabel = new QLabel();
        unreadLabel->setAlignment(Qt::AlignCenter);
        unreadLabel->setStyleSheet(
            "QLabel { "
            "  background-color: #fed7d7; "
            "  color: #c53030; "
            "  font-weight: bold; "
            "  padding: 10px; "
            "  border-radius: 5px; "
            "}"
        );
        unreadLabel->hide();
        mainLayout->addWidget(unreadLabel);

        QLabel *privateLabel = new QLabel("Conversatii Private");
        privateLabel->setStyleSheet("font-size: 14px; font-weight: bold; margin-top: 10px;");
        mainLayout->addWidget(privateLabel);

        conversationsList = new QListWidget();
        conversationsList->setStyleSheet(
            "QListWidget { "
            "  background-color: white; "
            "  border: 2px solid #e2e8f0; "
            "  border-radius: 8px; "
            "  padding: 5px; "
            "} "
            "QListWidget::item { "
            "  padding: 12px; "
            "  border-bottom: 1px solid #e2e8f0; "
            "  border-radius: 4px; "
            "} "
            "QListWidget::item:hover { "
            "  background-color: #edf2f7; "
            "} "
            "QListWidget::item:selected { "
            "  background-color: #667eea; "
            "  color: white; "
            "}"
        );
        mainLayout->addWidget(conversationsList);

        QLabel *groupsLabel = new QLabel("Grupuri");
        groupsLabel->setStyleSheet("font-size: 14px; font-weight: bold; margin-top: 10px;");
        mainLayout->addWidget(groupsLabel);

        groupsList = new QListWidget();
        groupsList->setStyleSheet(conversationsList->styleSheet());
        mainLayout->addWidget(groupsList);

        QFrame *buttonFrame = new QFrame();
        QHBoxLayout *buttonLayout = new QHBoxLayout(buttonFrame);

        refreshBtn = new QPushButton("Refresh");
        refreshBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #48bb78; "
            "  color: white; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "} "
        );
        buttonLayout->addWidget(refreshBtn);

        createGroupBtn = new QPushButton("Create Group");
        createGroupBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #667eea; "
            "  color: white; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "} "
        );
        buttonLayout->addWidget(createGroupBtn);

         buttonLayout->addStretch();

        exitBtn = new QPushButton("Back");
        exitBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #cbd5e0; "
            "  color: #2d3748; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "} "
        );
        buttonLayout->addWidget(exitBtn);

        mainLayout->addWidget(buttonFrame);

        connect(conversationsList, &QListWidget::itemDoubleClicked, this, &ConversationsWindow::onOpenPrivateChat);
        connect(groupsList, &QListWidget::itemDoubleClicked, this, &ConversationsWindow::onOpenGroupChat);
        connect(refreshBtn, &QPushButton::clicked, this, &ConversationsWindow::onRefresh);
        connect(createGroupBtn, &QPushButton::clicked, this, &ConversationsWindow::onCreateGroup);
        connect(exitBtn, &QPushButton::clicked, this, &ConversationsWindow::onExit);

        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &ConversationsWindow::onRefresh);
        refreshTimer->start(2000);

        QTimer::singleShot(100, this, &ConversationsWindow::onRefresh);
        
        printf("CONVERSATIONS constructor terminat\n");
        fflush(stdout);
    }

    ~ConversationsWindow()
    {
        if(refreshTimer) 
        {
            refreshTimer->stop();
        }
        if(chatWindow) 
        {
            delete chatWindow;
        }
    }

private slots:
    void onRefresh()
    {
        if(isRefreshing) 
        {
            printf("CONV refresh deja in curs\n");
            fflush(stdout);
            return;
        }
        
        isRefreshing = true;
        
        loadConversations();
        
        QTimer::singleShot(300, this, [this]() {
            loadGroups();
            isRefreshing = false;
            printf("CONV refresh done\n");
            fflush(stdout);
        });
    }

    void onCreateGroup()
    {
        bool ok;
        QString groupName = QInputDialog::getText(this, "Create Group", "Group name:", QLineEdit::Normal, "", &ok);
        
        if(!ok || groupName.trimmed().isEmpty())
        {
            return;
        }
        
        QString members = QInputDialog::getText(this, "Add Members", "Members (space-separated usernames):", QLineEdit::Normal, "", &ok);
        
        if(!ok)
        {
            return;
        }
        
        QString command = QString("CREATE_GROUP|%1|%2").arg(groupName.trimmed()).arg(members.trimmed());
        
        printf("CONV Creating group: %s\n", command.toUtf8().constData());
        fflush(stdout);
        
        if(client_send(command.toUtf8().constData()) != 0)
        {
            QMessageBox::critical(this, "Error", "Could not send command!");
            return;
        }
        
        char buffer[5000];
        memset(buffer, 0, sizeof(buffer));
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0)
        {
            QMessageBox::critical(this, "Error", "No response from server!");
            return;
        }
        
        QString response = QString::fromUtf8(buffer, bytes);
        
        if(response.startsWith("SUCCESS"))
        {
            QMessageBox::information(this, "Success", "Group created!");
            onRefresh();
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    void loadConversations()
    {
        printf("CONV trimit GET_CONVERSATIONS\n");
        fflush(stdout);
        
        if(client_send("GET_CONVERSATIONS") != 0) 
        {
            printf("CONV Eroare la trimitere GET_CONVERSATIONS\n");
            fflush(stdout);
            conversationsList->clear();
            conversationsList->addItem("#Eroare la conectare");
            privateUnreadCount = 0; 
            return;
        }

        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();

        char buffer[10000];
        memset(buffer, 0, sizeof(buffer));
        
        printf("CONV astept raspuns\n");
        fflush(stdout);
        
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            printf("CONV nu am primit raspuns\n");
            fflush(stdout);
            conversationsList->clear();
            conversationsList->addItem("#Nu s a primit raspuns");
            privateUnreadCount = 0;  
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        
        printf("CONV raspuns primit\n");
        fflush(stdout);

        conversationsList->clear();

        if(!response.startsWith("CONVERSATIONS:")) 
        {
            printf("CONV Format invalid\n");
            fflush(stdout);
            privateUnreadCount = 0; 
            return;
        }

        QString data = response.mid(14);
        
        printf("CONV date extrase: '%s'\n", data.left(100).toUtf8().constData());
        fflush(stdout);
        
        if(data.startsWith("NONE") || data.trimmed().isEmpty()) 
        {
            conversationsList->addItem("#Nicio conversatie");
            printf("CONV Nicio conversatie\n");
            fflush(stdout);
            privateUnreadCount = 0;  
            return;
        }

        QStringList conversations = data.split('|', Qt::SkipEmptyParts);
        int totalUnread = 0;
        
        printf("CONV gasit %d conversatii\n", conversations.size());
        fflush(stdout);

        for(int i = 0; i < conversations.size(); i++) 
        {
            QString conv = conversations[i].trimmed();
            
            printf("CONV conversatie %d: '%s'\n", i+1, conv.left(50).toUtf8().constData());
            fflush(stdout);
            
            QStringList parts = conv.split('~');
            
            if(parts.size() >= 4) 
            {
                QString username = parts[0].trimmed();
                int unread = parts[1].toInt();
                QString lastMsg = parts[2].trimmed();
                QString sender = parts[3].trimmed(); 
                
                if(lastMsg.length() > 40) 
                {
                    lastMsg = lastMsg.left(40) + "...";
                }

                QString displayText;
                
                if(unread > 0 && sender != "me")  
                {
                    displayText = QString("[NEW] %1\n   %2").arg(username).arg(lastMsg);
                    totalUnread += unread;
                    
                    QListWidgetItem *item = new QListWidgetItem(displayText);
                    item->setData(Qt::UserRole, username);
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                    conversationsList->addItem(item);
                }
                else
                {
                    displayText = QString("%1\n   %2").arg(username).arg(lastMsg);
                    
                    QListWidgetItem *item = new QListWidgetItem(displayText);
                    item->setData(Qt::UserRole, username);
                    conversationsList->addItem(item);
                }
            }
            else if(parts.size() >= 3) 
            {
                QString username = parts[0].trimmed();
                int unread = parts[1].toInt();
                QString lastMsg = parts[2].trimmed();
                
                if(lastMsg.length() > 40) 
                {
                    lastMsg = lastMsg.left(40) + "...";
                }

                QString displayText;
                if(unread > 0) 
                {
                    displayText = QString("[NEW] %1\n   %2").arg(username).arg(lastMsg);
                    totalUnread += unread;
                } 
                else 
                {
                    displayText = QString("%1\n   %2").arg(username).arg(lastMsg);
                }

                QListWidgetItem *item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, username);
            
                if(unread > 0) 
                {
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
                
                conversationsList->addItem(item);
            }
            else
            {
                printf("CONV Format invalid\n");
                fflush(stdout);
            }
        }

        privateUnreadCount = totalUnread;
        
        printf("CONV Total mesaje private necitite: %d\n", privateUnreadCount);
        fflush(stdout);

        if(totalUnread > 0) 
        {
            unreadLabel->setText(QString("Ai %1 mesaj%2 necitit%3!")
                .arg(totalUnread)
                .arg(totalUnread > 1 ? "e" : "")
                .arg(totalUnread > 1 ? "e" : ""));
            unreadLabel->show();
        } 
        else 
        {
            unreadLabel->hide();
        }
    }
    
    void loadGroups()
    {
        printf("GROUPS trimit GET_MY_GROUPS\n");
        fflush(stdout);
        
        if(client_send("GET_MY_GROUPS") != 0) 
        {
            printf("GROUPS EROARE la trimitere GET_MY_GROUPS\n");
            fflush(stdout);
            groupsList->clear();
            groupsList->addItem("#Eroare la conectare");
            return;
        }

        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();

        char buffer[10000];
        memset(buffer, 0, sizeof(buffer));
        
        printf("GROUPS astept raspuns GET_MY_GROUPS...\n");
        fflush(stdout);
        
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            printf("GROUPS nu am primit raspuns GET_MY_GROUPS\n");
            fflush(stdout);
            groupsList->clear();
            groupsList->addItem("#nu am primit raspuns");
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        
        printf("GROUPS raspuns primit: '%s'\n", response.left(200).toUtf8().constData());
        fflush(stdout);

        groupsList->clear();

        if(!response.startsWith("MY_GROUPS:")) 
        {
            printf("GROUPS format invalid\n");
            fflush(stdout);
            return;
        }

        QString data = response.mid(10); 
        
        if(data.startsWith("Nu faceti parte") || data.trimmed().isEmpty()) 
        {
            groupsList->addItem("Niciun grup");
            printf("GROUPS Niciun grup disponibil\n");
            fflush(stdout);
            
            if(privateUnreadCount > 0)
            {
                unreadLabel->setText(QString("Ai %1 mesaj%2 necitit%3!").arg(privateUnreadCount)
                                        .arg(privateUnreadCount > 1 ? "e" : "")
                                        .arg(privateUnreadCount > 1 ? "e" : ""));unreadLabel->show();
            }
            else
            {
                unreadLabel->hide();
            }
            return;
        }

        QStringList groups = data.split('|', Qt::SkipEmptyParts);
        
        printf("GROUPS Gasit %d grupuri\n", groups.size());
        fflush(stdout);
        
        int totalGroupUnread = 0;

        for(int i = 0; i < groups.size(); i++) 
        {
            QString group = groups[i].trimmed();
            
            printf("GROUPS Grup #%d: '%s'\n", i+1, group.toUtf8().constData());
            fflush(stdout);

            QStringList parts = group.split(':');
            if(parts.size() >= 4) 
            {
                QString groupName = parts[0].trimmed();
                QString creator = parts[1].trimmed();
                int unread = parts[2].toInt();
                QString lastMsg = parts[3].trimmed();
                
                printf("GROUPS   -> Nume: '%s', Creator: '%s', Necitite: %d, Msg: '%.30s'\n",
                    groupName.toUtf8().constData(), creator.toUtf8().constData(), unread, lastMsg.toUtf8().constData());
                fflush(stdout);
                
                if(lastMsg.length() > 40) 
                {
                    lastMsg = lastMsg.left(40) + "...";
                }

                QString displayText;
                if(unread > 0)
                {
                    displayText = QString("[NEW] %1\n   %2").arg(groupName)
                        .arg(lastMsg.isEmpty() ? "Niciun mesaj" : lastMsg);
                    totalGroupUnread += unread;
                }
                else
                {
                    displayText = QString("%1\n   %2").arg(groupName)
                        .arg(lastMsg.isEmpty() ? "Niciun mesaj" : lastMsg);
                }

                QListWidgetItem *item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, groupName);
                item->setData(Qt::UserRole + 1, creator);  
                
                if(unread > 0)
                {
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
                
                groupsList->addItem(item);
            }
            else
            {
                printf("GROUPS Format invalid pentru grup: %s\n", group.toUtf8().constData());
                fflush(stdout);
            }
        }
        
        int totalUnread = privateUnreadCount + totalGroupUnread;
        
        printf("GROUPS Calcul final: private=%d + grup=%d = TOTAL=%d\n", 
            privateUnreadCount, totalGroupUnread, totalUnread);
        fflush(stdout);
        
        if(totalUnread > 0)
        {
            unreadLabel->setText(QString("Ai %1 mesaj%2 necitit%3!").arg(totalUnread)
                .arg(totalUnread > 1 ? "e" : "").arg(totalUnread > 1 ? "e" : ""));
            unreadLabel->show();
        }
        else
        {
            unreadLabel->hide();
        }
    }

    void onOpenPrivateChat(QListWidgetItem *item)
    {
        QString username = item->data(Qt::UserRole).toString();
        username = username.trimmed().simplified();
        
        if(username.isEmpty() || username.startsWith("#")) 
        {
            return;
        }

        printf("CONVERSATIONS deschid chat cu %s\n", username.toUtf8().constData());
        fflush(stdout);

        if(refreshTimer) 
        {
            refreshTimer->stop();
        }

        if(chatWindow) 
        {
            delete chatWindow;
        }

        chatWindow = new ChatWindow(username, currentUsername, false);
        connect(chatWindow, &ChatWindow::returnToMain, this, &ConversationsWindow::onReturnFromChat);
        
        chatWindow->show();
        this->hide();
    }

    void onOpenGroupChat(QListWidgetItem *item)
    {
        QString groupName = item->data(Qt::UserRole).toString();
        QString creator = item->data(Qt::UserRole + 1).toString();
        
        if(groupName.isEmpty() || groupName.startsWith("#")) 
        {
            return;
        }

        printf("CONVERSATIONS deschid chat grup %s (creator: %s)\n", groupName.toUtf8().constData(), creator.toUtf8().constData());
        fflush(stdout);

        if(refreshTimer) 
        {
            refreshTimer->stop();
        }

        if(chatWindow) 
        {
            delete chatWindow;
        }

        chatWindow = new ChatWindow(groupName, currentUsername, true, creator, nullptr);
        connect(chatWindow, &ChatWindow::returnToMain, this, &ConversationsWindow::onReturnFromChat);
        
        chatWindow->show();
        this->hide();
    }

    void onReturnFromChat()
    {
        printf("CONVERSATIONS intoarcere din chat\n");
        fflush(stdout);
    
        if(chatWindow) 
        {
            chatWindow->deleteLater();
            chatWindow = nullptr;
        }
        
        this->show();
        this->raise();
        this->activateWindow();
        
        if(refreshTimer) 
        {
            if(refreshTimer->isActive())
            {
                refreshTimer->stop();
            }
            refreshTimer->start(2000);
        }
        
        QTimer::singleShot(100, this, &ConversationsWindow::onRefresh);
    }

    void onExit()
    {
        printf("CONVERSATIONS EXIT\n");
        fflush(stdout);
        
        if(refreshTimer)
        {
            refreshTimer->stop();
        }
        
        emit returnToMain();
        this->close();
    }
};

#endif