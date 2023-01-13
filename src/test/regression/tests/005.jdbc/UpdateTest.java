import java.sql.*;

public class UpdateTest extends PgpoolTest {
    public void do_test() throws SQLException {
	int N = 100;

	try {
	    ResultSet rs;
	    Statement stmt = null;

	    for (int i = 0; i < N; i++) {
		connection.setAutoCommit(false);
		stmt = connection.createStatement();
		stmt.executeUpdate("UPDATE up SET a = a + 1" );
		stmt.close();
		connection.commit();
	    }
	    connection.setAutoCommit(true);
	    stmt = connection.createStatement();
	    rs = stmt.executeQuery("SELECT max(a) FROM up" );
	    rs.next();
	    logwriter.println(rs.getInt(1));
	}
	finally {
	    connection.close();
	}
    }

    public String getTestName() {
	return "update";
    }
}
