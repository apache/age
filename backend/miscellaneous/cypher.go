package miscellaneous

import (
	"database/sql"
)

func CypherCall(db *sql.DB, q map[string]string, c chan<- ChannelResults) {
	data := ChannelResults{}
	results := []interface{}{}
	rows, err := db.Query(q["query"])
	if err != nil {
		data.Err = err
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
	rawData := make([]any, len(cols))
	for i := 0; i < len(cols); i++ {
		rawData[i] = new(string)
	}
	for rows.Next() {

		err := rows.Scan(rawData...)
		print(len(cols))
		if err != nil {
			data.Err = err
			break
		}
		results = append(results, rawData)
	}
	data.Res = results
	c <- data
}
