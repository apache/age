#! /bin/sh

rm -f *.class
rm -f result/*
javac *.java
java RunTest
