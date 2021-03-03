import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

class Executor {
	public static void main(String[] args) {
		select();
	}
	
	public static void select() {
		Connection conn = null;
        Statement stmt = null;
        ResultSet rset = null;

        String url = "jdbc:postgresql://127.0.0.1:5432/postgres";
        String user = "postgres";
        String password = "postgres";
		boolean tf = false;

        try {
            conn = DriverManager.getConnection(url, user, password);
            stmt = conn.createStatement();
			
			// Loading AGE
			tf = stmt.execute("CREATE EXTENSION IF NOT EXISTS age");
            tf = stmt.execute("LOAD 'age'");
            tf = stmt.execute("SET search_path = ag_catalog, \"$user\", public");
            
			// CREATE GRAPH
            rset = stmt.executeQuery("SELECT create_graph('g')");
			
			// MATCH (a) RETURN a 
            rset = stmt.executeQuery("SELECT * from cypher('g', $$ MATCH (a) RETURN a $$) as (a agtype)");
            
            while (rset.next()) {
            	System.out.println(rset.getString(1));
            }
        } catch (SQLException sqlEX) {
            System.out.println(sqlEX);
        } finally {
            try {
            	rset.close();
            	stmt.close();
                conn.close();
            } catch (SQLException sqlEX) {
                System.out.println(sqlEX);
            }
        }
	}

/*
    public static void insert() {
		Connection conn = null;
		PreparedStatement pstmt = null;
        ResultSet rset = null;
        int result = 0;
		
		String url = "jdbc:postgresql://127.0.0.1:5432/postgres";
        String user = "postgres";
        String password = "postgres";
		boolean tf = false;

        try {
			// transaction processing, auto commit - false
//			conn.setAutoCommit(false);

            conn = DriverManager.getConnection(url, user, password);
            pstmt = conn.createStatement();
			
			// Loading AGE
			tf = pstmt.execute("CREATE EXTENSION IF NOT EXISTS age");
            tf = pstmt.execute("LOAD 'age'");
            tf = pstmt.execute("SET search_path = ag_catalog, \"$user\", public");
            
            pstmt = conn.prepareStatement("SELECT * from cypher('g', $$ CREATE (a:Company {com_name: 'abc'}), (b:Company {com_name: 'def'}), (c:Company {com_name: 'ghi'}) $$) as (V agtype);");
            result = pstmt.executeUpdate();

			if (result == 0) {
				System.out.println("insert data fail");
			}
		} catch (SQLException sqlEX) {
			System.out.println(sqlEX);
		} finally {
			try {
				pstmt.close();
				conn.close();
			} catch (SQLException sqlEX) {
				System.out.println(sqlEX);
			}
		}
	}
    */
}