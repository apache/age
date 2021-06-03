package net.bitnine.ageapi.api.util;


import net.bitnine.ageapi.api.dto.CreateEdgeRequestDto;
import net.bitnine.ageapi.api.dto.CreateVertexRequestDto;
import net.bitnine.ageapi.api.dto.UpdateRequestDto;
import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;
import net.bitnine.agensgraph.deps.org.json.simple.parser.JSONParser;
import net.bitnine.agensgraph.deps.org.json.simple.parser.ParseException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class AGEJSONParse {

    public static final Logger LOG = LoggerFactory.getLogger(AGEJSONParse.class);


    //To make Age query format, just added prefix and suffix
    //Main cypher query is made by function "toCypherQuery"
    public String createAgeQuery(Object object1, Object object2,String graphname, processType pType) {
        return "SELECT * from cypher('"+ graphname.replaceAll("\"","") + "', $$"
                + toCypherQuery(object1, object2, pType)
                + "$$) as (a agtype)";
    }

    //Receives many dto objects and do functional transaction
    public String toCypherQuery(Object id, Object dto, processType processType)  {
        //object1 : Graphid string
        //object2 : Requestdto
        //processType : processType
        String cypherString = null;
        String key = null;
        Object val = null;
        Iterator itr;
        Object requestDto = null;


        try{

        //This switch takes main control flow.
        //Each cases make their requestDto into cypherString which can be used in function "createAgeQuery"
        switch (processType) {
            case VERTEX_CREATE:
                CreateVertexRequestDto createVertexRequestDto = (CreateVertexRequestDto) dto;
                cypherString = "create (a:" + createVertexRequestDto.getVLabel() + " "
                        + "{ " + JSONPropertyParsing(createVertexRequestDto.getProperties())
                        + "}) return id(a)";
                return cypherString;
            case EDGE_CREATE:
                CreateEdgeRequestDto createEdgeRequestDto = (CreateEdgeRequestDto) dto;
                cypherString = "MATCH (b), (c) WHERE "
                        + " id(b) = " + createEdgeRequestDto.getSource() + " and "
                        + " id(c) = " + createEdgeRequestDto.getTarget() + " "
                        + " CREATE (b)-[a: " + createEdgeRequestDto.getELabel()
                        + "{ " + JSONPropertyParsing(createEdgeRequestDto.getProperties())
                        + "}]->(c) return id(a)";
                return cypherString;
            case VERTEX_READ:
                cypherString = "match (a) where id(a)= " + (String) id
                        + " return properties(a)";
                return cypherString;
            case EDGE_READ:
                cypherString = "match ()-[a]->() where id(a)= " + (String) id
                        + " return properties(a)";
                return cypherString;
            case VERTEX_UPDATE:
                UpdateRequestDto updateVertexRequestDto = (UpdateRequestDto) dto;
                itr = JSONString_to_Map(updateVertexRequestDto.getProperties().toJSONString()).keySet().iterator();
                key = (String) itr.next();
                val = updateVertexRequestDto.getProperties().get(key);
                cypherString = "match (a) match (a) where id(a)= " + (String) id
                        + " set a." + key + " = " + cypherStringAdjuster(val);
                return cypherString;
            case EDGE_UPDATE:
                UpdateRequestDto updateEdgeRequestDto = (UpdateRequestDto) dto;
                itr = JSONString_to_Map(updateEdgeRequestDto.getProperties().toJSONString()).keySet().iterator();
                key = (String) itr.next();
                val = updateEdgeRequestDto.getProperties().get(key);
                cypherString = "match ()-[a]->() match (a) where id(a)= " + (String) id
                        + " set a." + key + " = " + cypherStringAdjuster(val);
                return cypherString;
            case VERTEX_DELETE:
                cypherString = "MATCH(a) WHERE id(a) = " + (String) id
                        + "DELETE (a) RETURN id(a)";
                return cypherString;
            case EDGE_DELETE:
                cypherString = "MATCH ()-[a]->() WHERE id(a) = " + (String) id
                        + "DELETE (a) RETURN id(a)";
                return cypherString;
        }

        }catch (ParseException e){
            e.printStackTrace();
        }

        return null;
    }

    //Some JSON property strings need some refine process to make clear cypher string.
    public String JSONPropertyParsing(JSONObject properties) {

        JSONParser jsonParser = new JSONParser();
        HashMap<String, Object> parsedProperties = properties;
        Iterator<String> propertiesIterator = parsedProperties.keySet().iterator();

        String cypherString = "";

        while (propertiesIterator.hasNext()) {
            String key = propertiesIterator.next();

            if (parsedProperties.get(key) instanceof String) {
                cypherString = cypherString + key + ":" + "\'" + parsedProperties.get(key) + "\'";
            } else {
                cypherString = cypherString + key + ":" + parsedProperties.get(key);
            }
            if (propertiesIterator.hasNext()) {
                cypherString = cypherString + ",";
            }
        }
        return cypherString;
    }

    public Object cypherStringAdjuster(Object obj) {

        if (obj instanceof String) {
            return "\"" + obj + "\"";
        } else
            return obj;
    }

    //This function is designed for iterating unclear json property
    //Extracting key from JSONObject
    public Map<String, Object> JSONString_to_Map(String JSONString) throws ParseException {
        JSONParser jp = new JSONParser();
        JSONObject jo = (JSONObject) jp.parse(JSONString);
        Iterator itr = jo.keySet().iterator();
        String k = null;
        HashMap<String, Object> returnMap = new HashMap<>();
        while (itr.hasNext()) {
            k = itr.next().toString();
            returnMap.put(k, jo.get(k));
            if (!itr.hasNext())
                return returnMap;
        }
        return null;
    }


}
