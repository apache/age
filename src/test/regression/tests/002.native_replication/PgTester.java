import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;

public class PgTester {

	public static void main(String[] args) {
		Integer startValue = 0;
		try{
			startValue = Integer.parseInt(args[0]);
		} catch (Exception e){
			System.out.println("Please provide a valid number as the first argument to the program");
			System.exit(-1);
		}

		
		Connection conn=null;
		try {
			Class.forName ("org.postgresql.Driver");

			conn =
				DriverManager.getConnection("jdbc:postgresql://localhost:11000/test");
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
		} catch (SQLException e) {
			e.printStackTrace();
		}

		try {
			//conn.setAutoCommit(false);
            PreparedStatement query = conn.prepareStatement("INSERT INTO public.sequencetester (recordno) VALUES (?)");
            int max = startValue + 10;
            for(int cv = startValue; cv <= max; cv++){
                query.setInt(1, cv);
                query.execute();
                //System.out.println("inserted "+cv);
            }
            query.close();
			//conn.commit();
            conn.close();
            System.out.println("Done");
        } catch (SQLException e) {
            e.printStackTrace();
        } 	
	}
}
