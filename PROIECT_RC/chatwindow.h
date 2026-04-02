#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QScreen>
#include <QDateTime>
#include <QSet>
#include <stdio.h>

extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
}

class ChatWindow : public QWidget
{
    Q_OBJECT

signals:
    void returnToMain();

private:
    QString prieten;
    bool isGroup;
    QString myUsername;    
    QTextEdit *chatDisplay;
    QLineEdit *messageInput;
    QPushButton *refreshBtn;
    QPushButton *exitBtn;
    QTimer *refreshTimer;
    int lastMessageCount;  
    bool isAdmin;
    QString creatorUsername;
    QPushButton *addMemberBtn;
    QPushButton *deleteGroupBtn;
    QPushButton *leaveGroupBtn;
    QSet<QString> myMessages;

public:
    ChatWindow(const QString& partner, const QString& myUsername, bool group = false, const QString& creator = "", QWidget *parent = nullptr) : 
            QWidget(parent), prieten(partner), isGroup(group), myUsername(myUsername), lastMessageCount(0), creatorUsername(creator)
    {
        printf("CHATWINDOW constructor pentru %s (grup: %d, creator: %s)\n", partner.toUtf8().constData(), group, creator.toUtf8().constData());
        fflush(stdout);
        
        isAdmin = (isGroup && !creator.isEmpty() && creator == myUsername);
        
        QString title = isGroup ? QString("Grup: %1").arg(prieten) : QString("Chat cu %1").arg(prieten);
        
        setWindowTitle(title);
        resize(700, 600);

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *headerLabel = new QLabel(title);
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setStyleSheet(
            "QLabel { "
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "  color: white; "
            "  font-size: 18px; "
            "  font-weight: bold; "
            "  padding: 15px; "
            "  border-radius: 8px; "
            "}"
        );
        mainLayout->addWidget(headerLabel);

        chatDisplay = new QTextEdit();
        chatDisplay->setReadOnly(true);
        chatDisplay->setStyleSheet(
            "QTextEdit { "
            "  background-color: #f5f5f5; "
            "  border: 1px solid #ddd; "
            "  border-radius: 8px; "
            "  padding: 10px; "
            "  font-size: 13px; "
            "}"
        );
        mainLayout->addWidget(chatDisplay);

        QFrame *inputFrame = new QFrame();
        inputFrame->setStyleSheet(
            "QFrame { "
            "  background-color: white; "
            "  border: 2px solid #191b25ff; "
            "  border-radius: 8px; "
            "  padding: 10px; "
            "}"
        );
        QHBoxLayout *inputLayout = new QHBoxLayout(inputFrame);

        messageInput = new QLineEdit();
        messageInput->setPlaceholderText("Scrie un mesaj...");
        messageInput->setStyleSheet(
            "QLineEdit { "
            "  border: 1px solid #ccc; "
            "  border-radius: 4px; "
            "  padding: 8px; "
            "  font-size: 13px; "
            "}"
        );
        inputLayout->addWidget(messageInput);

        mainLayout->addWidget(inputFrame);

        QFrame *buttonFrame = new QFrame();
        QHBoxLayout *buttonLayout = new QHBoxLayout(buttonFrame);

        refreshBtn = new QPushButton("Refresh");
        refreshBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #48bb78; "
            "  color: white; "
            "  padding: 8px 15px; "
            "  border: none; "
            "  border-radius: 4px; "
            "} "
            "QPushButton:hover { background-color: #38a169; }"
        );
        buttonLayout->addWidget(refreshBtn);

