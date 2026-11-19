<?php 

namespace Apache\AgePhp\AgeClient;

use Apache\AgePhp\AgeClient\Exceptions\AgeClientConnectionException;
use Apache\AgePhp\AgeClient\Exceptions\AgeClientQueryException;
use PgSql\Connection;
use PgSql\Result;

class AgeClient
{
    private Connection $connection;
    private ?Result $queryResult = null;
    private int $storedProcedureCount = 0;

    public function __construct(string $host, string $port, string $dbName, string $user, string $password)
    {
        $this->connection = pg_connect("host=$host port=$port dbname=$dbName user=$user password=$password");
        if (!$this->connection) throw new AgeClientConnectionException('Unable to connect to Postgres Database');
        
        pg_query($this->connection, "CREATE EXTENSION IF NOT EXISTS age;");
        pg_query($this->connection, "LOAD 'age';");
        pg_query($this->connection, "SET search_path = ag_catalog, '\$user', public;");
        pg_query($this->connection, "GRANT USAGE ON SCHEMA ag_catalog TO $user;");
    }

    public function close(): void
    {
        pg_close($this->connection);
    }

    /**
     * Flush outbound query data on the connection and any preparedId or queryResult
     */
    public function flush(): void
    {
        $this->prepareId = null;
        $this->queryResult = null;
        pg_flush($this->connection);
    }

    /**
     * Get the current open Postgresql connection to submit your own commands
     * @return Connection
     */
    public function getConnection(): Connection
    {
        return $this->connection;
    }

    public function createGraph(string $graphName): void
    {
        pg_query($this->connection, "SELECT create_graph('$graphName');");
    }

    public function dropGraph(string $graphName): void
    {
        pg_query($this->connection, "SELECT drop_graph('$graphName', true);");
    }

    /**
     * Execute Postgresql and Cypher queries, refer to Apache AGE documentation for query examples
     * @param string $query - Postgresql-style query
     * @return AgeClient
     */
    public function query(string $query): AgeClient
    {
        $this->flush();

        $queryResult = pg_query($this->connection, $query);
        if ($queryResult) $this->queryResult = $queryResult;

        return $this;
    }

    /**
     * Execute prepared cypher-specific command without to specify Postgresql SELECT or agtypes
     * @param string $graphName
     * @param string $columnCount - number of agtypes returned, will throw an error if incorrect
     * @param string $cypherQuery - Parameterized Cypher-style query
     * @param array $params - Array of parameters to substitute for the placeholders in the original prepared cypherQuery string
     * @return AgeClient
     */
    public function cypherQuery(string $graphName, int $columnCount, string $cypherQuery, array $params = []): AgeClient
    {
        $this->flush();

        $jsonParams = json_encode($params);

        $storedProcedureName = "cypher_stored_procedure_" . $this->storedProcedureCount++;
        $statement = "
            PREPARE $storedProcedureName(agtype) AS
            SELECT * FROM cypher('$graphName', \$\$ $cypherQuery \$\$, \$1) as (v0 agtype";

        for ($i = 1; $i < $columnCount; $i++) {
            $statement .= sprintf(", v%d agtype", $i);
        }

        $statement .= ");
            EXECUTE $storedProcedureName('$jsonParams')
        ";

        $queryResult = pg_query($this->connection, $statement);
        if ($queryResult) $this->queryResult = $queryResult;

        return $this;
    }

    /**
     * Get a row as an enumerated array with parsed AgTypes
     * @param int? $row
     * @return array|null
     * @throws AgeClientQueryException
     */
    public function fetchRow(?int $row = null): array|null
    {
        if (!$this->queryResult) throw new AgeClientQueryException('No result from prior query to fetch from');
        $fetchResults = pg_fetch_row($this->queryResult, $row);

        $parsedResults = [];
        foreach ($fetchResults as $key => $column) {
            $parsedResults[$key] = $column ? AGTypeParse::parse($column) : null;
        }

        return $parsedResults;
    }

    /**
     * Fetches all rows as an array with parsed AgTypes
     * @return array|null
     * @throws AgeClientQueryException
     */
    public function fetchAll(): array|null
    {
        if (!$this->queryResult) throw new AgeClientQueryException('No result from prior query to fetch from');
        $fetchResults = pg_fetch_all($this->queryResult);

        if (!$fetchResults) return null;

        $parsedResults = [];
        foreach ($fetchResults as $row) {
            $rowData = [];
            foreach ($row as $key => $column) {
                $rowData[$key] = $column ? AGTypeParse::parse($column) : null;
            }
            $parsedResults[] = $rowData;
        }

        return $parsedResults;
    }
}