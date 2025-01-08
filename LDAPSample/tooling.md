# LDAP client in Console

ldapsearch ( see [manual](https://linux.die.net/man/1/ldapsearch) and [ldaputils](https://wiki.debian.org/LDAP/LDAPUtils))

## Sample Usage

ldapsearch -x -LLL '(uid=FILTERSTRING)' -D"CURRENTUSERCREDENTIALS" -W dn cn uid -ZZ -d 5

Parameters: 

* -x: simple authentication
* -LLL: restricts the output to LDIFv1, disables comments, disables printing of the LDIF version
* FILTER: a mask to filter the result set for
* -D: sets the distinguished name to bind to LDAP
* -W: request for password in user input (with -w can be added in shell-command directly)
* attributes to display
* -ZZ: TLS (with required successfull TLS operation)
* -d: debugLevel (e.g.: 5)


Variables: 

* FILTERSTRING: a mask to fetch all users matching it e.g.: if19b00* \
  ATTENTION: you are limited to only a few entries allowed in the recordset
* CURRENTUSERCREDENTIALS: your uid,ou,dc information


see ldapsearch.sh using environment variables

### Pitfall: MacOs

In case you are using a Mac and you are getting an error with the sample above 
(something like "bind with no password") you need to consider reordering the parameters 
(internal implementation of getopt does not reorder parameters as other implementations used to do it).

So try to use the command in this parameter-order:
```
ldapsearch -x -LLL -D"CURRENTUSERCREDENTIALS" -W -ZZ -d 5 '(uid=FILTERSTRING)' dn cn uid
```

this will bind the user before executing the actual call.

# Related Config Files

/etc/ldap/ldap.conf

* Base dc
* URI ldap-uri

and

* TLS settings

# Abbreviations

https://www.ietf.org/rfc/rfc2253.txt

```
                    String  X.500 AttributeType
                    ------------------------------
                    CN      commonName
                    L       localityName
                    ST      stateOrProvinceName
                    O       organizationName
                    OU      organizationalUnitName
                    C       countryName
                    STREET  streetAddress
                    DC      domainComponent
                    UID     userid
```