        if(isGroup)
        {
            if(isAdmin)
            {
                addMemberBtn = new QPushButton("Add Member");
                addMemberBtn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #667eea; "
                    "  color: white; "
                    "  padding: 8px 15px; "
                    "  border: none; "
                    "  border-radius: 4px; "
                    "} "
                );
                buttonLayout->addWidget(addMemberBtn);
                connect(addMemberBtn, &QPushButton::clicked, this, &ChatWindow::onAddMember);
                
                deleteGroupBtn = new QPushButton("Delete Group");
                deleteGroupBtn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #e53e3e; "
                    "  color: white; "
                    "  padding: 8px 15px; "
                    "  border: none; "
                    "  border-radius: 4px; "
                    "} "
                );
                buttonLayout->addWidget(deleteGroupBtn);
                connect(deleteGroupBtn, &QPushButton::clicked, this, &ChatWindow::onDeleteGroup);
            }
            else
            {
                leaveGroupBtn = new QPushButton("Leave Group");
                leaveGroupBtn->setStyleSheet(
                    "QPushButton { "
                    "  background-color: #ed8936; "
                    "  color: white; "
                    "  padding: 8px 15px; "
                    "  border: none; "
                    "  border-radius: 4px; "
                    "} "
                );
                buttonLayout->addWidget(leaveGroupBtn);
                connect(leaveGroupBtn, &QPushButton::clicked, this, &ChatWindow::onLeaveGroup);
            }
        }

        buttonLayout->addStretch();

        exitBtn = new QPushButton("EXIT");
        exitBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #e53e3e; "
            "  color: white; "
            "  padding: 8px 15px; "
            "  border: none; "
            "  border-radius: 4px; "
            "} "
            "QPushButton:hover { background-color: #c53030; }"
        );
        buttonLayout->addWidget(exitBtn);

        mainLayout->addWidget(buttonFrame);

        connect(messageInput, &QLineEdit::returnPressed, this, &ChatWindow::onSendMessage);
        connect(refreshBtn, &QPushButton::clicked, this, &ChatWindow::onRefresh);
        connect(exitBtn, &QPushButton::clicked, this, &ChatWindow::onExit);

        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &ChatWindow::checkIfNeedRefresh);
        refreshTimer->start(100);

        onRefresh();
        
        printf("CHATWINDOW constructor terminat\n");
        fflush(stdout);
    }

    ~ChatWindow()
    {
        if(refreshTimer) 
        {
            refreshTimer->stop();
        }
    }

