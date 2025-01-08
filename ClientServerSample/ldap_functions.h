#ifndef LDAP_FUNCTIONS_H
#define LDAP_FUNCTIONS_H

#include <ldap.h>

// Function to initialize an LDAP connection
LDAP *initialize_ldap(const char *ldapUri, int ldapVersion);

// Function to authenticate a user with LDAP
int authenticate_ldap_user(LDAP *ldapHandle, const char *username, const char *password);

// Function to perform an LDAP search
int perform_ldap_search(LDAP *ldapHandle, const char *base, const char *filter);

#endif // LDAP_FUNCTIONS_H
