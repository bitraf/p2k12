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
