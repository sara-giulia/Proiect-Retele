#ifndef POSTSWINDOW_H
#define POSTSWINDOW_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPixmap>
#include <QFileInfo>
#include <QDebug>
#include <QLineEdit>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QPushButton>  
#include <QThread> 
#include <stdio.h>
#include <QTimer> 

extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
}

class PostsWindow : public QWidget
{
    Q_OBJECT

signals:
    void returnToMain(); 

private:
    QScrollArea *scrollArea;
    QWidget *postsContainer;
    QVBoxLayout *postsLayout;
    QLineEdit *commandInput;
    QPushButton *refreshBtn;  
    QString currentFilter;

public:
    PostsWindow(QWidget *parent = nullptr) : QWidget(parent)
    {
        printf("POSTSWINDOW Constructor apelat\n");
        fflush(stdout);
        
        currentFilter = "VIEW_POSTS|ALL";
        
        setWindowTitle("Postari");
        resize(800, 650); 

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);

        QLabel *headerLabel = new QLabel("<h2>Postari VirtualSoc</h2>");
        headerLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(headerLabel);

        scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);

        postsContainer = new QWidget();
        postsLayout = new QVBoxLayout(postsContainer);
        postsLayout->setAlignment(Qt::AlignTop);
        postsLayout->setSpacing(15);

        scrollArea->setWidget(postsContainer);
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

        QVBoxLayout *commandLayout = new QVBoxLayout(commandFrame);

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
        
        commandInput = new QLineEdit();
        commandInput->setPlaceholderText("EXIT || POST|image|text|type || DELETE_POST|post_id");
        commandInput->setStyleSheet(
            "QLineEdit { "
            "  padding: 8px; "
            "  font-size: 13px; "
            "  border: 1px solid #999; "
            "  border-radius: 4px; "
            "}"
        );

        commandLayout->addWidget(commandInput); 
        mainLayout->addWidget(commandFrame);

        connect(commandInput, &QLineEdit::returnPressed, this, &PostsWindow::onSendCommand);
        connect(refreshBtn, &QPushButton::clicked, this, &PostsWindow::onRefresh);

        printf("POSTSWINDOW constructor terminat\n");
        fflush(stdout);
    }

    void setCurrentFilter(const QString& filter)
    {
        currentFilter = filter;
        printf("POSTSWINDOW filter set to: %s\n", currentFilter.toUtf8().constData());
        fflush(stdout);
    }

    void displayPosts(const QString& response)
    {
        printf("POSTSWINDOW displayPosts() apelat\n");
        fflush(stdout);

        QLayoutItem *item;
        while ((item = postsLayout->takeAt(0)) != nullptr) 
        {
            delete item->widget();
            delete item;
        }

        QString cleanResponse = response.trimmed();
        
        if(cleanResponse.startsWith("ERROR|")) 
        {
            QString errorMsg = cleanResponse.mid(6).trimmed();
            QLabel *errorLabel = new QLabel(errorMsg);
            errorLabel->setStyleSheet(
                "color: red; "
                "font-weight: bold; "
                "font-size: 14px; "
                "padding: 20px; "
                "background-color: #ffe0e0; "
                "border: 2px solid red; "
                "border-radius: 8px;"
            );
            errorLabel->setAlignment(Qt::AlignCenter);
            errorLabel->setWordWrap(true);
            postsLayout->addWidget(errorLabel);
            return;
        }
        
        printf("POSTSWINDOW verificare format\n");
        fflush(stdout);
        
        if (!cleanResponse.startsWith("POSTS:")) 
        {
            printf("POSTSWINDOW] format invalid, nu incepe cu POSTS:\n");
            fflush(stdout);
            
            QLabel *errorLabel = new QLabel("format invalid de raspuns");
            errorLabel->setStyleSheet("color: red; font-weight: bold; padding: 20px;");
            errorLabel->setAlignment(Qt::AlignCenter);
            postsLayout->addWidget(errorLabel);
            return;
        }

        QString postsData = cleanResponse.mid(6);
  
        if (postsData.startsWith("NO_POSTS") || postsData.contains("Nicio postare")) 
        {
            printf("POSTSWINDOW NO_POSTS\n");
            fflush(stdout);
            
            QLabel *noPostsLabel = new QLabel("Nicio postare disponibila");
            noPostsLabel->setStyleSheet("font-size: 16px; padding: 40px; color: gray;");
            noPostsLabel->setAlignment(Qt::AlignCenter);
            postsLayout->addWidget(noPostsLabel);
            return;
        }

        QStringList posts = postsData.split('|', Qt::SkipEmptyParts);
        
        printf("POSTSWINDOW postari detectate: %d\n", posts.size());
        fflush(stdout);

        if(posts.isEmpty()) 
        {
            printf("POSTSWINDOW lista goala\n");
            fflush(stdout);
            
            QLabel *emptyLabel = new QLabel("Raspuns gol de la server");
            emptyLabel->setStyleSheet("color: orange; padding: 20px;");
            emptyLabel->setAlignment(Qt::AlignCenter);
            postsLayout->addWidget(emptyLabel);
            return;
        }

        int postCount = 0;

        for (int i = 0; i < posts.size(); i++)
        {
            QString post = posts[i].trimmed();
            
            if (post.isEmpty()) 
            {
                printf("POSTSWINDOW postarea %d gol\n", i);
                fflush(stdout);
                continue;
            }

            QStringList fields = post.split('~');
            
            printf("POSTSWINDOW Numar campuri: %d\n", fields.size());
            fflush(stdout);

            if (fields.size() < 6) 
            {
                printf("POSTSWINDOW postare invalida, prea puține câmpuri: %d != 6\n", fields.size());
                fflush(stdout);
                continue;
            }

            QString postId = fields[0].trimmed();
            QString username = fields[1].trimmed();
            QString imagePath = fields[2].trimmed();
            QString text = fields[3].trimmed();
            QString postType = fields[4].trimmed();
            QString timestamp = fields[5].trimmed();

            printf("POSTSWINDOW postare valida:\n");
            printf("POSTSWINDOW id: %s\n", postId.toUtf8().constData());
            printf("POSTSWINDOW User: %s\n", username.toUtf8().constData());
            printf("POSTSWINDOW Image: %s\n", imagePath.toUtf8().constData());
            printf("POSTSWINDOW Text: %s\n", text.toUtf8().constData());
            fflush(stdout);

            QFrame *postFrame = new QFrame();
            postFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
            postFrame->setLineWidth(2);
            postFrame->setStyleSheet(
                "QFrame { "
                " background-color: white; "
                " border: 2px solid #e0e0e0; "
                " border-radius: 10px; "
                " padding: 15px; "
                "}"
            );

            QVBoxLayout *postLayout = new QVBoxLayout(postFrame);
            postLayout->setSpacing(10);

            QLabel *headerLabel = new QLabel(
                QString("<b>%1</b> %2 | %3")
                .arg(username)
                .arg(postType.toUpper())
                .arg(timestamp)
            );
            headerLabel->setWordWrap(true);
            postLayout->addWidget(headerLabel);

            QFrame *separator = new QFrame();
            separator->setFrameShape(QFrame::HLine);
            separator->setStyleSheet("background-color: #e0e0e0;");
            postLayout->addWidget(separator);

            if (!imagePath.isEmpty()) 
            {
                QFileInfo fileInfo(imagePath);
                
                printf("POSTSWINDOW incarcare imagine: %s\n", imagePath.toUtf8().constData());
                fflush(stdout);
                
                if (fileInfo.exists()) 
                {
                    QPixmap pixmap(imagePath);
                    if (!pixmap.isNull())
                    {
                        if (pixmap.width() > 500) 
                        {
                            pixmap = pixmap.scaledToWidth(500, Qt::SmoothTransformation);
                        }
                        
                        QLabel *imageLabel = new QLabel();
                        imageLabel->setPixmap(pixmap);
                        imageLabel->setAlignment(Qt::AlignCenter);
                        imageLabel->setStyleSheet(
                            "border: 1px solid #ddd; "
                            "border-radius: 8px; "
                            "padding: 5px; "
                            "background-color: #f5f5f5;"
                        );
                        postLayout->addWidget(imageLabel);
                        
                        printf("POSTSWINDOW imagine afisata\n");
                        fflush(stdout);
                    }
                    else 
                    {
                        QLabel *errorImg = new QLabel(QString("Nu pot incarca: %1").arg(fileInfo.fileName()));
                        errorImg->setStyleSheet("color: orange; padding: 5px;");
                        postLayout->addWidget(errorImg);
                        
                        printf("POSTSWINDOW QPixmap failed\n");
                        fflush(stdout);
                    }
                } 
                else 
                {
                    QLabel *missingImg = new QLabel(QString("fara imagine"));
                    missingImg->setStyleSheet("color: red; padding: 5px;");
                    postLayout->addWidget(missingImg);
                    
                    printf("POSTSWINDOW fara poza\n");
                    fflush(stdout);
                }
            }

            if (!text.isEmpty()) 
            {
                QLabel *textLabel = new QLabel(text);
                textLabel->setWordWrap(true);
                textLabel->setStyleSheet(
                    "font-size: 13px; "
                    "padding: 10px; "
                    "background-color: #f9f9f9; "
                    "border-radius: 5px;"
                );
                postLayout->addWidget(textLabel);
            }

            postsLayout->addWidget(postFrame);
            postCount++;
            
            printf("POSTSWINDOW postare adaugata\n");
            fflush(stdout);
        }

        if(postCount == 0) 
        {
            printf("POSTSWINDOW nicio postare valida\n");
            fflush(stdout);
            
            QLabel *noValidLabel = new QLabel("Nicio postare valida in raspuns");
            noValidLabel->setStyleSheet("color: orange; padding: 20px;");
            noValidLabel->setAlignment(Qt::AlignCenter);
            postsLayout->addWidget(noValidLabel);
        } 
        else 
        {
            QLabel *countLabel = new QLabel(QString("%1 postari afisate").arg(postCount));
            countLabel->setAlignment(Qt::AlignCenter);
            countLabel->setStyleSheet("color: #666; padding: 10px; font-size: 12px;");
            postsLayout->addWidget(countLabel);
            
            printf("POSTSWINDOW SUCCES: afisate %d postari\n", postCount);
            fflush(stdout);
        }
    }

