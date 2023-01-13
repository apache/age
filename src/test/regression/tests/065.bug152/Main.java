import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

public class Main {
    public static void main(String[] args) throws Exception {
	//org.postgresql.Driver.setLogLevel(org.postgresql.Driver.DEBUG);
	Class.forName("org.postgresql.Driver");
	String url = "jdbc:postgresql://localhost:11000/test";
        try (Connection con = DriverManager.getConnection(url)) {
            try (Statement stmt = con.createStatement()) {
                stmt.execute("DROP TABLE IF EXISTS test");
                stmt.execute("CREATE TABLE test(id bigint NOT NULL PRIMARY KEY, description character varying)");
                stmt.execute("INSERT INTO test SELECT generate_series(1, 10) AS id, md5(random()::text) AS description");
            }
            StringBuilder sb = new StringBuilder();
            try (PreparedStatement pstmt = con.prepareStatement("SELECT description FROM test WHERE id = ?")) {
                for (int i = 0; i < 100; i++) {
	            System.out.println(1 + i % 10);
                    pstmt.setInt(1, 1 + i % 10);
                    try (ResultSet rs = pstmt.executeQuery()) {
                        rs.next();
                        sb.append(rs.getString(1));
                    }
                }
            }
        }
    }
}
