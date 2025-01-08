#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ldap.h>
#include "ldap_functions.h"

LDAP *initialize_ldap(const char *ldapUri, int ldapVersion) {
    LDAP *ldapHandle;
    int rc = ldap_initialize(&ldapHandle, ldapUri);
    if (rc != LDAP_SUCCESS) {
        fprintf(stderr, "ldap_initialize failed: %s\n", ldap_err2string(rc));
        return NULL;
    }

    rc = ldap_set_option(ldapHandle, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion);
    if (rc != LDAP_OPT_SUCCESS) {
        fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION) failed: %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        return NULL;
    }

    return ldapHandle;
}

int authenticate_ldap_user(LDAP *ldapHandle, const char *username, const char *password) {
    BerValue bindCredentials = { .bv_val = (char *)password, .bv_len = strlen(password) };
    int rc = ldap_sasl_bind_s(ldapHandle, username, LDAP_SASL_SIMPLE, &bindCredentials, NULL, NULL, NULL);

    if (rc != LDAP_SUCCESS) {
        fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
        return -1;
    }

    return 0;
}

int perform_ldap_search(LDAP *ldapHandle, const char *base, const char *filter) {
    LDAPMessage *result;
    int rc = ldap_search_ext_s(
        ldapHandle,
        base,
        LDAP_SCOPE_SUBTREE,
        filter,
        NULL, // Attributes to retrieve
        0,
        NULL,
        NULL,
        NULL,
        LDAP_NO_LIMIT,
        &result
    );

    if (rc != LDAP_SUCCESS) {
        fprintf(stderr, "LDAP search error: %s\n", ldap_err2string(rc));
        return -1;
    }

    // Process results
    LDAPMessage *entry = ldap_first_entry(ldapHandle, result);
    while (entry) {
        char *dn = ldap_get_dn(ldapHandle, entry);
        if (dn) {
            printf("DN: %s\n", dn);
            ldap_memfree(dn);
        }
        entry = ldap_next_entry(ldapHandle, entry);
    }

    ldap_msgfree(result);
    return 0;
}
