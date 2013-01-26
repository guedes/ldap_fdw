LDAP Foreign Data Wrapper for PostgreSQL 9.2
============================================

Synopsis
--------

A PostgreSQL's Foreign Data Wrapper (FDW) to query LDAP servers.

**DO NOT USE IT ON PRODUCTION**

It is not production ready but you could test it in your test
server and help us to improve it.

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
# Integration with Hstore

Well, you could do better than just retrieve that `object_body`! What about using [Hstore](http://www.postgresql.org/docs/9.2/static/hstore.html)?

See:

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



Support
-------

* [github](http://github.net/guedes/ldap_fdw)

Copyright and License
---------------------

This software is released under the PostgreSQL Licence.

Copyright (c) 2011-2013 Dickson S. Guedes.

