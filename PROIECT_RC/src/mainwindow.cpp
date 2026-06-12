#include "mainwindow.h"
#include <QApplication>
#include <QScreen>
#include <stdio.h>

MainWindow* MainWindow::mainWindowInstance = nullptr;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("VirtualSoc");
    resize(800, 600);
    
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
    
    postsWindow = nullptr;
    profileWindow = nullptr;
    adminWindow = nullptr;
    conversationsWindow = nullptr;
    chatWindow = nullptr;
    isAuthenticated = false;
    isPublicProfile = true;
    unreadCount = 0;
    checkingNotifications = false;
    
    notificationTimer = new QTimer(this);
    connect(notificationTimer, &QTimer::timeout, this, &MainWindow::checkNotifications);
    
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);
    
    createLoginUI();
    createMainMenuUI();
    
    stackedWidget->addWidget(loginWidget); 
    stackedWidget->addWidget(mainMenuWidget); 
    
    stackedWidget->setCurrentIndex(0);
    
    printf("MAINWINDOW constructor terminat\n");
    fflush(stdout);
}

MainWindow::~MainWindow()
{
    if(postsWindow) delete postsWindow;
    if(profileWindow) delete profileWindow;
    if(adminWindow) delete adminWindow;
    if(conversationsWindow) delete conversationsWindow;
    if(chatWindow) delete chatWindow;
    client_close();
}

void MainWindow::createLoginUI()
{
    loginWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(loginWidget);
    layout->setSpacing(15);
    layout->setContentsMargins(50, 50, 50, 50);
    
    QLabel *titleLabel = new QLabel("<h1>VirtualSoc</h1>");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #667eea; margin-bottom: 20px;");
    layout->addWidget(titleLabel);
    
    layout->addStretch();
    
    QLabel *userLabel = new QLabel("Username:");
    userLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(userLabel);
    
    usernameInput = new QLineEdit();
    usernameInput->setPlaceholderText("Enter username");
    usernameInput->setStyleSheet(
        "QLineEdit { "
        "  padding: 10px; "
        "  border: 2px solid #e2e8f0; "
        "  border-radius: 8px; "
        "  font-size: 14px; "
        "} "
        "QLineEdit:focus { border-color: #667eea; }"
    );
    layout->addWidget(usernameInput);
    
    QLabel *passLabel = new QLabel("Password:");
    passLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(passLabel);
    
    passwordInput = new QLineEdit();
    passwordInput->setPlaceholderText("Enter password");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setStyleSheet(usernameInput->styleSheet());
    layout->addWidget(passwordInput);
    
    layout->addSpacing(20);
    
    loginBtn = new QPushButton("LOGIN");
    loginBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #667eea; "
        "  color: white; "
        "  padding: 12px; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-size: 16px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #5568d3; }"
    );
    layout->addWidget(loginBtn);
    
    QLabel *registerLabel = new QLabel("Don't have an account?");
    registerLabel->setAlignment(Qt::AlignCenter);
    registerLabel->setStyleSheet("color: #666; margin-top: 20px;");
    layout->addWidget(registerLabel);
    
    QHBoxLayout *registerLayout = new QHBoxLayout();
    
    registerUserBtn = new QPushButton("Register as USER");
    registerUserBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #48bb78; "
        "  color: white; "
        "  padding: 10px; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #38a169; }"
    );
    registerLayout->addWidget(registerUserBtn);
    
    registerAdminBtn = new QPushButton("Register as ADMIN");
    registerAdminBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #ed8936; "
        "  color: white; "
        "  padding: 10px; "
        "  border: none; "
        "  border-radius: 8px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #dd6b20; }"
    );
    registerLayout->addWidget(registerAdminBtn);
    
    layout->addLayout(registerLayout);
    layout->addStretch();
    
    connect(loginBtn, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    connect(registerUserBtn, &QPushButton::clicked, this, &MainWindow::onRegisterUserClicked);
    connect(registerAdminBtn, &QPushButton::clicked, this, &MainWindow::onRegisterAdminClicked);
    connect(usernameInput, &QLineEdit::returnPressed, this, &MainWindow::onLoginClicked);
    connect(passwordInput, &QLineEdit::returnPressed, this, &MainWindow::onLoginClicked);
}

