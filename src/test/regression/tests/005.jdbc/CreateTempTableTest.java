/*
 * Test for native replication mode + temporary tables
 */
import java.sql.*;

public class CreateTempTableTest extends PgpoolTest {
    public void do_test() throws SQLException {

	try {
	    ResultSet rs;
	    PreparedStatement pstmt = null;
	    Statement stmt = null;

	    connection.setAutoCommit(false);

	    pstmt = connection.prepareStatement("CREATE TEMP TABLE t1(i INTEGER)");
	    pstmt.executeUpdate();
	    pstmt.close();

	    pstmt = connection.prepareStatement("/*NO LOAD BALANCE*/ INSERT INTO t1 SELECT ?");
	    pstmt.setInt(1, 100);
	    pstmt.executeUpdate();
	    pstmt.close();

	    pstmt = connection.prepareStatement("/*NO LOAD BALANCE*/ UPDATE t1 SET i = ?");
	    pstmt.setInt(1, 200);
	    pstmt.executeUpdate();
	    pstmt.close();

		pstmt = connection.prepareStatement("/*NO LOAD BALANCE*/ SELECT sum(i) FROM t1");
		rs = pstmt.executeQuery();
		rs.next();
		logwriter.println(rs.getInt(1));
		rs.close();
		pstmt.close();

	    connection.commit();
	}
	finally {
	    connection.close();
	    logwriter.close();
	}
    }

    public String getTestName() {
	return "CreateTempTable";
    }
}
