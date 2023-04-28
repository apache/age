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
	res := data.Res
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
retrieves graph metadata for a given graph, and returns it as JSON. It uses the graph object to determine
which graph to query, and the user object to determine the database version. The retrieved metadata is
then organized into two lists: one for nodes and one for edges, before being returned as a JSON response.
*/
func GraphMetaData(c echo.Context) error {
	dataChan := make(chan *sql.Rows, 1)
	errChan := make(chan error, 1)

	user := c.Get("user").(models.Connection)
	conn := c.Get("conn").(*sql.DB)
	graph := models.Graph{}
	c.Bind(&graph)
	conn.Exec(db.ANALYZE)
	go graph.GetMetaData(conn, user.Version, dataChan, errChan)
	data, err := <-dataChan, <-errChan

	if err != nil {
		return echo.NewHTTPError(400, err.Error())
	}
	results := models.MetaDataContainer{}

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
