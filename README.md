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

point of sale
-------------

In p2k12 you can buy stuff members put in. In order to keep the fridge in
healthy state where it is refilled we use
[nag-negative-balance.php](scripts/nag-negative-balance.php) to send out
"invoice" or more correctly reminders for people to fill up the fridge.

