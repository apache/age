package net.bitnine.ageapi.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import io.swagger.annotations.ApiModelProperty;
import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.NoArgsConstructor;
import lombok.Setter;
import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;
import org.springframework.boot.jackson.JsonComponent;


@Setter
@Getter
@JsonComponent
@NoArgsConstructor
@AllArgsConstructor
public class UpdateRequestDto {
    @JsonProperty
    @ApiModelProperty(position = 1, dataType = "application/json", example = "JSON_PROPERTY_HERE")
    private JSONObject properties;

    @Override
    public String toString() {
        return properties.toString();
    }

}
