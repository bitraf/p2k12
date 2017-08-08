p2k12
=====

We use [Autoconf](http://en.wikipedia.org/wiki/Autoconf) to build p2k12. Please note that this document only covers how to setup on Debian.

**Dependencies**:

```
apt-get install libpq-dev
apt-get install libreadline5-dev # Or something similiar
```


Setting up a development environment
------------------------------------

Assuming you in the projects top directory, the following commands should apply:

```
$: autoreconf -i
$: ./configure
$: make
```

Create an initial database:

    sudo -u postgres psql
    postgres=# CREATE USER p2k12 ENCRYPTED PASSWORD 'secret password' SUPERUSER;
    postgres=# CREATE DATABASE p2k12 OWNER p2k12;

The migration scripts will take care of creating users and delegating permissions.

Put this in `~/.pgpass`:

    localhost:5432:*:p2k12:secret password:p2k12
    localhost:5432:*:p2k12_pos:secret password:p2k12_pos

The `p2k12` binary will connect as the `p2k12_pos` user.

### SQL schema management

Use `migrate` from [SQLAlchemy](https://sqlalchemy-migrate.readthedocs.org/en/latest/) to migrate the database schema to the latest
version. Install `migrate` with `pip install sqlalchemy-migrate`.

Create a configuration script to keep your database settings:

    migrate manage --repository=migrations --url=postgresql://p2k12@localhost/p2k12 manage.py

Mark your database as managed:

    python manage.py version_control

Migrate to the latest version of the database:

    python manage.py upgrade

point of sale
-------------

In p2k12 you can buy stuff members put in. In order to keep the fridge in
healthy state where it is refilled we use
[nag-negative-balance.php](scripts/nag-negative-balance.php) to send out
"invoice" or more correctly reminders for people to fill up the fridge.

live deployment
---------------

    configure --enable-live
    make
    sudo make install
