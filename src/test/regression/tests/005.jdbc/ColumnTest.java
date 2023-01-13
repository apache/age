import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

public class ColumnTest extends PgpoolTest {
    public static String sql = 
            "SELECT"
            +" columntest.v1 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz001,"
            +" columntest.v2 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz002,"
            +" columntest.v3 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz003,"
            +" columntest.v4 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz004,"
            +" columntest.v5 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz005,"
            +" columntest.v6 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz006,"
            +" columntest.v7 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz007,"
            +" columntest.v8 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz008,"
            +" columntest.v9 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz009,"
            +" columntest.v10 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz010,"
            +" columntest.v11 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz011,"
            +" columntest.v12 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz012,"
            +" columntest.v13 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz013,"
            +" columntest.v14 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz014,"
            +" columntest.v15 as abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz015"
            +" FROM columntest";
    static String url = "jdbc:postgresql://localhost:9999/test";
    static String user = "y-asaba";
    static String password = "postgres";
    static final int COUNT = 100;

    public void do_test() throws SQLException {
        connection.setAutoCommit(false);
        
        Statement stmt = null;
        ResultSet rs = null;
        
        try {
            for (int i = 0; i < COUNT; i++) {
		try {
		    stmt = connection.createStatement();
		    rs = stmt.executeQuery(sql);
		    
		    while(rs.next()){
			logwriter.println(rs.getInt(1));
		    }
		} finally {
		    if(rs != null) rs.close();
		    if(stmt != null) stmt.close();
		}
	    }
	} finally {
	    connection.close();
	    logwriter.close();
	}
    }

    public String getTestName() {
	return "column";
    }
}
