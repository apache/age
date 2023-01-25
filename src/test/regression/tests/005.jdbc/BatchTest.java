import java.util.*;
import java.io.*;
import java.sql.*;

public class BatchTest extends PgpoolTest {
    public String [] batchSqls = new String[] {
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
	"INSERT INTO batch VALUES (1)",
    };

    public void do_test() throws SQLException {
        connection.setAutoCommit(false);
        
        Statement stmt = null;
        ResultSet rs = null;
        
        try {
            try {
                stmt = connection.createStatement();
                for(int i=0; i<batchSqls.length; i++) {
                    stmt.addBatch(batchSqls[i]);
                }
                int [] r = stmt.executeBatch();
                
                connection.commit();
		stmt.close();

		stmt = connection.createStatement();
		rs = stmt.executeQuery("SELECT count(*) FROM batch");
		rs.next();
		logwriter.println(rs.getInt(1));
                connection.commit();
            } finally {
                if(rs != null) rs.close();
                if(stmt != null) stmt.close();
            }
        } finally {
            connection.close();
	    logwriter.close();
        }
    }

    public String getTestName() {
	return "batch";
    }
}
