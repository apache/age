package net.bitnine.ageapi.api.dto;


import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.annotation.JsonPropertyOrder;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import io.swagger.annotations.ApiModelProperty;
import lombok.AccessLevel;
import lombok.Builder;
import lombok.Getter;
import lombok.Setter;

import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;
import org.springframework.boot.jackson.JsonComponent;


@Setter
@Getter
@JsonComponent
@JsonPropertyOrder(value = {"vLabel", "properties"})
public class CreateVertexRequestDto {

    @ApiModelProperty(position = 0, example = "VERTEX_LABELNAME_HERE")
    @JsonProperty("vLabel")
    private String vLabel;

    @ApiModelProperty(position = 1, dataType = "application/json", example = "JSON_PROPERTY_HERE")
    private JSONObject properties;
}