private slots:
    void onSendMessage()
    {
        QString message = messageInput->text().trimmed();
        
        if(message.isEmpty()) 
        {
            QMessageBox::warning(this, "Atentie", "Mesajul nu poate fi gol");
            return;
        }

        QString command;
        if(isGroup) 
        {
            command = QString("SEND_GROUP_MESSAGE|%1|%2").arg(prieten).arg(message);
        } 
        else 
        {
            command = QString("SEND_MESSAGE|%1|%2").arg(prieten).arg(message);
        }

        printf("CHATWINDOW trimit: %s\n", command.toUtf8().constData());
        fflush(stdout);

        if(client_send(command.toUtf8().constData()) != 0) 
        {
            QMessageBox::critical(this, "Eroare", "Nu s a putut trimite mesajul");
            return;
        }

        char buffer[5000];
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            QMessageBox::critical(this, "Eroare", "Nu s a primit raspuns");
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        
        if(response.startsWith("SUCCESS")) 
        {
            myMessages.insert(message); 

            QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
            chatDisplay->append(
                QString("<div style='text-align: left; margin: 5px;'>"
                        "<span style='background-color: #b97171ff; color: white; "
                        "padding: 8px 12px; border-radius: 12px; display: inline-block;'>"
                        "<b>Tu</b> [%1]<br>%2</span></div>").arg(timestamp)
                .arg(message.toHtmlEscaped())
            );
            
            messageInput->clear();
            lastMessageCount++;
            
            QScrollBar *scrollBar = chatDisplay->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }
        else
        {
            messageInput->clear();
        }
    }

    void checkIfNeedRefresh()
    {
        QString command;
        if(isGroup) 
        {
            command = QString("GET_GROUP_MESSAGES|%1").arg(prieten);
        } 
        else 
        {
            command = QString("GET_MESSAGES|%1").arg(prieten);
        }

        if(client_send(command.toUtf8().constData()) != 0) 
        {
            return;
        }

        char buffer[30000];
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);
        
        int currentMessageCount = countMessages(response);
        
        if(currentMessageCount != lastMessageCount)
        {
            printf("CHATWINDOW: Mesaje noi detectate (%d -> %d), refresh...\n", lastMessageCount, currentMessageCount);
            fflush(stdout);
            
            lastMessageCount = currentMessageCount;
            displayMessages(response);
        }
    }


    int countMessages(const QString& response)
    {
        if(response.startsWith("ERROR")) 
        {
            return 0;
        }

        if(isGroup) 
        {
            if(!response.startsWith("GROUP_MESSAGES")) 
            {
                return 0;
            }
            
            QString data = response.mid(15);
            if(data.contains("Niciun mesaj")) 
            {
                return 0;
            }
            
            return data.split('|', Qt::SkipEmptyParts).count();
        } 
        else 
        {
            if(!response.startsWith("MESSAGES:")) 
            {
                return 0;
            }
            
            QString data = response.mid(9).trimmed();
            if(data.contains("Niciun mesaj") || data.isEmpty()) 
            {
                return 0;
            }
            
            return data.split('#', Qt::SkipEmptyParts).count();
        }
    }

    void onRefresh()
    {
        QString command;
        if(isGroup) 
        {
            command = QString("GET_GROUP_MESSAGES|%1").arg(prieten);
        } 
        else 
        {
            command = QString("GET_MESSAGES|%1").arg(prieten);
        }

        printf("CHATWINDOW refresh: %s\n", command.toUtf8().constData());
        fflush(stdout);

        if(client_send(command.toUtf8().constData()) != 0) 
        {
            return;
        }

        char buffer[30000];
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            return;
        }

        QString response = QString::fromUtf8(buffer, bytes);

        extractMyMessages(response);
        displayMessages(response);
    }

    void onExit()
    {
        printf("CHATWINDOW EXIT\n");
        fflush(stdout);
        
        if(refreshTimer) 
        {
            refreshTimer->stop();
        }
        
        emit returnToMain();
        this->close();
    }

    void extractMyMessages(const QString& response)
    {
        if(myUsername.isEmpty()) return;
        
        if(isGroup) 
        {
            if(!response.startsWith("GROUP_MESSAGES")) return;
            
            QString data = response.mid(15);
            QStringList messages = data.split('|', Qt::SkipEmptyParts);
            
            for(const QString& msg : messages) 
            {
                QStringList lines = msg.split('\n', Qt::SkipEmptyParts);
                if(lines.size() >= 2) 
                {
                    QString senderAndMsg = lines[0];
                    int colonPos = senderAndMsg.indexOf(':');
                    if(colonPos > 0) 
                    {
                        QString sender = senderAndMsg.left(colonPos).trimmed();
                        QString message = senderAndMsg.mid(colonPos + 1).trimmed();
                        
                        if(sender == myUsername) 
                        {
                            myMessages.insert(message);
                        }
                    }
                }
            }
        } 
        else 
        {
            if(!response.startsWith("MESSAGES:")) return;
            
            QString data = response.mid(9).trimmed();
            QStringList messages = data.split('#', Qt::SkipEmptyParts);
            
            for(const QString& msg : messages) 
            {
                QStringList parts = msg.split("||", Qt::SkipEmptyParts);
                if(parts.size() >= 3) 
                {
                    QString sender = parts[0].trimmed();
                    QString messageText = parts[1].trimmed();
                    
                    if(sender == myUsername) 
                    {
                        myMessages.insert(messageText);
                    }
                }
            }
        }
    }

    void displayMessages(const QString& response)
    {
        if(response.startsWith("ERROR")) 
        {
            return; 
        }

        if(isGroup) 
        {
            if(!response.startsWith("GROUP_MESSAGES")) 
            {
                return;  
            }
        } 
        else 
        {
            if(!response.startsWith("MESSAGES:")) 
            {
                return;  
            }
        }

        chatDisplay->clear();

        if(response.startsWith("ERROR")) 
        {
            return;
        }

        if(isGroup) 
        {
            if(!response.startsWith("GROUP_MESSAGES")) return;
            
            QString data = response.mid(15); 
            
            if(data.contains("Niciun mesaj")) 
            {
                chatDisplay->append("<div style='text-align: center; color: #999; padding: 20px;'>"
                                    "Niciun mesaj în acest grup</div>");
                return;
            }

            QStringList messages = data.split('|', Qt::SkipEmptyParts);
            
            for(const QString& msg : messages) 
            {
                QStringList lines = msg.split('\n', Qt::SkipEmptyParts);
                if(lines.size() >= 2) 
                {
                    QString senderAndMsg = lines[0];
                    QString timestamp = lines[1];
                    timestamp.remove('[').remove(']');
                    
                    int colonPos = senderAndMsg.indexOf(':');
                    if(colonPos > 0) 
                    {
                        QString sender = senderAndMsg.left(colonPos).trimmed();
                        QString message = senderAndMsg.mid(colonPos + 1).trimmed();

                        bool isMyMessage = myMessages.contains(message);
                        
                        if(isMyMessage)
                        {
                            chatDisplay->append(QString("<div style='text-align: left; margin: 5px;'>"
                                        "<span style='background-color: #b97171ff; color: white; "
                                        "padding: 8px 12px; border-radius: 12px; display: inline-block;'>"
                                        "<b>Tu</b> [%1]<br>%2</span></div>")
                                        .arg(timestamp).arg(message.toHtmlEscaped()));
                        }
                        else
                        {
                            chatDisplay->append(QString("<div style='margin: 8px;'>"
                                    "<span style='background-color: #e2e8f0; "
                                    "padding: 8px 12px; border-radius: 12px; display: inline-block;'>"
                                    "<b>%1</b> <span style='color: #666; font-size: 11px;'>[%2]</span><br>"
                                    "%3</span></div>")
                                    .arg(sender).arg(timestamp).arg(message.toHtmlEscaped()));
                        }
                    }
                }
            }
        } 
        else 
        {
            if(!response.startsWith("MESSAGES:")) 
            {
                return;
            }
            
            QString data = response.mid(9);
            data = data.trimmed();
            
            if(data.contains("Niciun mesaj") || data.isEmpty())
            {
                chatDisplay->append("<div style='text-align: left; color: #999; padding: 20px;'>"
                                "Niciun mesaj cu aceasta persoana</div>");
                return;
            }

            QStringList messages = data.split('#', Qt::SkipEmptyParts);
            
            for(const QString& msg : messages) 
            {
                QStringList parts = msg.split("||", Qt::SkipEmptyParts);
                
                if(parts.size() < 3) 
                {
                    printf("CHATWINDOW: Format invalid: %s\n", msg.toUtf8().constData());
                    continue;
                }
                
                QString sender = parts[0].trimmed();
                QString messageText = parts[1].trimmed();
                QString timestamp = parts[2].trimmed();

                bool isMyMessage = myMessages.contains(messageText);  
                                
                if(isMyMessage)
                {
                    chatDisplay->append(QString("<div style='text-align: left; margin: 5px;'>"
                                "<span style='background-color: #b97171ff; color: white; "
                                "padding: 8px 12px; border-radius: 12px; display: inline-block;'>"
                                "<b>Tu</b> [%1]<br>%2</span></div>")
                                .arg(timestamp).arg(messageText.toHtmlEscaped()));
                }
                else
                {
                    chatDisplay->append(QString("<div style='text-align: left; margin: 5px;'>"
                                "<span style='background-color: #e2e8f0; "
                                "padding: 8px 12px; border-radius: 12px; display: inline-block;'>"
                                "<b>%1</b> <span style='color: #666; font-size: 11px;'>[%2]</span><br>"
                                "%3</span></div>")
                                .arg(sender).arg(timestamp).arg(messageText.toHtmlEscaped()));
                }
            }
        }

        QScrollBar *scrollBar = chatDisplay->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    void onAddMember()
    {
        bool ok;
        QString username = QInputDialog::getText(this, "Add Member", "Username to add:", QLineEdit::Normal, "", &ok);
        
        if(!ok || username.trimmed().isEmpty())
        {
            return;
        }
        
        QString command = QString("ADD_GROUP_MEMBER|%1|%2").arg(prieten).arg(username.trimmed());
        
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
            QMessageBox::information(this, "Success", QString("%1 added to group!").arg(username));
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    void onDeleteGroup()
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Group", 
            QString("Are you sure you want to delete the group '%1'? This cannot be undone! All messages will be lost!").arg(prieten),
            QMessageBox::Yes | QMessageBox::No);
        
        if(reply != QMessageBox::Yes)
        {
            return;
        }
        
        QString command = QString("DELETE_GROUP|%1").arg(prieten);
        
        printf("CHATWINDOW deleting group: %s\n", command.toUtf8().constData());
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
            if(refreshTimer) 
            {
                refreshTimer->stop();
            }
            
            QMessageBox::information(this, "Success", "Group deleted successfully!");
            
            emit returnToMain();
            this->close();
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    void onLeaveGroup()
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Leave Group", 
            QString("Are you sure you want to leave the group '%1'?").arg(prieten),
            QMessageBox::Yes | QMessageBox::No);
        
        if(reply != QMessageBox::Yes)
        {
            return;
        }
        
        QString command = QString("LEAVE_GROUP|%1").arg(prieten);
        
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
            if(refreshTimer) 
            {
                refreshTimer->stop();
            }
            
            QMessageBox::information(this, "Success", "You left the group!");
            
            emit returnToMain();
            this->close();
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    
};

#endif