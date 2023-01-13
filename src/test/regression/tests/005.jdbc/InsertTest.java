import java.sql.*;

public class InsertTest extends PgpoolTest {
    public void do_test() throws SQLException {
	int N = 100;

	try {
	    ResultSet rs;
	    PreparedStatement pstmt = null;
	    Statement stmt = null;

	    for (int i = 0; i < N; i++) {
		connection.setAutoCommit(false);
		pstmt = connection.prepareStatement("INSERT INTO ins VALUES (?)");
		pstmt.setInt(1, i);
		pstmt.executeUpdate();
		pstmt.close();
		connection.commit();
	    }
	    connection.setAutoCommit(true);
	    stmt = connection.createStatement();
	    rs = stmt.executeQuery("SELECT count(a) FROM ins" );
	    rs.next();
	    logwriter.println(rs.getInt(1));
	}
	finally {
	    connection.close();
	}
    }

    public String getTestName() {
	return "insert";
    }
}
