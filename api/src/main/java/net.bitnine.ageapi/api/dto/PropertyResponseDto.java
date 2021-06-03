package net.bitnine.ageapi.api.dto;

import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.Setter;
import net.bitnine.agensgraph.deps.org.json.simple.JSONObject;

@Setter
@Getter
@AllArgsConstructor
public class PropertyResponseDto {
    private JSONObject properties;
}
