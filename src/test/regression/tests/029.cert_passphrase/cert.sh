#!/usr/bin/env bash

# Create root cert
openssl req -new -x509 -nodes -out root.crt -keyout root.key -days 365 -subj /CN=MyRootCA
# PostgreSQL/Pgpool cert with password
openssl genrsa -aes256 -out server.key -passout pass:pgpoolsecret 2048
openssl req -new -out server.req -key server.key -subj "/CN=postgresql" -passin pass:pgpoolsecret
openssl x509 -req -in server.req -CAkey root.key -CA root.crt -days 365 -CAcreateserial -out server.crt
# Frontend Cert
openssl req -new -out postgresql.req -keyout frontend.key -nodes -subj "/CN=$USER"
openssl x509 -req -in postgresql.req -CAkey root.key -CA root.crt -days 365 -CAcreateserial -out frontend.crt
