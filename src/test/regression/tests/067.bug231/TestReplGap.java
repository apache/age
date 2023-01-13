import java.sql.*;

public class TestReplGap {

    public static void main(String[] args) throws SQLException, InterruptedException {

	try (Connection conn = DriverManager.getConnection("jdbc:postgresql://localhost:11000/test?loglevel=2", "t-ishii", "")) {

		conn.setAutoCommit(false);
		// Note: It is supposed that the database postgres contains table "t1".
		try (PreparedStatement stmt = conn.prepareStatement("select * from t1 where id = ? ")) {

			for (int i = 0; i < 100; i++) {
			    stmt.setInt(1, i);
			    stmt.executeQuery().close();

			    System.out.println(i);
			}
		    }

		conn.commit();
		conn.close();

		System.out.println("DONE");
	    } catch (SQLException ex) {
	    ex.printStackTrace();
	}
    }
}
