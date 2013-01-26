LDAP Foreign Data Wrapper for PostgreSQL 9.2
============================================

This is an initial working on a PostgreSQL's Foreign Data Wrapper (FDW)
to query LDAP servers.

By all means use it, but do so entirely at your own risk! You have been
warned!

Do you like to use it in production? **DO NOT USE IT ON PRODUCTION** without
a ton of tests before.

Please help us to improve and test it and turn it production-ready!

Dependencies
------------

The dependencies are:
 - PostgreSQL >= 9.2
 - OpenLDAP

To build you will also need:
 - GNU make
 - GCC

To install the build dependencies on RedHat based systems:

    yum install make postgresql92-devel gcc openldap-devel

The postgres92-devel package can be found in the PostgreSQL YUM Repository: http://yum.postgresql.org/
Compiling against 9.2 on RHEL6 is not yet supported.

To install the build dependencies on Debian based systems:

    apt-get install make libldap2-dev gcc


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

    export PATH="$PATH:/usr/pgsql-9.2/bin"
    make
    make installcheck
    make install

And finally, if all that fails, copy the entire distribution directory
to the `contrib/`subdirectory of the PostgreSQL source tree and try it
there without`pg_config`:

    env NO_PGXS=1 make && make installcheck && make install

If you encounter an error such as:

    ERROR:  must be owner of database regression

You need to run the test suite using a super user, such as the default
"postgres" super user:

    make installcheck PGUSER=postgres

Once ldap_fdw is installed, you can add it to a database connecting
as a super user and running:

    CREATE EXTENSION ldap_fdw;

Copyright and License
---------------------

This software is released under the PostgreSQL Licence.

Copyright (c) 2011 Dickson S. Guedes.
