CREATE EXTENSION ldap_fdw;

CREATE SERVER ldap_test_server
FOREIGN DATA WRAPPER ldap_fdw
OPTIONS ( address 'localhost', port '389');

CREATE USER MAPPING FOR current_user
SERVER ldap_test_server
OPTIONS (user_dn 'cn=admin,dc=guedesoft,dc=net', password 'ldap');

CREATE FOREIGN TABLE ldap_people (
   dn text,
   object_body text
)
SERVER ldap_test_server
OPTIONS (base_dn 'ou=people,dc=guedesoft,dc=net');

CREATE FOREIGN TABLE ldap_john_smith (
   dn text,
   object_body text
)
SERVER ldap_test_server
OPTIONS (base_dn 'ou=people,dc=guedesoft,dc=net', query '(cn=John Smith)');

SELECT * FROM ldap_people WHERE dn = 'cn=admin';
SELECT object_body FROM ldap_people WHERE dn = 'cn=Dickson Guedes';
SELECT * FROM ldap_people;
SELECT * FROM ldap_john_smith;
