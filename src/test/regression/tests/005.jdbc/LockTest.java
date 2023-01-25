import java.sql.*;
import java.io.*;

public class LockTest extends PgpoolTest {
    public final int COUNT = 100;
	
    public void do_test() throws SQLException, IOException {
	PreparedStatement pstmt = null;
	Statement stmt = null;
	ResultSet rs = null;
	try{
	    for (int i = 0; i < COUNT; i++) {
		connection.setAutoCommit(false);		
		/* table lock */
		stmt = connection.createStatement();
		stmt.executeUpdate("LOCK locktest");
				
		pstmt = connection.prepareStatement("SELECT * FROM locktest");
		rs = pstmt.executeQuery();
		rs.next();
		logwriter.println(rs.getInt(1));
		rs.close();	
		connection.commit();
	    }
	} finally {
	    if (connection != null) {
		connection.close();
	    }
	    
	}
    }

    public String getTestName() {
	return "lock";
    }
}
