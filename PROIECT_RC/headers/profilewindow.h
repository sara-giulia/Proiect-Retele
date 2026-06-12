#ifndef PROFILEWINDOW_H
#define PROFILEWINDOW_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPixmap>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QPushButton>   
#include <QTimer> 
#include <stdio.h>


extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
}

class ProfileWindow : public QWidget
{
    Q_OBJECT

signals:
    void returnToMain();

private:
    QScrollArea *scrollArea;
    QWidget *contentContainer;
    QVBoxLayout *contentLayout;
    QLineEdit *commandInput;
    QPushButton *refreshBtn;      
    QTimer *refreshTimer;       
    QString currentUser;       
    bool isMyProfile;
    QPushButton *addFriendBtn;        
    QPushButton *addCloseFriendBtn;  
    QString viewedUsername;  

public:
    ProfileWindow(QWidget *parent = nullptr) : QWidget(parent)
    {
        printf("PROFILEWINDOW constructor apelat\n");
        fflush(stdout);

        currentUser = "";
        isMyProfile = false;
        addFriendBtn = nullptr;     
        addCloseFriendBtn = nullptr;
        
        setWindowTitle("Profil Utilizator");
        resize(800, 650);

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);

        scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);

        contentContainer = new QWidget();
        contentLayout = new QVBoxLayout(contentContainer);
        contentLayout->setAlignment(Qt::AlignTop);
        contentLayout->setSpacing(15);

        scrollArea->setWidget(contentContainer);
        mainLayout->addWidget(scrollArea);

        QFrame *commandFrame = new QFrame();
        commandFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        commandFrame->setStyleSheet(
            "QFrame { "
            "  background-color: #f0f0f0; "
            "  border: 1px solid #ccc; "
            "  border-radius: 5px; "
            "  padding: 10px; "
            "}"
        );

        QHBoxLayout *commandLayout = new QHBoxLayout(commandFrame);
        
        commandInput = new QLineEdit();
        commandInput->setPlaceholderText("Scrie EXIT pentru a reveni la meniul principal");
        commandInput->setStyleSheet(
            "QLineEdit { "
            "  padding: 8px; "
            "  font-size: 13px; "
            "  border: 1px solid #999; "
            "  border-radius: 4px; "
            "}"
        );

        refreshBtn = new QPushButton("Refresh");
        refreshBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #2196F3; "
            "  color: white; "
            "  font-weight: bold; "
            "  padding: 8px 15px; "
            "  border: none; "
            "  border-radius: 4px; "
            "} "
            "QPushButton:hover { background-color: #1976D2; }"
        );
        
        commandLayout->addWidget(refreshBtn);
        commandLayout->addWidget(commandInput);

        mainLayout->addWidget(commandFrame);

        connect(commandInput, &QLineEdit::returnPressed, this, &ProfileWindow::onExit);
        connect(refreshBtn, &QPushButton::clicked, this, &ProfileWindow::onRefresh);

        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &ProfileWindow::onRefresh);
        refreshTimer->start(15000);

        contentLayout->update();
        contentContainer->updateGeometry();
        contentContainer->adjustSize();
        scrollArea->updateGeometry();
        scrollArea->update();
        this->update();
        this->repaint();
        QApplication::processEvents();
        
        printf("PROFILEWINDOW constructor terminat\n");
        fflush(stdout);
    }

    ~ProfileWindow()
    {
        if(refreshTimer) 
        {
            refreshTimer->stop();
        }
    }

    void displayMyProfile(const QString& response)
    {
        printf("PROFILEWINDOW displayMyProfile() apelat\n");
        fflush(stdout);

        isMyProfile = true;
        currentUser = "";

        QLayoutItem *item;
        while ((item = contentLayout->takeAt(0)) != nullptr) 
        {
            delete item->widget();
            delete item;
        }

        QString cleanResponse = response.trimmed();
        
        if(cleanResponse.startsWith("ERROR|")) 
        {
            QString errorMsg = cleanResponse.mid(6);
            QLabel *errorLabel = new QLabel(errorMsg);
            errorLabel->setStyleSheet("color: red; font-weight: bold; padding: 20px;");
            errorLabel->setAlignment(Qt::AlignCenter);
            contentLayout->addWidget(errorLabel);
            contentContainer->update();
            scrollArea->update();
            this->update();
            QApplication::processEvents();
            contentContainer->update();
            scrollArea->update();
            this->update();
            return;
        }
        
        if(!cleanResponse.startsWith("MY_PROFILE:")) 
        {
            QLabel *errorLabel = new QLabel("Format invalid de raspuns");
            errorLabel->setStyleSheet("color: red; padding: 20px;");
            contentLayout->addWidget(errorLabel);
            contentContainer->update();
            scrollArea->update();
            this->update();
            QApplication::processEvents();
            return;
        }

        QString data = cleanResponse.mid(11); 
        QStringList parts = data.split('|');
        
        if(parts.size() < 5) 
        {
            QLabel *errorLabel = new QLabel("Date incomplete");
            contentLayout->addWidget(errorLabel);
            contentContainer->update();
            scrollArea->update();
            this->update();
            QApplication::processEvents();
            return;
        }

        QString username = parts[0];
        QString userType = parts[1];
        QString profileStatus = parts[2];
        QString friendsData = parts[3];
        QString postsData = parts[4];
        
        bool isAdmin = (userType == "admin");

        QFrame *headerFrame = new QFrame();
        headerFrame->setStyleSheet(
            "QFrame { "
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
            "stop:0 #2196F3, stop:1 #1976D2); "
            "border-radius: 10px; "
            "padding: 20px; "
            "}"
        );
        
        QVBoxLayout *headerLayout = new QVBoxLayout(headerFrame);
        
        QLabel *nameLabel = new QLabel(username);
        nameLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
        headerLayout->addWidget(nameLabel);
        
        QLabel *typeLabel = new QLabel(QString("Tip: %1  |  Profil: %2").arg(userType.toUpper()).arg(profileStatus));
        typeLabel->setStyleSheet("color: white; font-size: 14px;");
        headerLayout->addWidget(typeLabel);
        
        contentLayout->addWidget(headerFrame);

        QFrame *friendsFrame = new QFrame();
        friendsFrame->setFrameStyle(QFrame::Box);
        friendsFrame->setStyleSheet(
            "QFrame { "
            "  background-color: #fff3e0; "
            "  border: 2px solid #ff9800; "
            "  border-radius: 8px; "
            "  padding: 15px; "
            "}"
        );
        
        QVBoxLayout *friendsLayout = new QVBoxLayout(friendsFrame);
        
        QLabel *friendsTitle = new QLabel("<b>Prietenii mei</b>");
        friendsTitle->setStyleSheet("font-size: 16px;");
        friendsLayout->addWidget(friendsTitle);
        
        if(friendsData == "NO_FRIENDS") 
        {
            QLabel *noFriendsLabel = new QLabel("Nu ai inca prieteni");
            noFriendsLabel->setStyleSheet("color: #666; font-style: italic;");
            friendsLayout->addWidget(noFriendsLabel);
        } 
        else 
        {
            QStringList friends = friendsData.split(';', Qt::SkipEmptyParts);
            
            for(const QString& friendEntry : friends) 
            {
                QStringList friendParts = friendEntry.split(':');
                if(friendParts.size() >= 2) 
                {
                    QString friendName = friendParts[0];
                    QString friendType = friendParts[1];
                    
                    QString typeText = (friendType == "CLOSE") ? "Close Friend" : "Friend";
                    
                    QLabel *friendLabel = new QLabel(QString("%1 <b>%2</b>").arg(friendName).arg(typeText));
                    friendLabel->setStyleSheet("padding: 5px;");
                    friendsLayout->addWidget(friendLabel);
                }
            }
        }
        
        contentLayout->addWidget(friendsFrame);

        QLabel *postsTitle = new QLabel("<b>Postarile mele</b>");
        postsTitle->setStyleSheet("font-size: 18px; margin-top: 10px;");
        contentLayout->addWidget(postsTitle);
        
        if(postsData == "NO_POSTS") 
        {
            QLabel *noPostsLabel = new QLabel("Nu ai inca postari");
            noPostsLabel->setStyleSheet("color: #666; font-style: italic; padding: 20px;");
            noPostsLabel->setAlignment(Qt::AlignCenter);
            contentLayout->addWidget(noPostsLabel);
        } 
        else 
        {
            QStringList posts = postsData.split('#', Qt::SkipEmptyParts);
            
            for(const QString& post : posts) 
            {
                QStringList fields = post.split('~');
                
                if(fields.size() < 6) continue;
                
                QString postId = fields[0];
                QString user = fields[1];
                QString imagePath = fields[2];
                QString text = fields[3];
                QString postType = fields[4];
                QString timestamp = fields[5];
                
                QFrame *postFrame = new QFrame();
                postFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
                postFrame->setStyleSheet(
                    "QFrame { "
                    "  background-color: white; "
                    "  border: 2px solid #e0e0e0; "
                    "  border-radius: 10px; "
                    "  padding: 15px; "
                    "}"
                );
                
                QVBoxLayout *postLayout = new QVBoxLayout(postFrame);
                
                QString headerText;
                if(isAdmin)
                {
                    headerText = QString("%1 | %2 | <span style='color:#e53e3e;'>ID: %3</span>").arg(postType.toUpper()).arg(timestamp).arg(postId);
                }
                else
                {
                    headerText = QString("%1 | %2").arg(postType.toUpper()).arg(timestamp);
                }
                
                QLabel *postHeader = new QLabel(headerText);
                postLayout->addWidget(postHeader);
                
                if(!imagePath.isEmpty()) 
                {
                    QFileInfo fileInfo(imagePath);
                    if(fileInfo.exists()) 
                    {
                        QPixmap pixmap(imagePath);
                        if(!pixmap.isNull()) 
                        {
                            if(pixmap.width() > 500) 
                            {
                                pixmap = pixmap.scaledToWidth(500, Qt::SmoothTransformation);
                            }
                            QLabel *imageLabel = new QLabel();
                            imageLabel->setPixmap(pixmap);
                            imageLabel->setAlignment(Qt::AlignCenter);
                            postLayout->addWidget(imageLabel);
                        }
                    }
                }
                
                if(!text.isEmpty()) 
                {
                    QLabel *textLabel = new QLabel(text);
                    textLabel->setWordWrap(true);
                    textLabel->setStyleSheet("padding: 10px; background-color: #f9f9f9;");
                    postLayout->addWidget(textLabel);
                }
                
                contentLayout->addWidget(postFrame);
            }
        }
        
        contentLayout->update();
        contentContainer->updateGeometry();
        contentContainer->adjustSize();
        scrollArea->updateGeometry();
        scrollArea->update();
        this->update();
        this->repaint();
        QApplication::processEvents();
        printf("PROFILEWINDOW profil afisat\n");
        fflush(stdout);
    }

    void displayUserFeed(const QString& data)
    {
        printf("PROFILEWINDOW displayUserFeed() called\n");
        fflush(stdout);
        
        isMyProfile = false;
        
        QLayoutItem *item;
        while ((item = contentLayout->takeAt(0)) != nullptr) 
        {
            delete item->widget();
            delete item;
        }
        
        QString content = data.mid(10);
        
        QStringList parts = content.split("|");
        if(parts.size() < 4)
        {
            QLabel *errorLabel = new QLabel("ERROR: Invalid user feed format");
            errorLabel->setStyleSheet("color: red; padding: 20px;");
            contentLayout->addWidget(errorLabel);
            return;
        }
        
        viewedUsername = parts[0].trimmed();
        currentUser = viewedUsername;
        QString relation = parts[1].trimmed();
        QString profileStatus = parts[2].trimmed();
        QString viewerType = parts[3].trimmed(); 
    
        bool isAdmin = (viewerType == "ADMIN");
        
        QFrame *headerFrame = new QFrame();
        headerFrame->setStyleSheet(
            "QFrame { "
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
            "    stop:0 #667eea, stop:1 #764ba2); "
            "  border-radius: 10px; "
            "  padding: 20px; "
            "}"
        );
        
        QVBoxLayout *headerLayout = new QVBoxLayout(headerFrame);
        
        QLabel *nameLabel = new QLabel(viewedUsername + "'s Profile");
        nameLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
        headerLayout->addWidget(nameLabel);
        
        QString relationText = (relation == "none") ? "Not Friends" : (relation == "CLOSE_FRIEND") ? "Close Friend" : "Friend";
        
        QLabel *infoLabel = new QLabel(QString("Relation: %1  |  Profile: %2").arg(relationText).arg(profileStatus));
        infoLabel->setStyleSheet("color: white; font-size: 14px;");
        headerLayout->addWidget(infoLabel);
        
        contentLayout->addWidget(headerFrame);
        
        QWidget *buttonWidget = new QWidget();
        QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(0, 10, 0, 10);
        
        addFriendBtn = new QPushButton("Add as FRIEND");
        addFriendBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #48bb78; "
            "  color: white; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "  font-size: 14px; "
            "} "
            "QPushButton:hover { background-color: #38a169; } "
        );
        
        addCloseFriendBtn = new QPushButton("Add as CLOSE FRIEND");
        addCloseFriendBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #ed8936; "
            "  color: white; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "  font-size: 14px; "
            "} "
            "QPushButton:hover { background-color: #dd6b20; } "
        );
        
        QPushButton *removeFriendBtn = new QPushButton("Remove Friend");
        removeFriendBtn->setStyleSheet(
            "QPushButton { "
            "  background-color: #e53e3e; "
            "  color: white; "
            "  padding: 10px 20px; "
            "  border: none; "
            "  border-radius: 5px; "
            "  font-weight: bold; "
            "  font-size: 14px; "
            "} "
            "QPushButton:hover { background-color: #c53030; } "
        );
        
        if(relation == "CLOSE_FRIEND")
        {
            addFriendBtn->setText("Change to FRIEND");
            addCloseFriendBtn->setText("Keep as CLOSE FRIEND");
            removeFriendBtn->setVisible(true);
        }
        else if(relation == "FRIEND")
        {
            addFriendBtn->setText("Keep as FRIEND");
            addCloseFriendBtn->setText("Upgrade to CLOSE FRIEND");
            removeFriendBtn->setVisible(true);
        }
        else
        {
            removeFriendBtn->setVisible(false);
        }
        
        connect(addFriendBtn, &QPushButton::clicked, this, &ProfileWindow::onAddFriendClicked);
        connect(addCloseFriendBtn, &QPushButton::clicked, this, &ProfileWindow::onAddCloseFriendClicked);
        connect(removeFriendBtn, &QPushButton::clicked, this, [this]() 
        {
            printf("Removing friend %s\n", viewedUsername.toUtf8().constData());
            fflush(stdout);
            
            QString command = QString("DELETE_FRIEND|%1").arg(viewedUsername);
            
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
                QMessageBox::information(this, "Success", QString("%1 removed from friends!").arg(viewedUsername));
                onRefresh();
            }
            else
            {
                QMessageBox::warning(this, "Error", response);
            }
        });
        
        buttonLayout->addWidget(addFriendBtn);
        buttonLayout->addWidget(addCloseFriendBtn);
        buttonLayout->addWidget(removeFriendBtn);
        buttonLayout->addStretch();
        
        contentLayout->addWidget(buttonWidget);

        QLabel *postsTitle = new QLabel("<b>Posts</b>");
        postsTitle->setStyleSheet("font-size: 18px; margin-top: 10px;");
        contentLayout->addWidget(postsTitle);
        
        if(parts.size() > 4)
        {
            QString postsData = parts.mid(4).join("|");
            
            if(postsData.trimmed() == "NO_POSTS" || postsData.isEmpty())
            {
                QLabel *noPostsLabel = new QLabel("No posts available");
                noPostsLabel->setStyleSheet("color: #666; font-style: italic; padding: 20px;");
                noPostsLabel->setAlignment(Qt::AlignCenter);
                contentLayout->addWidget(noPostsLabel);
            }
            else
            {
                QStringList posts = postsData.split("|");
                for(const QString& post : posts)
                {
                    if(post.isEmpty()) continue;
                    
                    QStringList postParts = post.split("~");
                    if(postParts.size() >= 6)
                    {
                        QString postId = postParts[0];
                        QString author = postParts[1];
                        QString imagePath = postParts[2];
                        QString text = postParts[3];
                        QString type = postParts[4];
                        QString timestamp = postParts[5];
                        
                        QFrame *postFrame = new QFrame();
                        postFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
                        postFrame->setStyleSheet(
                            "QFrame { "
                            "  background-color: white; "
                            "  border: 2px solid #e0e0e0; "
                            "  border-radius: 10px; "
                            "  padding: 15px; "
                            "}"
                        );
                        
                        QVBoxLayout *postLayout = new QVBoxLayout(postFrame);

                        QString headerText;
                        if(isAdmin)
                        {
                            headerText = QString("<b>%1</b> | %2 | <span style='color:#e53e3e;'>ID: %3</span>").arg(author).arg(timestamp).arg(postId);
                        }
                        else
                        {
                            headerText = QString("<b>%1</b> | %2").arg(author).arg(timestamp);
                        }
                        
                        QLabel *postHeader = new QLabel(headerText);
                        postHeader->setStyleSheet("color: #4a5568;");
                        postLayout->addWidget(postHeader);
                        
                        if(!imagePath.isEmpty())
                        {
                            QFileInfo fileInfo(imagePath);
                            if(fileInfo.exists())
                            {
                                QPixmap pixmap(imagePath);
                                if(!pixmap.isNull())
                                {
                                    if(pixmap.width() > 500)
                                    {
                                        pixmap = pixmap.scaledToWidth(500, Qt::SmoothTransformation);
                                    }
                                    QLabel *imageLabel = new QLabel();
                                    imageLabel->setPixmap(pixmap);
                                    imageLabel->setAlignment(Qt::AlignCenter);
                                    postLayout->addWidget(imageLabel);
                                }
                            }
                        }
                        
                        if(!text.isEmpty())
                        {
                            QLabel *textLabel = new QLabel(text);
                            textLabel->setWordWrap(true);
                            textLabel->setStyleSheet("padding: 10px; background-color: #f9f9f9;");
                            postLayout->addWidget(textLabel);
                        }
                        
                        QLabel *typeLabel = new QLabel(QString("Type: %1").arg(type));
                        typeLabel->setStyleSheet("font-size: 12px; color: #a0aec0;");
                        postLayout->addWidget(typeLabel);
                        
                        contentLayout->addWidget(postFrame);
                    }
                }
            }
        }
        
        contentLayout->update();
        contentContainer->updateGeometry();
        contentContainer->adjustSize();
        scrollArea->updateGeometry();
        scrollArea->update();
        this->update();
        this->repaint();
        QApplication::processEvents();
        
        printf("PROFILEWINDOW user feed displayed\n");
        fflush(stdout);
    }

