#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <Arduino.h>
#include <map>
#include <Preferences.h>

enum UserRole {
    ROLE_MONITOR,
    ROLE_DEV,
    ROLE_ADMIN
};

struct User {
    String username;
    String passwordHash; // Store hashed passwords
    UserRole role;
};

class UserManager {
public:
    UserManager();
    void begin();
    bool authenticate(const String& username, const String& password);
    bool addUser(const String& username, const String& password, UserRole role);
    bool deleteUser(const String& username);
    bool changePassword(const String& username, const String& oldPassword, const String& newPassword);
    UserRole getUserRole(const String& username);

private:
    std::map<String, User> users;
    void loadUsers();
    void saveUsers();
    String hashPassword(const String& password);
};

#endif // USER_MANAGER_H