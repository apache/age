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

		String sql = "INSERT INTO t VALUES ('1', 'a', 'X');";
		Statement pst = conn.createStatement();
		pst.executeUpdate(sql);
		pst.close();
		conn.close();

		} catch (Exception e) {
			System.err.println("jdbctest: ERROR: " + e);
			System.exit(1);
		}
	}
}
