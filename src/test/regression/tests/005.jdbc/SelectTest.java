import java.sql.*;
import java.io.*;

public class SelectTest extends PgpoolTest {
    public void do_test() throws SQLException {
	try {
	    for (int i = 0; i < 1; i++) {
		connection.setAutoCommit(false);
		Statement stmt = null;
		PreparedStatement pstmt = null;
		ResultSet rs = null;

		pstmt = connection.prepareStatement("SELECT count(*) FROM sel");
		rs = pstmt.executeQuery();
		rs.next();
		logwriter.println(rs.getInt(1));
		rs.close();
		pstmt.close();

		pstmt = connection.prepareStatement("SELECT sum(a) FROM sel");
		rs = pstmt.executeQuery();
		rs.next();
		logwriter.println(rs.getInt(1));
		rs.close();
		pstmt.close();
		connection.commit();
	    }
	} finally {
	    connection.close();
	    logwriter.close();
	}
    }

    public String getTestName() {
	return "select";
    }
}
