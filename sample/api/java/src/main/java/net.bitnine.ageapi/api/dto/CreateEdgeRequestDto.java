package net.bitnine.ageapi.api.dto;


import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.annotation.JsonPropertyOrder;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import io.swagger.annotations.ApiModelProperty;
import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;
import org.springframework.boot.jackson.JsonComponent;

@Setter
@Getter
@JsonComponent
@JsonSerialize
@JsonPropertyOrder(value = {"eLabel", "source", "target", "properties"})
public class CreateEdgeRequestDto {

    @ApiModelProperty(example = "EDGE_LABELNAME_HERE")
    @JsonProperty("eLabel")
    private String eLabel;

    @ApiModelProperty(position = 1, example = "EDGE_STARTING_NODEID_HERE")
    private String source;

    @ApiModelProperty(position = 2, example = "EDGE_STARTING_NODEID_HERE")
    private String target;

    @ApiModelProperty(position = 3, example = "JSON_PROPERTY_HERE")
    private JSONObject properties;
}