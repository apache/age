import java.io.PrintWriter;
import java.io.FileInputStream;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.Statement;
import java.util.Properties;

import org.postgresql.Driver;

public class Sample {
    public Sample() {
    }

    public static void main(String[] args) throws Exception {
        ResultSet rs;
        Properties props = new Properties();
        props.load(new FileInputStream("javatest.prop"));
        String url = props.getProperty("jdbc.url");
        String user = props.getProperty("jdbc.user");
        String pwd = props.getProperty("jdbc.password");

        DriverManager.setLogWriter(new PrintWriter(System.out));
        Connection conn = DriverManager.getConnection(url, user, pwd);
        conn.setAutoCommit(true);
        Statement st = conn.createStatement();
        st.setFetchSize(100);
	// Does not hit cache
        rs = st.executeQuery("SELECT 1");
        while (rs.next()) {
            System.out.println(rs.getString(1));
        }
        rs.close();

	// Does hit cache
        rs = st.executeQuery("SELECT 1");
        while (rs.next()) {
            System.out.println(rs.getString(1));
        }
        rs.close();

	// To call do_query()
        rs = st.executeQuery("SELECT * FROM t1");
        while (rs.next()) {
            System.out.println(rs.getString(1));
        }
        rs.close();

	conn.close();
    }
}
