# LDAP Sample in C

To use this program you need to install the OpenLDAP Development Libraries.
You can do this with these commands: 
# sudo apt update
# sudo apt install libldap2-dev

You can verify the installation like this: 
# find /usr/include -name ldap.h
The result should look something like this:
# /usr/include/ldap.h

Use OpenLDAP C-API
Ubuntu Packet libldap2-dev
include file <ldap.h>
gcc Option -lldap -llber
e.g.: g++ -std=c++14 -Wall -o myldap myldap.c -lldap -llber

Internal LDAP-Server address
Host: ldap.technikum-wien.at
Port: 389
Search Base: dc=technikum-wien,dc=at