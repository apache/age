package net.bitnine.ageapi.api.controller;


import lombok.RequiredArgsConstructor;
import net.bitnine.ageapi.api.dto.*;
import net.bitnine.ageapi.api.service.AGEAPIService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;
import java.sql.SQLException;

@RestController
@RequiredArgsConstructor
public class AGEAPIController {

    AGEAPIService ageapiService;

    @Autowired
    public AGEAPIController(AGEAPIService ageapiService) {
        this.ageapiService = ageapiService;
    }


    //Vertex Create
    @PostMapping(path = "/api/create/vertex")
    public GraphIdResponseDto executeVertexCreate(@RequestBody CreateVertexRequestDto createVertexRequestDto) throws SQLException , ClassNotFoundException {
        return ageapiService.executeVertexCreate(createVertexRequestDto);
    }

    //Vertex Read
    @GetMapping(path = "/api/read/vertex/{graphId}")
    public PropertyResponseDto executeVertexRead(@PathVariable String graphId) throws SQLException  {
        return ageapiService.executeVertexRead(graphId);
    }

    //Vertex Update
    @PutMapping(path = "/api/update/vertex/{graphId}")
    public void executeVertexUpdate(@PathVariable String graphId, @RequestBody UpdateRequestDto updateRequestDto) throws SQLException{
        ageapiService.executeVertexUpdate(graphId, updateRequestDto);
    }

    //Vertex Delete
    @DeleteMapping(path = "/api/delete/vertex/{graphId}")
    public GraphIdResponseDto executeVertexDelete(@PathVariable String graphId) throws SQLException  {
        return ageapiService.executeVertexDelete(graphId);
    }


    //Edge Create
    @PostMapping(path = "/api/create/edge")
    public GraphIdResponseDto executeEdgeCreate(@RequestBody CreateEdgeRequestDto createEdgeRequestDto) throws SQLException  {
        return ageapiService.executeEdgeCreate(createEdgeRequestDto);
    }

    //Edge Update
    @PutMapping(path = "/api/update/edge/{graphId}")
    public void executeEdgeUpdate(@PathVariable String graphId, @RequestBody UpdateRequestDto updateRequestDto) throws SQLException {
        ageapiService.executeEdgeUpdate(graphId, updateRequestDto);
    }

    //Edge Read
    @GetMapping(path = "/api/read/edge/{graphId}")
    public PropertyResponseDto executeEdgeRead(@PathVariable String graphId) throws SQLException  {
        return ageapiService.executeEdgeRead(graphId);
    }

    //Edge Delete
    @DeleteMapping(path = "/api/delete/edge/{graphId}")
    public GraphIdResponseDto executeEdgeDelete(@PathVariable String graphId) throws SQLException {
        return ageapiService.executeEdgeDelete(graphId);
    }



}
