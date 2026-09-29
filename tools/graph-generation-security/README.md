# Graph generator permission reproducer

Run this only against a disposable PostgreSQL database with AGE installed. The
test creates and removes synthetic graphs and login roles. The generated logins
need trust authentication for a separate connection.

```sh
python3 -m pip install 'psycopg[binary]'
python3 reproduce.py --host 127.0.0.1 --dbname age_security_audit
```

Each case compares a denied SQL `INSERT` with a graph generator call and checks
the committed row counts from a new connection. `bypass: true` indicates the
generator wrote rows that the login could not insert with SQL. The RLS cases
grant table `INSERT` but force RLS with no `INSERT` policy.