void MainWindow::createMainMenuUI()
{
    mainMenuWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(mainMenuWidget);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);
    
    QFrame *headerFrame = new QFrame();
    headerFrame->setStyleSheet(
        "QFrame { "
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "    stop:0 #667eea, stop:1 #764ba2); "
        "  border-radius: 10px; "
        "  padding: 15px; "
        "}"
    );
    QVBoxLayout *headerLayout = new QVBoxLayout(headerFrame);
    
    welcomeLabel = new QLabel();
    welcomeLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(welcomeLabel);
    
    profileTypeLabel = new QLabel();
    profileTypeLabel->setStyleSheet("color: white; font-size: 14px;");
    headerLayout->addWidget(profileTypeLabel);
    
    notificationLabel = new QLabel();
    notificationLabel->setStyleSheet(
        "QLabel { "
        "  background-color: #fed7d7; "
        "  color: #c53030; "
        "  padding: 8px; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "}"
    );
    notificationLabel->hide();
    headerLayout->addWidget(notificationLabel);
    
    layout->addWidget(headerFrame);
    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; }");
    
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(10);
    
    QGroupBox *profileGroup = new QGroupBox("Profile");
    profileGroup->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #e2e8f0; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 10px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  left: 10px; "
        "  padding: 0 5px; "
        "}"
    );
    QVBoxLayout *profileLayout = new QVBoxLayout(profileGroup);
    
    QHBoxLayout *profileTypeLayout = new QHBoxLayout();
    setPublicBtn = new QPushButton("Set PUBLIC");
    setPrivateBtn = new QPushButton("Set PRIVATE");
    
    QString profileBtnStyle = 
        "QPushButton { "
        "  background-color: #4299e1; "
        "  color: white; "
        "  padding: 8px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #3182ce; }";
    
    setPublicBtn->setStyleSheet(profileBtnStyle);
    setPrivateBtn->setStyleSheet(profileBtnStyle);
    
    profileTypeLayout->addWidget(setPublicBtn);
    profileTypeLayout->addWidget(setPrivateBtn);
    profileLayout->addLayout(profileTypeLayout);
    
    viewMyProfileBtn = new QPushButton("View My Profile");
    viewMyProfileBtn->setStyleSheet(profileBtnStyle);
    profileLayout->addWidget(viewMyProfileBtn);
    
    scrollLayout->addWidget(profileGroup);
    
    QGroupBox *postsGroup = new QGroupBox("Posts");
    postsGroup->setStyleSheet(profileGroup->styleSheet());
    QVBoxLayout *postsLayout = new QVBoxLayout(postsGroup);
    
    viewAllPostsBtn = new QPushButton("View ALL Posts");
    viewFriendsPostsBtn = new QPushButton("View FRIENDS Posts");
    viewCloseFriendsPostsBtn = new QPushButton("View CLOSE FRIENDS Posts");
    viewMyPostsBtn = new QPushButton("View My Posts");
    
    QString postsBtnStyle = 
        "QPushButton { "
        "  background-color: #48bb78; "
        "  color: white; "
        "  padding: 8px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #38a169; }";
    
    viewAllPostsBtn->setStyleSheet(postsBtnStyle);
    viewFriendsPostsBtn->setStyleSheet(postsBtnStyle);
    viewCloseFriendsPostsBtn->setStyleSheet(postsBtnStyle);
    viewMyPostsBtn->setStyleSheet(postsBtnStyle);
    
    postsLayout->addWidget(viewAllPostsBtn);
    postsLayout->addWidget(viewFriendsPostsBtn);
    postsLayout->addWidget(viewCloseFriendsPostsBtn);
    postsLayout->addWidget(viewMyPostsBtn);
    
    scrollLayout->addWidget(postsGroup);
    
    QGroupBox *messagesGroup = new QGroupBox("Messages");
    messagesGroup->setStyleSheet(profileGroup->styleSheet());
    QVBoxLayout *messagesLayout = new QVBoxLayout(messagesGroup);
    
    viewConversationsBtn = new QPushButton("View My Conversations");
    viewConversationsBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #ed8936; "
        "  color: white; "
        "  padding: 10px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "} "
        "QPushButton:hover { background-color: #dd6b20; }"
    );
    messagesLayout->addWidget(viewConversationsBtn);
    
    scrollLayout->addWidget(messagesGroup);
    
    QGroupBox *usersGroup = new QGroupBox("Users");
    usersGroup->setStyleSheet(profileGroup->styleSheet());
    QVBoxLayout *usersLayout = new QVBoxLayout(usersGroup);
    
    QLabel *searchLabel = new QLabel("Search user:");
    usersLayout->addWidget(searchLabel);
    
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchUserInput = new QLineEdit();
    searchUserInput->setPlaceholderText("Enter username...");
    searchUserInput->setStyleSheet(
        "QLineEdit { "
        "  padding: 8px; "
        "  border: 2px solid #e2e8f0; "
        "  border-radius: 5px; "
        "}"
    );
    searchLayout->addWidget(searchUserInput);
    
    searchUserBtn = new QPushButton("Search");
    searchUserBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #9f7aea; "
        "  color: white; "
        "  padding: 8px 15px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #805ad5; }"
    );
    searchLayout->addWidget(searchUserBtn);
    usersLayout->addLayout(searchLayout);
    
    viewAllUsersBtn = new QPushButton("ADMIN: View All Users");
    viewAllUsersBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #e53e3e; "
        "  color: white; "
        "  padding: 10px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #c53030; }"
    );
    viewAllUsersBtn->hide();
    usersLayout->addWidget(viewAllUsersBtn);
    
    scrollLayout->addWidget(usersGroup);
    scrollLayout->addStretch();
    
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea);
    
    logoutBtn = new QPushButton("Logout");
    logoutBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #718096; "
        "  color: white; "
        "  padding: 10px; "
        "  border: none; "
        "  border-radius: 5px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #4a5568; }"
    );
    layout->addWidget(logoutBtn);
    
    connect(setPublicBtn, &QPushButton::clicked, this, &MainWindow::onSetPublicClicked);
    connect(setPrivateBtn, &QPushButton::clicked, this, &MainWindow::onSetPrivateClicked);
    connect(viewMyProfileBtn, &QPushButton::clicked, this, &MainWindow::onViewMyProfileClicked);
    
    connect(viewAllPostsBtn, &QPushButton::clicked, this, &MainWindow::onViewAllPostsClicked);
    connect(viewFriendsPostsBtn, &QPushButton::clicked, this, &MainWindow::onViewFriendsPostsClicked);
    connect(viewCloseFriendsPostsBtn, &QPushButton::clicked, this, &MainWindow::onViewCloseFriendsPostsClicked);
    connect(viewMyPostsBtn, &QPushButton::clicked, this, &MainWindow::onViewMyPostsClicked);
    
    connect(viewConversationsBtn, &QPushButton::clicked, this, &MainWindow::onViewConversationsClicked);
    
    connect(searchUserBtn, &QPushButton::clicked, this, &MainWindow::onSearchUserClicked);
    connect(searchUserInput, &QLineEdit::returnPressed, this, &MainWindow::onSearchUserClicked);
    connect(viewAllUsersBtn, &QPushButton::clicked, this, &MainWindow::onViewAllUsersClicked);
    
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
}