private slots:
    void onSendCommand()
    {
        QString command = commandInput->text().trimmed();
        
        if(command.isEmpty()) 
        {
            return;
        }

        if(command.toUpper() == "EXIT") 
        {
            printf("POSTSWINDOW EXIT - intoarcere la MainWindow\n");
            fflush(stdout);
            
            emit returnToMain(); 
            this->close();
            return;
        }
    
        if(command.toUpper().startsWith("POST|"))
        {
            printf("POSTSWINDOW sending POST command: %s\n", command.toUtf8().constData());
            fflush(stdout);
            
            commandInput->setEnabled(false);
            refreshBtn->setEnabled(false);
            
            int result = client_send(command.toUtf8().constData());
            
            if(result != 0)
            {
                printf("ERROR: Could not send command\n");
                fflush(stdout);
                commandInput->clear();
                commandInput->setEnabled(true);
                refreshBtn->setEnabled(true);
                return;
            }
            
            char buffer[5000];
            memset(buffer, 0, sizeof(buffer));
            int bytes = client_receive(buffer, sizeof(buffer));
            
            if(bytes <= 0)
            {
                printf("ERROR: No response from server\n");
                fflush(stdout);
            }
            else
            {
                QString response = QString::fromUtf8(buffer, bytes);
                printf("POSTSWINDOW response: %s\n", response.toUtf8().constData());
                fflush(stdout);
            }
            
            commandInput->clear();
            commandInput->setEnabled(true);
            refreshBtn->setEnabled(true);
            
            QTimer::singleShot(500, this, &PostsWindow::onRefresh);
            
            return;
        }

        if(command.toUpper().startsWith("DELETE_POST|"))
        {
            printf("POSTSWINDOW sending DELETE_POST command: %s\n", command.toUtf8().constData());
            fflush(stdout);
            
            commandInput->setEnabled(false);
            refreshBtn->setEnabled(false);
            
            int result = client_send(command.toUtf8().constData());
            
            if(result != 0)
            {
                printf("ERROR: Could not send command\n");
                fflush(stdout);
                commandInput->clear();
                commandInput->setEnabled(true);
                refreshBtn->setEnabled(true);
                return;
            }
            
            char buffer[5000];
            memset(buffer, 0, sizeof(buffer));
            int bytes = client_receive(buffer, sizeof(buffer));
            
            if(bytes <= 0)
            {
                printf("ERROR: No response from server\n");
                fflush(stdout);
            }
            else
            {
                QString response = QString::fromUtf8(buffer, bytes);
                printf("POSTSWINDOW response: %s\n", response.toUtf8().constData());
                fflush(stdout);
            }
                
            commandInput->clear();
            commandInput->setEnabled(true);
            refreshBtn->setEnabled(true);
            
            QTimer::singleShot(500, this, &PostsWindow::onRefresh);
                        
            return;
        }
        
        commandInput->clear();
    }    

    void onRefresh()
    {
        printf("POSTSWINDOW refresh with filter: %s\n", currentFilter.toUtf8().constData());
        fflush(stdout);
        
        if(client_send(currentFilter.toUtf8().constData()) != 0) 
        {
            printf("POSTSWINDOW eroare la trimitere VIEW_POSTS\n");
            return;
        }
        
        char buffer[30000];
        memset(buffer, 0, sizeof(buffer));
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes <= 0) 
        {
            printf("POSTSWINDOW eroare la primire raspuns\n");
            return;
        }
        
        QString response = QString::fromUtf8(buffer, bytes);
        
        displayPosts(response);
    }
};

#endif