/*

# su - postgres

$ pgbench -i

$ cat jdbctest.prop
jdbc.url=jdbc:postgresql://localhost:9999/postgres
jdbc.user=postgres
jdbc.password=postgres

$ cat run.sh
java -Djdbc.drivers=org.postgresql.Driver \
     -classpath .:./postgresql-9.0-802.jdbc4.jar \
     "jdbctest2"

*/

import java.sql.*;
import javax.sql.*;
import java.util.*;
import java.io.*;

public class jdbctest2 {
	public static void main(String[] args) {
		try {


		Properties prop = new Properties();
		prop.load(new FileInputStream("jdbctest.prop"));
		String url = prop.getProperty("jdbc.url");
		String user = prop.getProperty("jdbc.user");
		String pwd = prop.getProperty("jdbc.password");

		Connection conn = DriverManager.getConnection(url, user, pwd);
		conn.setAutoCommit(false);

		String sql2 = "SELECT aid, bid, abalance, filler FROM pgbench_accounts WHERE aid != ? LIMIT ?";
		PreparedStatement pst2 = conn.prepareStatement(sql2);
		int aid = 1, lim = 7000, c = 0;
		ResultSet rs;
		String tmp;

		for (c = 0; c < 200; ++c) {
			aid = (int) Math.floor(Math.random() * 50000) + 1;
			lim = 7500 + (int) Math.floor(Math.random() * 1000) + 1;
			System.out.println("aid:" + aid + " lim:" + lim);
			System.out.flush();
			pst2.setInt(1, aid);
			pst2.setInt(2, lim);
			rs = pst2.executeQuery();
			while (rs.next()) {
				tmp = rs.getString(4);
			}
			rs.close();
		}
		pst2.close();
		conn.close();


		} catch (Exception e) {
			System.err.println("jdbctest: ERROR: " + e);
			System.exit(1);
		}
	}
}
