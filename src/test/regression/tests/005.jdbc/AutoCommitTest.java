import java.util.*;
import java.io.*;
import java.sql.*;

public class AutoCommitTest extends PgpoolTest{
    public String sql = "select * from autocommit where a = ?";

    public static void main(String[] args) throws SQLException {
	AutoCommitTest test = new AutoCommitTest();
	test.do_test();
    }

    public void do_test() throws SQLException
    {
        connection.setAutoCommit(true);
        
        PreparedStatement stmt = null;
        ResultSet rs = null;
        
        try {
            for(int i = 0; i < 10; i++) {
                stmt = connection.prepareStatement(sql);
                try {
                    stmt.setInt(1, i + 1);
                    rs = stmt.executeQuery();
                    
                    while(rs.next()){
                        logwriter.print(rs.getInt(1) + " ");
                    }
                    logwriter.println();
                } finally {
                    if(rs != null) rs.close();
                    if(stmt != null) stmt.close();
                }
            }
        } finally {
            connection.close();
	    logwriter.close();
        }	
    }

    public String getTestName() {
	return "autocommit";
    }
}
