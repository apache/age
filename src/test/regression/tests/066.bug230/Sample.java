import java.io.PrintWriter;
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
        Properties props = new Properties();
        props.setProperty("user", System.getenv("USER"));
        props.setProperty("password", "");
        DriverManager.setLogWriter(new PrintWriter(System.out));
	//        Driver.setLogLevel(2);
        Connection conn = DriverManager.getConnection(
                "jdbc:postgresql://localhost:11000/test", props);
        conn.setAutoCommit(false);
        Statement st = conn.createStatement();
        st.setFetchSize(100);
        ResultSet rs = st.executeQuery("SELECT * from GENERATE_SERIES(1,1000)");
        while (rs.next()) {
            System.out.println(rs.getString(1));
        }
        rs.close();
	conn.close();
    }
}
