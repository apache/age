import java.sql.*;
import java.util.*;
import java.io.*;

public class RunTest {
    public static void main(String[] args)
    {
	RunTest test = new RunTest();
	test.run();
    }

    public void run()
    {
	try {
	    PgpoolTest test;
	    Properties prop = new Properties();
	    prop.load(new FileInputStream("pgpool.properties"));
	    String tests = prop.getProperty("pgpooltest.tests");
	    String host = prop.getProperty("pgpooltest.host");
	    String port = prop.getProperty("pgpooltest.port");
	    String user = prop.getProperty("pgpooltest.user");
	    String password = prop.getProperty("pgpooltest.password");
	    String dbname = prop.getProperty("pgpooltest.dbname");

	    // setup database
	    String command_line = "psql -f prepare.sql";
	    command_line = command_line + " -h " + host + " -p " + port +
		" -U " + user + " " + dbname;
	    Process proc = Runtime.getRuntime().exec(command_line);
	    proc.waitFor();

	    StringTokenizer tokenizer = new StringTokenizer(tests, " ");

	    while(tokenizer.hasMoreTokens()) {
		test = testFactory(tokenizer.nextToken());
		if (test == null) {
		    System.out.println("unknown testcase");
		    System.exit(1);
		}
		test.do_test();
		test.check();
	    }
	} catch (IOException e) {
	    System.out.println("cannot read property file");
	    System.exit(1);
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }

    public PgpoolTest testFactory(String testcase)
    {
	if (testcase == null)
	    return null;

	if (testcase.equals("autocommit"))
	    return new AutoCommitTest();

	if (testcase.equals("batch"))
	    return new BatchTest();

	if (testcase.equals("column"))
	    return new ColumnTest();

	if (testcase.equals("lock"))
	    return new LockTest();

	if (testcase.equals("select"))
	    return new SelectTest();

	if (testcase.equals("update"))
	    return new UpdateTest();

	if (testcase.equals("insert"))
	    return new InsertTest();

	if (testcase.equals("CreateTempTable"))
	    return new CreateTempTableTest();

	if (testcase.equals("PrepareThreshold"))
	    return new PrepareThresholdTest();

	/* unknown test case */
	return null;
    }
}
