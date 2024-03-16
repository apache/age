import org.apache.age.jdbc.base.Agtype;
import org.postgresql.jdbc.PgConnection;

import java.sql.*;

public class AgeBasic {

    public static PgConnection connection;
    static final String DB_URL = "jdbc:postgresql://localhost:5432/demo";
    static final String USER = "postgres";
    static final String PASS = "pass";

    public static void main(String[] args) {

        // Open a connection
        try {
            makeConnection();

            if (graphExist("demo_graph"))
                dropGraph();

            createGraph();
            getResult();
            getObjectsFromResult();

            // after all
            dropGraph();
            closeConnection();
        }
        catch (Exception e) {
            System.out.println(e.getMessage());
        }

    }

    /**
     * Creating a connection with the database
     * Loading AGE extension & configuring database for AGE
     */
    public static void makeConnection() throws SQLException {
        connection = DriverManager.getConnection(DB_URL, USER, PASS).unwrap(PgConnection.class);
        connection.addDataType("agtype", Agtype.class);

        // configure AGE
        Statement statement = connection.createStatement();
        statement.execute("CREATE EXTENSION IF NOT EXISTS age;");
        statement.execute("LOAD 'age'");
        statement.execute("SET search_path = ag_catalog, \"$user\", public;");
    }
    
    /**
     * Using Agtype parser on the ResultSet to get the result in Agtype.
     */
    public static void getResult() throws SQLException {
        Statement stmt = connection.createStatement();
        ResultSet rs = stmt.executeQuery("SELECT * from cypher('demo_graph', $$ MATCH (n) RETURN n $$) as (n agtype);");
        
        // Returning result as Agtype
        System.out.println("\nRESULT AS AGTYPE");
        while (rs.next()) {
            Agtype returnedAgtype = rs.getObject(1, Agtype.class);
            System.out.println(returnedAgtype.getValue());
        }
    }

    /**
     * Using Agtype to extract specific objects from the Agtype result.
     */
    public static void getObjectsFromResult() throws SQLException {
        Statement stmt = connection.createStatement();
        ResultSet rs = stmt.executeQuery("SELECT * from cypher('demo_graph', $$ MATCH (n) RETURN n $$) as (n agtype);");

        System.out.println("\nEXTRACTING OBJECTS FROM RESULT");
        while (rs.next()) {
            Agtype returnedAgtype = rs.getObject(1, Agtype.class);
            String nodeLabel = returnedAgtype.getMap().getObject("label").toString();
            String nodeProp = returnedAgtype.getMap().getObject("properties").toString();

            System.out.println("Vertex : " + nodeLabel + ", \tProps : " + nodeProp);
        }
    }

    /**
     * Creating a 'demo_graph'.
     * Then adding some Nodes.
     * Then connecting them with some relation.
     */
    public static void createGraph() throws SQLException {
        Statement statement = connection.createStatement();

        // Nodes labeled as PERSON
        statement.execute("SELECT create_graph('demo_graph');");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'imran', bornIn : 'Pakistan'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'ali', bornIn : 'Pakistan'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'usama', bornIn : 'Pakistan'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'akbar', bornIn : 'Pakistan'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'james', bornIn : 'US'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'david', bornIn : 'US'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Person {name : 'max', bornIn : 'US'}) $$) AS (a agtype);");

        // Nodes labeled as COUNTRY
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Country{name : 'Pakistan'}) $$) AS (a agtype);");
        statement.execute("SELECT * FROM cypher('demo_graph', $$ CREATE (n:Country{name : 'US'}) $$) AS (a agtype);");

        // creating relationship (n:PERSON)-[r:BORNIN]-(n:COUNTRY)
        statement.execute("SELECT * FROM cypher('demo_graph', $$ MATCH (a:Person), (b:Country) WHERE a.bornIn = b.name CREATE (a)-[r:BORNIN]->(b) RETURN r $$) as (r agtype);");
    }

    /**
     * After done querying drop the whole graph.
     * Dropping the GRAPH with CASCADE:true. Deletes the whole data in the graph.
     */
    public static void dropGraph() throws SQLException {
        Statement statement = connection.createStatement();
        statement.execute("SELECT * FROM ag_catalog.drop_graph('demo_graph', true);");
    }

    public static boolean graphExist(String graph_name) throws SQLException {

        PreparedStatement ps = connection.prepareStatement("SELECT * FROM ag_catalog.ag_graph WHERE name = ?");
        ps.setString(1, graph_name);
        ps.executeQuery();
        ResultSet rs = ps.getResultSet();
        return !(!rs.isBeforeFirst() && rs.getRow() == 0);
    }

    /**
     * At last close the connection before exit.
     */
    public static void closeConnection() throws SQLException {
        connection.close();
    }
    
}