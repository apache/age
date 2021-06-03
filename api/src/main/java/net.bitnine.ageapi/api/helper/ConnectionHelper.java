package net.bitnine.ageapi.api.helper;

import lombok.Data;
import lombok.Getter;
import lombok.Setter;

//import org.apache.age.jdbc.base.Agtype;
import org.postgresql.PGConnection;
import org.postgresql.util.PSQLException;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.annotation.PropertySource;
import org.springframework.stereotype.Component;

import javax.annotation.PostConstruct;
import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;

@Component
@PropertySource("classpath:application.yml")
@Getter
@Setter
public class ConnectionHelper {

    private Connection connection;
    private Statement statement;
    private DataSource dataSource;

    @Value("${graphname}")
    private String graphname;

    public ConnectionHelper(DataSource dataSource) {
        try{
            this.dataSource = dataSource;
            this.connection = dataSource.getConnection();
            this.statement = connection.createStatement();
        }catch(Exception e){
            //e.printStackTrace();
        }
    }

    @PostConstruct
    public void init(){
        try{
            statement.execute("CREATE EXTENSION IF NOT EXISTS age;");
            statement.execute("LOAD 'age';");
            statement.execute("set search_path = a, ag_catalog, \"$user\", public");
            statement.execute("SELECT create_graph(\'" + graphname + "\'");
            System.out.println("graphname:"+graphname);
        }catch (SQLException e){
            //e.printStackTrace();
            System.out.println("graphname:"+graphname+" is already exists.");
        }
    }


}
