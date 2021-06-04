package net.bitnine.ageapi.api.service;


import net.bitnine.ageapi.api.dto.*;
import net.bitnine.ageapi.api.helper.ConnectionHelper;
import net.bitnine.ageapi.api.util.AGEJSONParse;
import net.bitnine.ageapi.api.util.processType;
import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;
import net.bitnine.agensgraph.deps.org.json.simple.parser.JSONParser;
import net.bitnine.agensgraph.deps.org.json.simple.parser.ParseException;
import org.postgresql.util.PSQLException;
import org.springframework.beans.factory.annotation.Autowired;

import org.springframework.boot.json.JacksonJsonParser;
import org.springframework.stereotype.Service;

import javax.sql.DataSource;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

@Service
public class AGEAPIService {

    private final DataSource dataSource;

    @Autowired
    private ConnectionHelper connectionHelper;
    private JSONParser jp;
    private AGEJSONParse AGEJSONParse;


    public AGEAPIService(DataSource dataSource) {
        this.dataSource = dataSource;
        jp = new JSONParser();
        AGEJSONParse = new AGEJSONParse();
    }



    //Vertex - Create
    public GraphIdResponseDto executeVertexCreate(CreateVertexRequestDto createVertexRequestDto)  {
        try (ResultSet resultSet = connectionHelper.getStatement().executeQuery(AGEJSONParse.createAgeQuery(null, (Object) createVertexRequestDto, connectionHelper.getGraphname(), processType.VERTEX_CREATE));) {
            if (resultSet.next())
                return new GraphIdResponseDto((resultSet.getString(1)));
        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
        return null;
    }

    //Vertex - Read
    public PropertyResponseDto executeVertexRead(String graphId)   {
        try(ResultSet resultSet = connectionHelper.getStatement().executeQuery(AGEJSONParse.createAgeQuery(graphId, null, connectionHelper.getGraphname(), processType.VERTEX_READ));) {
            if (resultSet.next())
                return new PropertyResponseDto((JSONObject) jp.parse((resultSet.getString(1))));
        } catch (SQLException | NullPointerException | ParseException e) {
            e.printStackTrace();
        }
        return null;
    }

    //Vertex - Update
    public void executeVertexUpdate(String graphId, UpdateRequestDto updateRequestDto) {
        try {
            connectionHelper.getStatement().execute(AGEJSONParse.createAgeQuery(graphId, updateRequestDto, connectionHelper.getGraphname(), processType.VERTEX_UPDATE));
        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
    }

    //Vertex - Delete
    public GraphIdResponseDto executeVertexDelete(String graphId){
        try(ResultSet resultSet = connectionHelper.getStatement().executeQuery(AGEJSONParse.createAgeQuery(graphId, null, connectionHelper.getGraphname(), processType.VERTEX_DELETE));) {
            if (resultSet.next())
                return new GraphIdResponseDto(resultSet.getString(1));
        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
        return null;
    }





    //E-C
    public GraphIdResponseDto executeEdgeCreate(CreateEdgeRequestDto createEdgeRequestDto) {
        try (ResultSet resultSet = connectionHelper.getStatement().executeQuery(AGEJSONParse.createAgeQuery(null, (Object) createEdgeRequestDto, connectionHelper.getGraphname(), processType.EDGE_CREATE))) {
            resultSet.next();
            return new GraphIdResponseDto((resultSet.getString(1)));
        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
        return null;
    }

    //E-R
    public PropertyResponseDto executeEdgeRead(String graphId)  {
        try {
            ResultSet resultSet = connectionHelper.getConnection()
                    .createStatement()
                    .executeQuery(
                            AGEJSONParse.createAgeQuery(graphId, null, connectionHelper.getGraphname(), processType.EDGE_READ));
            if (resultSet.next())
                return new PropertyResponseDto((JSONObject) jp.parse(resultSet.getString(1)));
            else
                return null;

        } catch (SQLException | NullPointerException | ParseException e) {
            e.printStackTrace();
        }
        return null;
    }

    //E-U
    public void executeEdgeUpdate(String graphId, UpdateRequestDto updateRequestDto) {
        try {
            connectionHelper.getStatement().executeQuery(AGEJSONParse.createAgeQuery(graphId, updateRequestDto, connectionHelper.getGraphname(), processType.EDGE_UPDATE));
        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
    }

    //E-D
    public GraphIdResponseDto executeEdgeDelete(String graphId)  {
        try {
            ResultSet resultSet = connectionHelper.getConnection()
                    .createStatement()
                    .executeQuery(
                            AGEJSONParse.createAgeQuery(graphId, null, connectionHelper.getGraphname(), processType.EDGE_DELETE));
            if (resultSet.next())
                return new GraphIdResponseDto(resultSet.getString(1));
            else
                return null;

        } catch (SQLException | NullPointerException e) {
            e.printStackTrace();
        }
        return null;
    }

}
