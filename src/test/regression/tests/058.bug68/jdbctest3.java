import java.sql.*;
import javax.sql.*;
import java.util.*;
import java.io.*;
public class jdbctest3 {
  public static void main(String[] args) {
    try {
      Properties prop = new Properties();
      prop.load(new FileInputStream("jdbctest.prop"));
      String url = prop.getProperty("jdbc.url");
      String user = prop.getProperty("jdbc.user");
      String pwd = prop.getProperty("jdbc.password");
      Connection conn = DriverManager.getConnection(url, user, pwd);
      conn.setAutoCommit(false);
      String sql_u = "UPDATE pgbench_accounts SET filler = 'x' WHERE aid = ?";
      String sql_s = "SELECT * FROM pgbench_accounts WHERE aid != ? LIMIT 100";
      PreparedStatement pst_u, pst_s;
      int aid = 1, c1 = 0, c2 = 0;
      ResultSet rs;
      String tmp;
      for (c1 = 0; c1 < 10 ; ++c1) {
        pst_u = conn.prepareStatement(sql_u);
        pst_s = conn.prepareStatement(sql_s);
        aid = (int) Math.floor(Math.random() * 10) + 1;
        pst_u.setInt(1, aid);
        pst_u.executeUpdate();
        for (c2 = 0; c2 < 10; ++c2) {
          aid = (int) Math.floor(Math.random() * 10) + 1;
          pst_s.setInt(1, aid);
          rs = pst_s.executeQuery();
          while (rs.next()) { tmp = rs.getString(4); }
          rs.close();
        }
        conn.commit();
        pst_u.close();
        pst_s.close();
      }
      conn.close();
    } catch (Exception e) {
      System.err.println("jdbctest: ERROR: " + e);
      e.printStackTrace();
      System.exit(1);
    }
  }
}