void MainWindow::switchToLoginUI()
{
    stackedWidget->setCurrentIndex(0);
    if(usernameInput) 
    {
        usernameInput->setFocus();
    }
}

void MainWindow::switchToMainMenuUI()
{
    stackedWidget->setCurrentIndex(1);
    welcomeLabel->setText(QString("Welcome, %1!").arg(currentUsername));
    updateProfileTypeLabel();
    
    viewAllUsersBtn->setVisible(userType == "admin");
    
    if(!notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::updateProfileTypeLabel()
{
    profileTypeLabel->setText(QString("Profile: %1").arg(isPublicProfile ? "PUBLIC" : "PRIVATE"));
}

void MainWindow::sendCommand(const QString& command)
{
    printf("Sending: %s\n", command.toUtf8().constData());
    fflush(stdout);
    
    if(client_send(command.toUtf8().constData()) != 0)
    {
        printf("ERROR sending command\n");
        fflush(stdout);
        QMessageBox::critical(this, "Error", "Could not send command!");
        lastResponse = "ERROR|Could not send command";
        return;
    }
    
    char buffer[30000];
    memset(buffer, 0, sizeof(buffer));
    
    unsigned int bytes = client_receive(buffer, sizeof(buffer));
    
    if(bytes <= 0)
    {
        printf("ERROR: no response received\n");
        fflush(stdout);
        QMessageBox::critical(this, "Error", "No response from server!");
        lastResponse = "ERROR|No response from server";
        return;
    }
    
    if(bytes >= sizeof(buffer))
    {
        bytes = sizeof(buffer) - 1;
    }
    
    buffer[bytes] = '\0';
    
    lastResponse = QString::fromUtf8(buffer, bytes);
    
    printf("Response: %s\n", lastResponse.left(100).toUtf8().constData());
    fflush(stdout);
}

void MainWindow::onLoginClicked()
{
    QString username = usernameInput->text().trimmed();
    QString password = passwordInput->text().trimmed();
    
    if(username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please enter username and password!");
        return;
    }
    
    QString command = QString("LOGIN|%1|%2").arg(username).arg(password);
    sendCommand(command);
    
    if(lastResponse.startsWith("SUCCESS|Autentificat:"))
    {
        isAuthenticated = true;
        currentUsername = username;
        
        QStringList parts = lastResponse.split(":");
        if(parts.size() >= 3)
        {
            userType = parts[2].trimmed();
        }
        
        printf("Authenticated: %s (type: %s)\n", currentUsername.toUtf8().constData(), userType.toUtf8().constData());
        fflush(stdout);
        
        switchToMainMenuUI();
    }
    else
    {
        QMessageBox::critical(this, "Error", "Authentication failed!\n" + lastResponse);
    }
}

void MainWindow::onRegisterUserClicked()
{
    QString username = usernameInput->text().trimmed();
    QString password = passwordInput->text().trimmed();
    
    if(username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please enter username and password!");
        return;
    }
    
    QString command = QString("REGISTER|%1|%2|USER").arg(username).arg(password);
    sendCommand(command);
    
    if(lastResponse.startsWith("SUCCESS"))
    {
        QMessageBox::information(this, "Success", "Account created! You can now login.");
        passwordInput->clear();
    }
    else
    {
        QMessageBox::critical(this, "Error", lastResponse);
    }
}

void MainWindow::onRegisterAdminClicked()
{
    QString username = usernameInput->text().trimmed();
    QString password = passwordInput->text().trimmed();
    
    if(username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please enter username and password!");
        return;
    }
    
    QString command = QString("REGISTER|%1|%2|ADMIN").arg(username).arg(password);
    sendCommand(command);
    
    if(lastResponse.startsWith("SUCCESS"))
    {
        QMessageBox::information(this, "Success", "Admin account created! You can now login.");
        passwordInput->clear();
    }
    else
    {
        QMessageBox::critical(this, "Error", lastResponse);
    }
}

void MainWindow::onSetPublicClicked()
{
    sendCommand("SET_PROFILE_TYPE|PUBLIC");
    
    if(lastResponse.startsWith("SUCCESS"))
    {
        isPublicProfile = true;
        updateProfileTypeLabel();
    }
    else if(lastResponse.startsWith("ERROR"))
    {
        QMessageBox::warning(this, "Error", lastResponse);
    }
}

void MainWindow::onSetPrivateClicked()
{
    sendCommand("SET_PROFILE_TYPE|PRIVATE");
    
    if(lastResponse.startsWith("SUCCESS"))
    {
        isPublicProfile = false;
        updateProfileTypeLabel();
    }
    else if(lastResponse.startsWith("ERROR"))
    {
        QMessageBox::warning(this, "Error", lastResponse);
    }
}

void MainWindow::onViewMyProfileClicked()
{
    sendCommand("VIEW_MY_PROFILE");
    
    if(lastResponse.startsWith("MY_PROFILE:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!profileWindow)
        {
            profileWindow = new ProfileWindow(nullptr);
            connect(profileWindow, &ProfileWindow::returnToMain, this, &MainWindow::onReturnFromProfile);
        }
        
        profileWindow->displayMyProfile(lastResponse);
        profileWindow->show();
        this->hide();
    }
}

void MainWindow::onViewAllPostsClicked()
{
    sendCommand("VIEW_POSTS|ALL");
    
    if(lastResponse.startsWith("POSTS:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!postsWindow)
        {
            postsWindow = new PostsWindow(nullptr);
            connect(postsWindow, &PostsWindow::returnToMain, this, &MainWindow::onReturnFromPosts);
        }
        
        postsWindow->setCurrentFilter("VIEW_POSTS|ALL");
        postsWindow->displayPosts(lastResponse);
        postsWindow->show();
        this->hide();
    }
}

void MainWindow::onViewFriendsPostsClicked()
{
    sendCommand("VIEW_POSTS|FRIENDS");
    
    if(lastResponse.startsWith("POSTS:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!postsWindow)
        {
            postsWindow = new PostsWindow(nullptr);
            connect(postsWindow, &PostsWindow::returnToMain, this, &MainWindow::onReturnFromPosts);
        }
        
        postsWindow->setCurrentFilter("VIEW_POSTS|FRIENDS");
        postsWindow->displayPosts(lastResponse);
        postsWindow->show();
        this->hide();
    }
}

void MainWindow::onViewCloseFriendsPostsClicked()
{
    sendCommand("VIEW_POSTS|CLOSE_FRIENDS");
    
    if(lastResponse.startsWith("POSTS:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!postsWindow)
        {
            postsWindow = new PostsWindow(nullptr);
            connect(postsWindow, &PostsWindow::returnToMain, this, &MainWindow::onReturnFromPosts);
        }
        
        postsWindow->setCurrentFilter("VIEW_POSTS|CLOSE_FRIENDS");
        postsWindow->displayPosts(lastResponse);
        postsWindow->show();
        this->hide();
    }
}

void MainWindow::onViewMyPostsClicked()
{
    sendCommand("VIEW_POSTS|MY_POSTS");
    
    if(lastResponse.startsWith("POSTS:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!postsWindow)
        {
            postsWindow = new PostsWindow(nullptr);
            connect(postsWindow, &PostsWindow::returnToMain, this, &MainWindow::onReturnFromPosts);
        }
        
        postsWindow->setCurrentFilter("VIEW_POSTS|MY_POSTS");
        postsWindow->displayPosts(lastResponse);
        postsWindow->show();
        this->hide();
    }
}

void MainWindow::onViewConversationsClicked()
{
    if(notificationTimer) notificationTimer->stop();
    
    if(conversationsWindow)
    {
        delete conversationsWindow;
        conversationsWindow = nullptr;
    }
    
    conversationsWindow = new ConversationsWindow(currentUsername, nullptr);
    connect(conversationsWindow, &ConversationsWindow::returnToMain, this, &MainWindow::onReturnFromConversations);
    
    conversationsWindow->show();
    this->hide();
}

void MainWindow::onSearchUserClicked()
{
    QString username = searchUserInput->text().trimmed();
    
    if(username.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please enter a username!");
        return;
    }
    
    QString command = QString("VIEW_FEED|%1").arg(username);
    sendCommand(command);
    
    if(lastResponse.startsWith("USER_FEED:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!profileWindow)
        {
            profileWindow = new ProfileWindow(nullptr);
            connect(profileWindow, &ProfileWindow::returnToMain, this, &MainWindow::onReturnFromProfile);
        }
        
        profileWindow->displayUserFeed(lastResponse);
        profileWindow->show();
        this->hide();
    }
    else
    {
        QMessageBox::warning(this, "Error", lastResponse);
    }
}

void MainWindow::onViewAllUsersClicked()
{
    sendCommand("VIEW_ALL_USERS");
    
    if(lastResponse.startsWith("ALL_USERS:"))
    {
        if(notificationTimer) notificationTimer->stop();
        
        if(!adminWindow)
        {
            adminWindow = new AdminWindow(nullptr);
            connect(adminWindow, &AdminWindow::returnToMain, this, &MainWindow::onReturnFromAdmin);
        }
        
        adminWindow->displayUsers(lastResponse);
        adminWindow->show();
        this->hide();
    }
}

void MainWindow::onLogoutClicked()
{
    printf("MAIN: Logout started\n");
    fflush(stdout);
    
    if(notificationTimer && notificationTimer->isActive())
    {
        notificationTimer->stop();
    }
    
    isAuthenticated = false;
    
    if(client_send("LOGOUT") == 0)
    {
        char buffer[1000];
        memset(buffer, 0, sizeof(buffer));
        int bytes = client_receive(buffer, sizeof(buffer));
        
        if(bytes > 0)
        {
            printf("MAIN: Logout response: %s\n", buffer);
            fflush(stdout);
        }
    }
    
    switchToLoginUI();
    
    if(usernameInput)
    {
        usernameInput->clear();
        usernameInput->setFocus();
    }
    
    if(passwordInput)
    {
        passwordInput->clear();
    }
    
    currentUsername = "";
    userType = "";
    lastResponse = "";
    
    if(notificationLabel)
    {
        notificationLabel->hide();
    }
    
    printf("MAIN: Logout complete\n");
    fflush(stdout);
}

void MainWindow::onReturnFromPosts()
{
    this->show();
    this->raise();
    this->activateWindow();
    
    if(notificationTimer && !notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::onReturnFromProfile()
{
    this->show();
    this->raise();
    this->activateWindow();
    
    if(notificationTimer && !notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::onReturnFromAdmin()
{
    this->show();
    this->raise();
    this->activateWindow();
    
    if(notificationTimer && !notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::onReturnFromConversations()
{
    this->show();
    this->raise();
    this->activateWindow();
    
    if(notificationTimer && !notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::onReturnFromChatWindow()
{
    this->show();
    this->raise();
    this->activateWindow();
    
    if(notificationTimer && !notificationTimer->isActive())
    {
        notificationTimer->start(1000);
    }
}

void MainWindow::checkNotifications()
{
    if(!isAuthenticated || !this->isVisible() || checkingNotifications)
    {
        return;
    }
    
    checkingNotifications = true;
    
    if(client_send("GET_UNREAD_COUNT") != 0)
    {
        checkingNotifications = false;
        return;
    }
    
    char buffer[1000];
    memset(buffer, 0, sizeof(buffer));
    int bytes = client_receive(buffer, sizeof(buffer));
    
    if(bytes <= 0)
    {
        checkingNotifications = false;
        return;
    }
    
    QString response = QString::fromUtf8(buffer, bytes);
    
    if(response.startsWith("UNREAD_COUNT:"))
    {
        int count = response.mid(13).toInt();
        
        if(count > 0)
        {
            notificationLabel->setText(QString("%1 new message%2!").arg(count).arg(count > 1 ? "s" : ""));
            notificationLabel->show();
        }
        else
        {
            notificationLabel->hide();
        }
    }
    
    checkingNotifications = false;
}