private slots:
    void onRefresh()
    {
        printf("PROFILEWINDOW refresh\n");
        fflush(stdout);
        
        QString command;
        
        if(isMyProfile) 
        {
            command = "VIEW_MY_PROFILE";
        } 
        else if(!currentUser.isEmpty()) 
        {
            command = QString("VIEW_FEED|%1").arg(currentUser);
        } 
        else 
        {
            printf("PROFILEWINDOW eroare refresh\n");
            return;
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
        
        if(isMyProfile) 
        {
            displayMyProfile(response);
        } 
        else 
        {
            displayUserFeed(response);
        }
    }

    void onAddFriendClicked()
    {
        printf("Adding %s as FRIEND\n", viewedUsername.toUtf8().constData());
        fflush(stdout);
        
        QString command = QString("ADD_FRIEND|%1|FRIEND").arg(viewedUsername);

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
            QMessageBox::information(this, "Success", QString("%1 is now a FRIEND!").arg(viewedUsername));
            
            onRefresh();
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    void onAddCloseFriendClicked()
    {
        printf("Adding %s as CLOSE_FRIEND\n", viewedUsername.toUtf8().constData());
        fflush(stdout);
        
        QString command = QString("ADD_FRIEND|%1|CLOSE_FRIEND").arg(viewedUsername);
        
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
            QMessageBox::information(this, "Success", QString("%1 is now a CLOSE FRIEND!").arg(viewedUsername));
            
            onRefresh();
        }
        else
        {
            QMessageBox::warning(this, "Error", response);
        }
    }

    void onExit()
    {
        QString command = commandInput->text().trimmed().toUpper();
        
        if(command.isEmpty() || command == "EXIT") 
        {
            printf("PROFILEWINDOW EXIT\n");
            fflush(stdout);

            if(refreshTimer) 
            {
                refreshTimer->stop();
            }
            
            emit returnToMain();
            this->close();
        } 
        else 
        {
            QMessageBox::warning(this, "Atentie", "Din aceasta fereastra poti folosi doar EXIT!");
            commandInput->clear();
        }
    }
};

#endif