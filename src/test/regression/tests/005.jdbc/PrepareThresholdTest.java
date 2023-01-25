import java.util.*;
import java.io.*;
import java.sql.*;

class SqlItem {
	public String stmt;
	public Object[] params;
	SqlItem(String s, Object[] p) {
		stmt = s;
		params = p;
	}
};

public class PrepareThresholdTest extends PgpoolTest {

	private SqlItem [] sqllist;

	public void init_sqls() {
		java.sql.Timestamp ts =  new java.sql.Timestamp(new java.util.Date().getTime());
		sqllist = new SqlItem[] {
			new SqlItem("INSERT INTO prepare_threshold(i,t) VALUES(?, ?)", new Object[] {1, ts}),
			new SqlItem("INSERT INTO prepare_threshold(i) VALUES(?)",      new Object[] {1}),
			new SqlItem("INSERT INTO prepare_threshold(d) VALUES(?)",      new Object[] {ts})
		};
	}

	public void do_test() throws SQLException {
		init_sqls();
		try {
			PreparedStatement pstmt = null;
			ResultSet rs = null;
			try {
				connection.setAutoCommit(false);

				for (SqlItem sql: sqllist) {
					pstmt = connection.prepareStatement(sql.stmt);

					for (int i = 0; i < 10; i++) {
						for (int j = 0; j < sql.params.length; j++) {
							Object param = sql.params[j];
							if (param.getClass().getName().equals("java.sql.Timestamp")) {
								pstmt.setTimestamp(j+1, (java.sql.Timestamp) param);
							} else {
								pstmt.setObject(j+1, param);
							}
						}

						pstmt.executeUpdate();
					}
					connection.commit();
					pstmt.close();

				}

				pstmt = connection.prepareStatement("SELECT count(*) FROM prepare_threshold");
				rs = pstmt.executeQuery();
				rs.next();
				logwriter.println(rs.getInt(1));

			} finally {
				if (rs != null) rs.close();
				connection.commit();
				if (pstmt != null) pstmt.close();
			}

		} finally {
			connection.close();
			logwriter.close();
		}
	}

	public String getTestName() {
		return "PrepareThreshold";
	}
}
