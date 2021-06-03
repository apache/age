package net.bitnine.ageapi.api.service;

import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import lombok.Getter;
import lombok.Setter;
import org.junit.jupiter.api.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.configurationprocessor.json.JSONObject;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.request.MockHttpServletRequestBuilder;
import org.springframework.test.web.servlet.request.MockMvcRequestBuilders;
import org.springframework.transaction.annotation.Transactional;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.springframework.test.web.servlet.result.MockMvcResultHandlers.print;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@SpringBootTest
@AutoConfigureMockMvc
@TestMethodOrder(MethodOrderer.OrderAnnotation.class)
@Setter
@Getter
@Transactional
@TestInstance(TestInstance.Lifecycle.PER_CLASS)
public class AGEAPIServiceTest {

    private JSONObject vertex1;
    private JSONObject vertex2;
    private JSONObject edge1;

    private JsonParser jp;
    private JSONObject jo;

    @Autowired
    private MockMvc mockMvc;

    @Test
    @Order(1)
    @DisplayName("create vertex1")
    @JsonSerialize
    void vertex1CreateTest() throws Exception {

        JSONObject jo = new JSONObject();
        jo.put("vLabel", "test");
        jo.put("properties", new JSONObject().put("testkey", "testvalue"));

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .post("/api/create/vertex")
                .content(jo.toString())
                .contentType("application/json");

        vertex1 = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());
    }

    @Test
    @Order(2)
    @DisplayName("create vertex2")
    void vertex2CreateTest() throws Exception {
        JSONObject jo = new JSONObject();
        jo.put("vLabel", "test");
        jo.put("properties", new JSONObject().put("testkey", "testvalue"));

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .post("/api/create/vertex")
                .content(jo.toString())
                .contentType("application/json");

        vertex2 = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());
    }

    @Test
    @Order(3)
    @DisplayName("read vertex2")
    void vertexReadTest() throws Exception {

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .get("/api/read/vertex/{graphId}", vertex2.getString("graphId"))
                .contentType("application/json");

        JSONObject jo = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());

        assertThat(jo.get("properties").toString(), is(equalTo(new JSONObject().put("testkey", "testvalue").toString())));
    }

    @Test
    @Order(4)
    @DisplayName("update vertex2")
    void vertexUpdateTest() throws Exception {

        JSONObject jo = new JSONObject();
        jo.put("properties", new JSONObject().put("testkey", "updatevalue"));

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .put("/api/update/vertex/{graphId}", vertex2.getString("graphId"))
                .content(jo.toString())
                .contentType("application/json");
        this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn();
    }

    @Test
    @Order(5)
    @DisplayName("read vertex2(after update)")
    void vertexReadAfterUpdateTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .get("/api/read/vertex/{graphId}", vertex2.getString("graphId"))
                .contentType("application/json");
        JSONObject jo = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString().trim());
        assertThat(jo.get("properties").toString(), is(equalTo(new JSONObject().put("testkey", "updatevalue").toString())));
    }

    @Test
    @Order(6)
    @DisplayName("create edge")
    void edge1CreateTest() throws Exception {

        JSONObject jo = new JSONObject();
        jo.put("eLabel", "test");
        jo.put("source", vertex1.get("graphId"));
        jo.put("target", vertex2.get("graphId"));
        jo.put("properties", new JSONObject().put("testkey", "testvalue"));

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .post("/api/create/edge")
                .content(jo.toString())
                .contentType("application/json");

        edge1 = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());

    }

    @Test
    @Order(7)
    @DisplayName("read edge")
    void edgeReadTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .get("/api/read/edge/{graphId}", edge1.getString("graphId"))
                .contentType("application/json");
        JSONObject jo = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());
        assertThat(jo.get("properties").toString(), is(equalTo(new JSONObject().put("testkey", "testvalue").toString())));
    }

    @Test
    @Order(8)
    @DisplayName("update edge")
    void edgeUpdateTest() throws Exception {

        JSONObject updateJo = new JSONObject();
        updateJo.put("properties", new JSONObject().put("testkey", "updatevalue"));

        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .put("/api/update/edge/{graphId}", edge1.getString("graphId"))
                .content(updateJo.toString())
                .contentType("application/json");

        JSONObject jo = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());

        assertThat(jo.get("properties").toString(), is(equalTo(new JSONObject().put("testkey", "updatevalue").toString())));
    }

    @Test
    @Order(9)
    @DisplayName("read edge (after update)")
    void edgeReadAfterupdateTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .get("/api/read/edge/{graphId}", edge1.getString("graphId"))
                .contentType("application/json");
        JSONObject jo = new JSONObject(this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn().getResponse().getContentAsString());
        assertThat(jo.get("properties").toString(), is(equalTo(new JSONObject().put("testkey", "updatevalue").toString())));
    }

    @Test
    @Order(10)
    @DisplayName("delete edge")
    void edgeDeleteTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .delete("/api/delete/edge/{graphId}", edge1.getString("graphId"))
                .contentType("application/json");
        this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn();
    }

    @Test
    @Order(11)
    @DisplayName("delete vertex1")
    void vertex1DeleteTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .delete("/api/delete/vertex/{graphId}", vertex1.getString("graphId"))
                .contentType("application/json");
        this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn();
    }

    @Test
    @Order(12)
    @DisplayName("delete vertex2")
    void vertex2DeleteTest() throws Exception {
        MockHttpServletRequestBuilder requestBuilder = MockMvcRequestBuilders
                .delete("/api/delete/vertex/{graphId}", vertex2.getString("graphId"))
                .contentType("application/json");
        this.mockMvc.perform(requestBuilder)
                .andExpect(status().isOk())
                .andDo(print())
                .andReturn();
    }

    @Test
    @Order(13)
    @DisplayName("test Finished")
    void testFinished() {
    }

}
