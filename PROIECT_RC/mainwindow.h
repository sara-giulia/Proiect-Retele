#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QFrame>
#include <QGroupBox>
#include <QMessageBox>
#include <QStackedWidget> 
#include <QScrollArea>
#include "postswindow.h"
#include "profilewindow.h"
#include "adminwindow.h"
#include "conversationswindow.h"
#include "chatwindow.h"

extern "C" {
    int client_send(const char* command);
    int client_receive(char* response, int response_size);
    void client_close();
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    PostsWindow *postsWindow;
    ProfileWindow *profileWindow;
    AdminWindow *adminWindow;
    ConversationsWindow *conversationsWindow;
    ChatWindow *chatWindow;
    
    bool isAuthenticated;
    QString currentUsername;
    QString userType;
    bool isPublicProfile;
    QString lastResponse;
    
    QStackedWidget *stackedWidget;
    QWidget *loginWidget;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QPushButton *loginBtn;
    QPushButton *registerUserBtn;
    QPushButton *registerAdminBtn;
    
    QWidget *mainMenuWidget;
    QLabel *welcomeLabel;
    QLabel *profileTypeLabel;
    QLabel *notificationLabel;
    
    QPushButton *setPublicBtn;
    QPushButton *setPrivateBtn;
    QPushButton *viewMyProfileBtn;
    
    QPushButton *viewAllPostsBtn;
    QPushButton *viewFriendsPostsBtn;
    QPushButton *viewCloseFriendsPostsBtn;
    QPushButton *viewMyPostsBtn;
    
    QPushButton *viewConversationsBtn;
    
    QLineEdit *searchUserInput;
    QPushButton *searchUserBtn;
    QPushButton *viewAllUsersBtn;
    
    QPushButton *logoutBtn;
    
    QTimer *notificationTimer;
    int unreadCount;
    bool checkingNotifications;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    static MainWindow* mainWindowInstance;

private:
    void createLoginUI();
    void createMainMenuUI();
    void switchToLoginUI();
    void switchToMainMenuUI();
    void updateProfileTypeLabel();
    void sendCommand(const QString& command);
    void checkNotifications();

private slots:
    void onLoginClicked();
    void onRegisterUserClicked();
    void onRegisterAdminClicked();
    
    void onSetPublicClicked();
    void onSetPrivateClicked();
    void onViewMyProfileClicked();
    
    void onViewAllPostsClicked();
    void onViewFriendsPostsClicked();
    void onViewCloseFriendsPostsClicked();
    void onViewMyPostsClicked();
    
    void onViewConversationsClicked();
    
    void onSearchUserClicked();
    void onViewAllUsersClicked();
    
    void onLogoutClicked();
    
    void onReturnFromPosts();
    void onReturnFromProfile();
    void onReturnFromAdmin();
    void onReturnFromConversations();
    void onReturnFromChatWindow();
};

#endif