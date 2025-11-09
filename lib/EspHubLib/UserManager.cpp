#include "UserManager.h"
#include "StreamLogger.h" // For Log
#include <mbedtls/md.h> // For SHA256 hashing

extern StreamLogger* Log;

UserManager::UserManager() {
}

void UserManager::begin() {
    loadUsers();
    if (users.empty()) {
        // Create default admin user if no users exist
        addUser("admin", "admin", ROLE_ADMIN);
        Log->println("Created default admin user: admin/admin");
    }
}

bool UserManager::authenticate(const String& username, const String& password) {
    if (users.count(username)) {
        return users[username].passwordHash == hashPassword(password);
    }
    return false;
}

bool UserManager::addUser(const String& username, const String& password, UserRole role) {
    if (users.count(username)) {
        Log->printf("ERROR: User '%s' already exists.\n", username.c_str());
        return false;
    }
    User newUser;
    newUser.username = username;
    newUser.passwordHash = hashPassword(password);
    newUser.role = role;
    users[username] = newUser;
    saveUsers();
    Log->printf("User '%s' added with role %d.\n", username.c_str(), role);
    return true;
}

bool UserManager::deleteUser(const String& username) {
    if (users.count(username)) {
        users.erase(username);
        saveUsers();
        Log->printf("User '%s' deleted.\n", username.c_str());
        return true;
    }
    Log->printf("ERROR: User '%s' not found.\n", username.c_str());
    return false;
}

bool UserManager::changePassword(const String& username, const String& oldPassword, const String& newPassword) {
    if (users.count(username)) {
        if (authenticate(username, oldPassword)) {
            users[username].passwordHash = hashPassword(newPassword);
            saveUsers();
            Log->printf("Password for user '%s' changed.\n", username.c_str());
            return true;
        }
        Log->printf("ERROR: Incorrect old password for user '%s'.\n", username.c_str());
        return false;
    }
    Log->printf("ERROR: User '%s' not found.\n", username.c_str());
    return false;
}

UserRole UserManager::getUserRole(const String& username) {
    if (users.count(username)) {
        return users[username].role;
    }
    return ROLE_MONITOR; // Default to lowest privilege
}

void UserManager::loadUsers() {
    Preferences preferences;
    preferences.begin("user_manager", true);

    size_t count = preferences.getUInt("user_count", 0);
    for (size_t i = 0; i < count; ++i) {
        String userKey = "user_" + String(i);
        String username = preferences.getString(userKey + "_name", "");
        String passwordHash = preferences.getString(userKey + "_hash", "");
        UserRole role = static_cast<UserRole>(preferences.getUChar(userKey + "_role", ROLE_MONITOR));
        if (!username.isEmpty()) {
            users[username] = {username, passwordHash, role};
        }
    }
    preferences.end();
    Log->printf("Loaded %u users.\n", users.size());
}

void UserManager::saveUsers() {
    Preferences preferences;
    preferences.begin("user_manager", false);

    preferences.clear(); // Clear existing users before saving new ones
    preferences.putUInt("user_count", users.size());
    size_t i = 0;
    for (auto const& [username, user] : users) {
        String userKey = "user_" + String(i);
        preferences.putString(userKey + "_name", user.username);
        preferences.putString(userKey + "_hash", user.passwordHash);
        preferences.putUChar(userKey + "_role", user.role);
        i++;
    }
    preferences.end();
    Log->printf("Saved %u users.\n", users.size());
}

String UserManager::hashPassword(const String& password) {
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
    
    unsigned char output[32]; // SHA256 produces 32 bytes
    mbedtls_md_finish(&ctx, output);
    mbedtls_md_free(&ctx);

    char hex_output[65]; // 32 bytes * 2 chars/byte + null terminator
    for (int i = 0; i < 32; i++) {
        sprintf(&hex_output[i*2], "%02x", output[i]);
    }
    return String(hex_output);
}