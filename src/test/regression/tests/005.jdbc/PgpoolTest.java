import java.util.*;
import java.io.*;
import java.sql.*;

public abstract class PgpoolTest {
    protected String url = "jdbc:postgresql://";
    protected String user;
    protected String password;
    protected String dbname;
    protected String host;
    protected String port;
    protected String options;
    protected Connection connection;
    protected PrintWriter logwriter;

    public PgpoolTest()
    {
	try {
	    Properties prop = new Properties();
	    prop.load(new FileInputStream("pgpool.properties"));
	    host = prop.getProperty("pgpooltest.host");
	    port = prop.getProperty("pgpooltest.port");
	    user = prop.getProperty("pgpooltest.user");
	    password = prop.getProperty("pgpooltest.password");
	    dbname = prop.getProperty("pgpooltest.dbname");
	    options = prop.getProperty("pgpooltest.options");
	    
	    url = url + host + ":" + port + "/" + dbname;

	    if (!options.equals(""))
		url = url + "?" + options;

	    File dir = new File("result");
	    dir.mkdir();

	    logwriter = new PrintWriter(new FileWriter("result/" + getTestName()), true);
	    try {
		Class.forName("org.postgresql.Driver");
		connection = DriverManager.getConnection(url, user, password);
	    } catch (ClassNotFoundException e) {
		System.out.println("cannot load JDBC Driver");
		System.exit(1);
	    } catch (Exception e) {
		System.out.println("cannot connect to pgpool");
		System.exit(1);
	    }
	} catch (Exception e) {
	    System.out.println("cannot read property file");
	    System.exit(1);
	}
    }

    public void check() {
	try {
	    String command_line = "diff -u expected/" + getTestName() + " result/" +
		getTestName();
	    Process proc;

	    proc = Runtime.getRuntime().exec(command_line);
	    proc.waitFor();
	    System.out.print(getTestName() + ": ");
	    if (proc.exitValue() == 0)
		System.out.println("ok");
	    else if (proc.exitValue() == 1)
		System.out.println("FAIL");
	    else
		System.out.println("unknown error" + proc.exitValue());
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }

    public abstract void do_test() throws SQLException, IOException;
    public abstract String getTestName();
}
