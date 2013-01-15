LDAP Foreign Data Wrapper for PostgreSQL 9.2
============================================


This is a very very very experimental PostgreSQL's extension 
thats implements a Foreign Data Wrapper (FDW) for the LDAP.

I'm using this code to learn FDW internals, and it is a prof of concept.

By all means use it, but do so entirely at your own risk! You have been
warned!

Do you like to use it in production? You are crazy, please **DO NOT USE IT ON PRODUCTION**. Well, not yet!

Do you like to help to improve it and turn it production-ready? Cool! You are
welcome! ":)

Building
--------

To build it, just do this:

    make
    make installcheck
    make install

If you encounter an error such as:

    "Makefile", line 8: Need an operator

You need to use GNU make, which may well be installed on your system as
`gmake`:

    gmake
    gmake installcheck
    gmake install

If you encounter an error such as:

    make: pg_config: Command not found

Be sure that you have `pg_config` installed and in your path. If you used a
package management system such as RPM to install PostgreSQL, be sure that the
`-devel` package is also installed. If necessary tell the build process where
to find it:

    env PG_CONFIG=/path/to/pg_config make && make installcheck && make install

And finally, if all that fails (and if you're on PostgreSQL 8.1 or lower, it
likely will), copy the entire distribution directory to the `contrib/`
subdirectory of the PostgreSQL source tree and try it there without
`pg_config`:

    env NO_PGXS=1 make && make installcheck && make install

If you encounter an error such as:

    ERROR:  must be owner of database regression

You need to run the test suite using a super user, such as the default
"postgres" super user:

    make installcheck PGUSER=postgres

Once ldap_fdw is installed, you can add it to a database. If you're running
PostgreSQL 9.1.0 or greater, it's a simple as connecting to a database as a
super user and running:

    CREATE EXTENSION ldap_fdw;

If you've upgraded your cluster to PostgreSQL 9.1 and already had ldap_fdw
installed, you can upgrade it to a properly packaged extension with:

    CREATE EXTENSION ldap_fdw FROM unpackaged;

For versions of PostgreSQL less than 9.1.0, you'll need to run the
installation script:

    psql -d mydb -f /path/to/pgsql/share/contrib/ldap_fdw.sql

If you want to install ldap_fdw and all of its supporting objects into a specific
schema, use the `PGOPTIONS` environment variable to specify the schema, like
so:

    PGOPTIONS=--search_path=extensions psql -d mydb -f ldap_fdw.sql

Dependencies
------------
The `ldap_fdw` FDW has the follow dependencies other than PostgreSQL:

* LDAP

Copyright and License
---------------------

This software is released under the PostgreSQL Licence.

Copyright (c) 2011 Dickson S. Guedes.
