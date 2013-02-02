LDAP Foreign Data Wrapper for PostgreSQL 9.2
============================================

Synopsis
--------

A PostgreSQL's Foreign Data Wrapper (FDW) to query LDAP servers.

Warnings
--------

* **DO NOT USE IT ON PRODUCTION**: It is not production ready but you could test it in your test
server and help us to improve it;
* it not implements limits *yet*, so if you do a `select * from ldap_table` it will fetch all records from LDAP server. Some LDAP servers limits this to 500;
* by now it only supports two columns: `dn` and `object_body` where the former is populated with the DN and the last with the LDAP entry converted to a Hstore compatible format;

Description
-----------

LDAP Foreign Data Wrapper for PostgreSQL 9.2.

Usage
-----

Create the extension in your database:

    CREATE EXTENSION ldap_fdw;

Then create a foreign server to connect to your LDAP
server:

    CREATE SERVER ldap_myldap_server
    FOREIGN DATA WRAPPER ldap_fdw
    OPTIONS ( address 'myldap_server_address', port '389');

Create user mapping:

    CREATE USER MAPPING FOR current_user
    SERVER ldap_myldap_server
    OPTIONS (user_dn 'cn=SomeUser,dc=example,dc=com', password 'the_user_password');

Finally create a foreign table with a base DN pointing to some OU:

    CREATE FOREIGN TABLE ldap_people (
       dn text,
       object_body text
    )
    SERVER ldap_myldap_server
    OPTIONS (base_dn 'OU=people,DC=example,DC=com');

And voila!

    > SELECT * FROM ldap_people;

                            dn                         |                          object_body                           
    ---------------------------------------------------+----------------------------------------------------------------
     cn=Dickson Guedes,ou=people,dc=example,dc=net     | cn => "Dickson Guedes",                                       +
                                                       | gidNumber => "500",                                           +
                                                       | homeDirectory => "/home/users/guedes",                        +
                                                       | sn => "Guedes",                                               +
                                                       | loginShell => "/bin/sh",                                      +
                                                       | objectClass => "{\"inetOrgPerson\",\"posixAccount\",\"top\"}",+
                                                       | userPassword => "ldap",                                       +
                                                       | uidNumber => "1000",                                          +
                                                       | uid => "guedes",                                              +
                                                       | givenName => "{\"Dickson, Guedes\",\"Gueduxo\"}",             +
                                                       | 
     cn=John Smith,ou=people,dc=example,dc=net         | cn => "John Smith",                                           +
                                                       | givenName => "John",                                          +
                                                       | gidNumber => "500",                                           +
                                                       | homeDirectory => "/home/users/jsmith",                        +
                                                       | sn => "Smith",                                                +
                                                       | loginShell => "/bin/sh",                                      +
                                                       | objectClass => "{\"inetOrgPerson\",\"posixAccount\",\"top\"}",+
                                                       | userPassword => "{SSHA}y0GfklAHS9AEDz87AdQ+UAQi3bGlfqXt",     +
                                                       | uidNumber => "1001",                                          +
                                                       | uid => "jsmith",                                              +
                                                       | 

If you use a filter on `dn` column `ldap_fdw` will convert it to LDAP dialect and will send it to the server and this one
will reply less entries. By now, only `dn` has this behaviour.

See:

    > SELECT object_body FROM ldap_people WHERE dn = 'cn=Dickson Guedes';

                              object_body                           
    ----------------------------------------------------------------
     cn => "Dickson Guedes",                                       +
     gidNumber => "500",                                           +
     homeDirectory => "/home/users/guedes",                        +
     sn => "Guedes",                                               +
     loginShell => "/bin/sh",                                      +
     objectClass => "{\"inetOrgPerson\",\"posixAccount\",\"top\"}",+
     userPassword => "ldap",                                       +
     uidNumber => "1000",                                          +
     uid => "guedes",                                              +
     givenName => "{\"Dickson, Guedes\",\"Gueduxo\"}",             +
     
    (1 row)

## Retrieving specific attributes

You can choose a list of attributes to retrieve, just add an option `attributes` to FOREIGN TABLE's options with 
a comma-separeted attributes like this:

    ALTER FOREIGN TABLE ldap_people
    OPTIONS ( ADD attributes 'gidNumber,uidNumber,homeDirectory' );

    SELECT * FROM ldap_people WHERE dn = 'cn=John Smith';

                         dn                      |              object_body               
    ---------------------------------------------+----------------------------------------
     cn=John Smith,ou=people,dc=guedesoft,dc=net | gidNumber => "500",                   +
                                                 | uidNumber => "1001",                  +
                                                 | homeDirectory => "/home/users/jsmith",+
                                                 | 
    (1 row)

# Integration with Hstore

Well, you could do better than just retrieve that `object_body`! What about using [Hstore](http://www.postgresql.org/docs/9.2/static/hstore.html)?

See:

    > CREATE EXTENSION hstore;
    > WITH
        _hstore as (
           SELECT hstore(object_body) as h
           FROM ldap_people
         )
         SELECT h -> 'cn' as cn,
                h -> 'objectClass' as object_class,
                h -> 'homeDirectory' as home
         FROM _hstore;

            cn        |              object_class              |         home         
    ------------------+----------------------------------------+----------------------
     Dickson Guedes   | {"inetOrgPerson","posixAccount","top"} | /home/users/guedes
     John Smith       | {"inetOrgPerson","posixAccount","top"} | /home/users/jsmith


# Integration with pgcrypto

Your application doesn't know how to query a LDAP server? But it know `SELECT`? What about
check if some user's password is correct using your PostgreSQL server as a `proxy`?

Yes, you can! See:

    > CREATE EXTENSION pgcrypto;
    > WITH
     _hstore as
     (
        SELECT hstore(object_body) as h
        FROM ldap_people
        WHERE dn = 'cn=John Smith'
     ),
     _user_pass as
     (
        SELECT  substr(decode(substr( h -> 'userPassword' , 7 ), 'base64'), 21) as salt,
                h -> 'userPassword' as encrypted_password
        FROM _hstore
    ),
    _generated_pass as
    (
        SELECT '{SSHA}' || encode(digest( 'TheUserPassword123' || salt, 'sha1') || salt, 'base64') as password,
               encrypted_password
        FROM _user_pass
    )
    SELECT password = encrypted_password   as password_match
    FROM  _generated_pass;

     password_match 
    ----------------
     t
    (1 row)

Well too verbose, but you could use a function, no? :)

Support
-------

* [github](http://github.net/guedes/ldap_fdw)

Copyright and License
---------------------

This software is released under the PostgreSQL Licence.

Copyright (c) 2011-2013 Dickson S. Guedes.

