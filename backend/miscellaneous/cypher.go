package miscellaneous

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"strings"
)

type Vertex struct {
	ID         int64                  `json:"id"`
	Label      string                 `json:"label"`
	Properties map[string]interface{} `json:"properties"`
}

type Edge struct {
	ID         int64                  `json:"id"`
	Label      string                 `json:"label"`
	EndID      int64                  `json:"end_id"`
	StartID    int64                  `json:"start_id"`
	Properties map[string]interface{} `json:"properties"`
}

type Row struct {
	V  Vertex `json:"v"`
	R  Edge   `json:"r"`
	V2 Vertex `json:"v2"`
}

func CypherCall(db *sql.DB, q map[string]string, c chan<- ChannelResults) {
	data := ChannelResults{}
	var results []map[string]interface{}
	rows, err := db.Query(q["query"])
	if err != nil {
		c <- data
		return
	}
	cols, _ := rows.ColumnTypes()
	if len(cols) == 1 && cols[0].DatabaseTypeName() == "VOID" {
		data.Msg = map[string]string{
			"status": "success",
		}
		c <- data
		return
	}
	columns := make([]string, len(cols))
	for i, col := range cols {
		columns[i] = col.Name()
	}
	data.Columns = columns
	data.Command = "SELECT"

	for rows.Next() {
		rawResult := make([]interface{}, len(cols))
		result := make(map[string]interface{})
		for i := 0; i < len(cols); i++ {
			rawResult[i] = new(string)
		}
		err := rows.Scan(rawResult...)
		if err != nil {
			data.Err = err
			c <- data
			return
		}
		for i, raw := range rawResult {
			str := *raw.(*string)

			// split the string at "::" and only keep the first part (the JSON)
			parts := strings.SplitN(str, "::", 2)
			jsonStr := parts[0]

			var val interface{}
			err := json.Unmarshal([]byte(jsonStr), &val)
			if err != nil {
				fmt.Println("Failed to unmarshal value:", err)
				c <- data
				return
			}

			result[columns[i]] = val
		}

		results = append(results, result)
	}
	data.Rows = results
	data.RowCount = len(results)
	c <- data
}