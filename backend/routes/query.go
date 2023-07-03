package routes

import (
	"age-viewer-go/db"
	m "age-viewer-go/miscellaneous"
	"age-viewer-go/models"
	"database/sql"
	"fmt"
	"strconv"

	"github.com/gorilla/sessions"
	"github.com/labstack/echo"
)

/*
CypherMiddleWare retrieves the user's database connection information from a session,
establishes a connection to the database, and sets the connection and user information
in the Echo context for subsequent request handling. It also initializes the graph extension and sets the search path.
*/
func CypherMiddleWare(next echo.HandlerFunc) echo.HandlerFunc {
	return func(c echo.Context) error {
		userDb := c.Get("database")

		if userDb == nil {
			return echo.NewHTTPError(403, "user not initialized")
		}

		user := userDb.(*sessions.Session).Values["db"].(models.Connection)

		conn, err := user.GetConnection("disable")

		if err != nil {
			return echo.NewHTTPError(400, err.Error())
		}
		if err = conn.Ping(); err != nil {
			return echo.NewHTTPError(400, err.Error())
		}
		if !user.GraphInit {
			_, err = conn.Exec(db.INIT_EXTENSION)
			if err != nil {
				msg := fmt.Sprintf("could not communicate with db\nerror: %s", err.Error())
				return echo.NewHTTPError(400, msg)
			}
			user.GraphInit = true
		}
		if user.Version == 0 {
			rows, err := conn.Query(db.PG_VERSION)
			if err != nil {
				fmt.Print(err.Error())
			}
			var version string
			for rows.Next() {
				rows.Scan(&version)
			}

			val, _ := strconv.Atoi(version[:2])
			user.Version = val
		}
		conn.Exec(db.SET_SEARCH_PATH)
		c.Set("conn", conn)
		c.Set("user", user)
		return next(c)

	}
}

/*
Cypher function processes Cypher queries in a context object, The function extracts
the query from the context object, executes it on a database connection, and returns
the results as a JSON response. It also handles errors that may arise during the query execution.
*/
func Cypher(ctx echo.Context) error {
	q := map[string]string{}
	var err error

	err = ctx.Bind(&q)
	if err != nil {
		e := m.ErrorHandle{
			Msg: "unable to parse query",
			Err: err,
		}
		return ctx.JSON(400, e.JSON())
	}
	conn := ctx.Get("conn").(*sql.DB)
	c := make(chan m.ChannelResults, 1)
	m.CypherCall(conn, q, c)
	data := <-c
	err = data.Err
	res := data
	msg := data.Msg
	if err != nil {
		return echo.NewHTTPError(400, fmt.Sprintf("unable to process query. error: %s", err.Error()))
	}

	if _, ok := msg["status"]; ok {
		return ctx.JSON(204, msg)
	}

	return ctx.JSON(200, res)
}

/*
queries the database for graph metadata, such as labels, count, kind, and namespace, and organizes this information
into a structured format, separating nodes and edges for each unique namespace. The function then returns this metadata
as a JSON object.
*/
func GraphMetaData(c echo.Context) error {
	// Create channels to handle data and errors
	dataChan := make(chan *sql.Rows, 1)
	errChan := make(chan error, 1)

	// Retrieve user and connection information from the context
	user := c.Get("user").(models.Connection)
	conn := c.Get("conn").(*sql.DB)

	// Analyze the database for faster processing
	conn.Exec(db.ANALYZE)

	// Run GetMetaData in a goroutine
	models.GetMetaData(conn, user.Version, dataChan, errChan)

	// Wait for the data and error results
	data, err := <-dataChan, <-errChan

	// Check if there is any error
	if err != nil {
		return echo.NewHTTPError(400, err.Error())
	}

	// Initialize a new MetaDataContainer with an empty map for Graphs
	results := models.MetaDataContainer{
		Graphs: make(map[string]models.GraphData),
	}

	// Iterate through the data rows
	for data.Next() {
		row := models.MetaData{}

		// Scan the row data into the MetaData struct
		err := data.Scan(&row.Label, &row.Cnt, &row.Kind, &row.NameSpace, &row.Namespace_id, &row.Graph, &row.Relation)
		if err != nil {
			print(err.Error())
		}

		// Check for empty namespace (no graphs) and return an error
		if row.NameSpace == "" {
			return echo.NewHTTPError(400, "No Graph exists for the database")
		}

		// Initialize GraphData if it doesn't exist for the current namespace
		if _, exists := results.Graphs[row.NameSpace]; !exists {
			results.Graphs[row.NameSpace] = models.GraphData{}
		}

		// Get the current GraphData for the namespace
		graphData := results.Graphs[row.NameSpace]

		// Append the row data to Nodes or Edges based on the Kind
		if row.Kind == "v" {
			graphData.Nodes = append(graphData.Nodes, row)
		} else {
			graphData.Edges = append(graphData.Edges, row)
		}

		// Update the GraphData in the results map
		results.Graphs[row.NameSpace] = graphData
	}

	// Return the final results as JSON

	return c.JSON(200, results)
}
