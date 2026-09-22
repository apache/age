Setup using docker image https://age.apache.org/age-manual/master/intro/setup.html#installing-via-docker-image

Using command:
```
    docker run --name myPostgresDb -p 5455:5432 -e POSTGRES_USER=db_user -e POSTGRES_PASSWORD=db_password -e POSTGRES_DB=pg_test -d apache/age
```