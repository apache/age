import java.sql.*;
import javax.sql.*;
import java.util.*;
import java.io.*;

public class jdbctest {
	public static void main(String[] args) {
		try {

		Properties prop = new Properties();
		prop.load(new FileInputStream("jdbctest.prop"));
		String url = prop.getProperty("jdbc.url");
		String user = prop.getProperty("jdbc.user");
		String pwd = prop.getProperty("jdbc.password");

		Connection conn = DriverManager.getConnection(url, user, pwd);
		conn.setAutoCommit(true);
		String sql = "INSERT INTO t1 VALUES (1);";
		Statement pst = conn.createStatement();
		pst.executeUpdate(sql);
		pst.close();
		PreparedStatement prest = conn.prepareStatement("SELECT * FROM t1");
		ResultSet rs = prest.executeQuery();
		rs.next();
		rs.close();
		prest.close();

		/*
		 * Cache test in a explicit transaction
		 */
		conn.setAutoCommit(false);
		// execute DML. This should prevent SELECTs from using query cache in the transaction.
		sql = "UPDATE t1 SET i = 2;";
		pst = conn.createStatement();
		pst.executeUpdate(sql);
		pst.close();
		// should not use the cache and should return "2", rather than "1"
		prest = conn.prepareStatement("SELECT * FROM t1");
		rs = prest.executeQuery();
		rs.next();
		System.out.println(rs.getInt(1));
		rs.close();
		prest.close();
		conn.commit();
		conn.close();

		} catch (Exception e) {
			System.err.println("jdbctest: ERROR: " + e);
			System.exit(1);
		}
	}
}
