#!/usr/bin/env bash

[ -e "index.txt" ] && rm "index.txt"
touch index.txt
echo '1000' > serial
echo 'unique_subject = yes/no' > index.txt.attr
echo '1000' >  crlnumber
if [ -d "certrecord" ]; then rm -Rf certrecord; fi
mkdir certrecord
if [ -d "newcerts" ]; then rm -Rf newcerts; fi
mkdir newcerts

cat > crl_openssl.conf <<EOF

[ ca ]
default_ca = CA_default

[ CA_default ]
dir               = .
database          = index.txt
serial            = serial
certs             = newcerts
new_certs_dir     = certrecord

default_md        = sha256
crlnumber         = crlnumber
default_crl_days  = 365

name_opt          = ca_default
cert_opt          = ca_default
default_days      = 375
preserve          = no
policy            = policy_loose

# The root key and root certificate.
private_key       = root.key
certificate       = root.crt

[ policy_loose ]
# Allow the intermediate CA to sign a more diverse range of certificates.
# See the POLICY FORMAT section of the `ca` man page.
countryName             = optional
stateOrProvinceName     = optional
localityName            = optional
organizationName        = optional
organizationalUnitName  = optional
commonName              = supplied
emailAddress            = optional

[req]
distinguished_name  = req_distinguished_name
default_bits        = 2048

[req_distinguished_name]

EOF

# Print OpenSSL version
openssl version

# Create root cert
openssl req -new -x509 -nodes -out root.crt -keyout root.key -config crl_openssl.conf -days 365 -subj /CN=MyRootCA
# PostgreSQL/Pgpool cert
openssl req -new -out server.req -keyout server.key -config crl_openssl.conf -nodes -subj "/CN=postgresql"
openssl ca -batch -in server.req -config crl_openssl.conf -days 375 -notext -md sha256 -out server.crt

# Frontend Cert
openssl req -new -out frontend.req -keyout frontend.key -config crl_openssl.conf -nodes -subj "/CN=$USER"
openssl ca -batch -in frontend.req -config crl_openssl.conf -days 375 -notext -md sha256 -out frontend.crt

# Generate clean CRL (No revocation so far)
openssl ca -gencrl -config crl_openssl.conf -out server.crl -cert root.crt -keyfile root.key
# Revoke Frontend Cert
openssl ca -revoke frontend.crt -config crl_openssl.conf -keyfile root.key -cert root.crt -out root.crl
# Generate CRL after revocation
openssl ca -gencrl -config crl_openssl.conf -out server_revoked.crl -cert root.crt -keyfile root.key
