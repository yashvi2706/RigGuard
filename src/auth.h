#ifndef AUTH_H
#define AUTH_H

#include "utils.h"
#include "filestore.h"

int  login(char *username_out, char *role_out);
void logout(const char *username);
int  is_commander(const char *role);

#endif
