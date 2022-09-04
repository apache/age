<br>

<p align="center">
     <img src="https://age.apache.org/age-manual/master/_static/logo.png" width="30%" height="30%">
</p>
<br>

<h3 align="center">
    <a href="https://age.apache.org/age-manual/master/_static/logo.png" target="_blank">
        <img src="https://age.apache.org/age-manual/master/_static/logo.png"" height="25" height="30% alt="Apache AGE">
    </a>
    <a href="https://age.apache.org/age-manual/master/_static/logo.png" target="_blank">
    </a>
     is a leading multi-model graph database </h3>
     
</h3>

<h3 align="center">Graph Processing & Analytics for Relational Databases</h3>

<br>


</br>



<p align="center">                                                                                                    
  <a href="https://github.com/apache/age/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/releases">
    <img src="https://img.shields.io/badge/Release-v1.1.1-FFA500?labelColor=gray&style=flat&link=https://github.com/apache/age/releases"/>
  </a>
  &nbsp;
  <a href="https://www.postgresql.org/docs/12/index.html">
    <img src="https://img.shields.io/badge/Version-PostgreSQL 12-00008B?labelColor=gray&style=flat&link=https://www.postgresql.org/docs/12/index.html"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/issues">
    <img src="https://img.shields.io/github/issues/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/network/members">
    <img src="https://img.shields.io/github/forks/apache/age"/>
  </a>
  &nbsp;
  <a href="https://github.com/apache/age/stargazers">
    <img src="https://img.shields.io/github/stars/apache/age"/>
  </a>
  &nbsp;
  <a href="https://discord.gg/NMsBs9X8Ss">
    <img src="https://img.shields.io/discord/1022177873127280680.svg?label=discord&style=flat&color=5a66f6"></a>
</p>

<br>

Apache AGE is a PostgreSQL Extension that provides graph database functionality. AGE is an acronym for A Graph Extension, and is inspired by Bitnine's fork of PostgreSQL 10, AgensGraph, which is a multi-model database. The goal of the project is to create single storage that can handle both relational and graph model data so that users can use standard ANSI SQL along with openCypher, the Graph query language.

A graph consists of a set of vertices (also called nodes) and edges, where each individual vertex and edge possesses a map of properties. A vertex is the basic object of a graph, that can exist independently of everything else in the graph. An edge creates a directed connection between two vertices. A graph database is simply composed of vertices and edges. This type of database is useful when the meaning is in the relationships between the data. Relational databases can easily handle direct relationships, but indirect relationships are more difficult to deal with in relational databases. A graph database stores relationship information as a first-class entity. Apache AGE gives you the best of both worlds, simultaneously.

Apache AGE is:

Powerful -- AGE adds graph database support to the already popular PostgreSQL database: PostgreSQL is used by organizations including Apple, Spotify, and NASA.
Flexible -- AGE allows you to perform openCypher queries, which make complex queries much easier to write.
Intelligent -- AGE allows you to perform graph queries that are the basis for many next level web services such as fraud & intrusion detection, master data management, product recommendations, identity and relationship management, experience personalization, knowledge management and more.

## Overview

**This README file is for the Apache AGE PostgreSQL 12 release**

- **Apache AGE is currently being developed for the PostgreSQL 11 and 12 releases** and will support all the future releases of PostgreSQL.
- Apache AGE supports the openCypher graph query language.
- Apache AGE enables querying multiple graphs at the same time.
- Apache AGE will be enhanced with an aim to support all of the key features of AgensGraph (PostgreSQL fork extended with graph DB functionality).

## Latest happenings

- Latest Apache AGE for PG 12 release, [Apache AGE 1.1.1 (https://github.com/apache/age/releases/tag/PG12/v1.1.1-rc0).
- The latest Apache AGE documentation is now available at [here](https://age.apache.org/docs/master/index.html).
- The roadmap - [Apache AGE website](http://age.apache.org/).
- Send all your comments and inquiries to the user mailing list, users@age.apache.org.

## Installation

- [Use a docker image - official ver.](https://hub.docker.com/r/apache/age)
- [Installing from source](https://age.apache.org/#)

## Graph visualization tool for AGE

Apache AGE Viewer is a subproject of the Apache AGE project:  <https://github.com/apache/age-viewer>

- This is a visualization tool.
After AGE Extension Installation
You can use this tool to use the visualization features.
- Follow the instructions on the link to run it.
Under Connect to Database , select database type as "Apache AGE"

## Documentation

Here is the link to the latest [Apache AGE documentation](https://age.apache.org/docs/master/index.html).
You can learn about how to install Apache AGE, its features and built-in functions and how to use various Cypher queries.

## Language Specific Drivers

### Built-in

- [Go driver](./drivers/golang)
- [Java driver](./drivers/jdbc)
- [NodeJs driver](./drivers/nodejs)
- [Python driver](./drivers/python)

### Community

- [Apache AGE Python Driver](https://github.com/rhizome-ai/apache-age-python)
- [Apache AGE Rust Driver](https://github.com/Dzordzu/rust-apache-age.git)

## Contribution

You can improve ongoing efforts or initiate new ones by sending pull requests to [this repository](https://github.com/apache/age).
Also, you can learn from the code review process, how to merge pull requests, and from code style compliance to documentation, by visiting the [Apache AGE official site - Developer Guidelines](https://age.apache.org/#codereview).
