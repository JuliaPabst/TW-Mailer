#ifndef LDAP_FUNCTIONS_H
#define LDAP_FUNCTIONS_H

#include <ldap.h>
#include <string.h>


// Function to perform an LDAP search
char *ldapFind(char* username, char* password);

#endif // LDAP_FUNCTIONS_H
