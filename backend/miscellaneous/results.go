package miscellaneous

type ChannelResults struct {
	Rows     []map[string]interface{} `json:"rows"`
	Columns  []string                 `json:"columns"`
	Err      error                    `json:"error,omitempty"`
	Msg      map[string]string        `json:"message,omitempty"`
	RowCount int                      `json:"rowCount"`
	Command  string                   `json:"command"`
}
