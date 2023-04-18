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
		if err = conn.Ping(); err != nil { // check if the connection is still valid
			return echo.NewHTTPError(400, err.Error())
		}
		if !user.GraphInit {
			_, err = conn.Exec(db.INIT_EXTENSION) // initialize the AGE extension, if it is not already
			if err != nil {
				msg := fmt.Sprintf("could not communicate with db\nerror: %s", err.Error())
				return echo.NewHTTPError(400, msg)
			}
			user.GraphInit = true // set the flag to true, so that the extension is not initialized again
		}
		if user.Version == 0 {
			rows, err := conn.Query(db.PG_VERSION) // get the version of the database, if Version in User connection struct is 0.
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
	results := []interface{}{}
	var err error

	err = ctx.Bind(&q) // extract the query from the request body
	if err != nil {
		e := m.ErrorHandle{
			Msg: "unable to parse query",
			Err: err,
		}
		return ctx.JSON(400, e.JSON())
	}
	conn := ctx.Get("conn").(*sql.DB)   // get the database connection from the context object
	rows, err := conn.Query(q["query"]) // execute the query on the database

	if err != nil {
		return echo.NewHTTPError(400, fmt.Sprintf("unable to process query. error: %s", err.Error()))
	}

	cols, _ := rows.ColumnTypes()
	if len(cols) == 1 && cols[0].DatabaseTypeName() == "VOID" { // if the query is a DDL query(Data Definition Language), return a 204 status code
		return ctx.JSON(204, map[string]string{
			"status": "success",
		})
	}
	for rows.Next() { // iterate over the rows returned by the query
		data := []any{}
		err := rows.Scan(&data)
		if err != nil {
			print(fmt.Sprintf("\n\nerror: %s", err.Error()))
		}
		results = append(results, data)
	}
	return ctx.JSON(200, results)
}

/*
retrieves graph metadata for a given graph, and returns it as JSON. It uses the graph object to determine
which graph to query, and the user object to determine the database version. The retrieved metadata is
then organized into two lists: one for nodes and one for edges, before being returned as a JSON response.
*/
func GraphMetaData(c echo.Context) error {

	user := c.Get("user").(models.Connection) // get the user object from the context object
	conn := c.Get("conn").(*sql.DB)
	graph := models.Graph{}
	c.Bind(&graph) // extract the graph object from the request body
	conn.Exec(db.ANALYZE)
	data, err := graph.GetMetaData(conn, user.Version) // get the metadata for the graph according to the version.

	if err != nil {
		return echo.NewHTTPError(400, err.Error())
	}
	results := models.MetaDataContainer{}

	// organize the metadata into two lists: one for nodes and one for edges
	for data.Next() {
		row := models.MetaData{}
		err := data.Scan(&row.Label, &row.Cnt, &row.Kind)
		if row.Kind == "v" {
			results.Nodes = append(results.Nodes, row)
		} else {
			results.Edges = append(results.Edges, row)
		}

		if err != nil {
			print(err.Error())
		}
	}
	return c.JSON(200, results)
}
