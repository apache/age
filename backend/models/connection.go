package models

import (
	"database/sql"
	"fmt"

	"github.com/labstack/echo"
	_ "github.com/lib/pq"
)

type Connection struct {
	Port      string
	Host      string
	Password  string
	User      string
	DbName    string
	SSL       string `default:"require"`
	GraphInit bool
	Version   int
}

func FromRequestBody(context echo.Context) (*Connection, error) {
	user := new(Connection)
	e := context.Bind(&user)
	return user, e
}

func (c *Connection) ToSQLString(secure ...string) string {
	if len(secure) > 0 {
		c.SSL = secure[0]
	}

	str := fmt.Sprintf(
		"user=%s port=%s host=%s password=%s sslmode=%s dbname=%s",
		c.User,
		c.Port,
		c.Host,
		c.Password,
		c.SSL,
		c.DbName,
	)

	return str
}

func (c Connection) GetConnection(secure ...string) (*sql.DB, error) {
	return sql.Open("postgres", c.ToSQLString(secure...))
}
