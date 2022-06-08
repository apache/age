#! /bin/bash

set -e

readonly C_FILES=`find src -name "*.c" -type f ! -name "cypher_gram.c"`
readonly H_FILES=`find src -name "*.h" -type f`

for FILE in ${C_FILES}
do
	clang-format -style=file -i ${FILE}
done

for FILE in ${H_FILES}
do
	clang-format -style=file -i ${FILE}
done