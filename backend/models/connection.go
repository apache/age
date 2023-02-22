package models

import (
	"database/sql"
	"fmt"
	"strconv"

	"github.com/labstack/echo"
)

type Connection struct {
	Port     string
	Host     string
	Password string
	User     string
	DbName   string
}

func FromRequestBody(context echo.Context) (*Connection, error) {
	user := new(Connection)
	e := context.Bind(&user)
	return user, e
}

func (c *Connection) ToSQLString(secure ...bool) string {
	ssl := "false"
	if len(secure) > 0 {
		ssl = strconv.FormatBool(secure[0])
	}

	str := fmt.Sprintf(
		"user=%s port=%s host=%s password=%s sslmode=%s dbname=%s",
		c.User,
		c.Port,
		c.Host,
		c.Password,
		ssl,
		c.DbName,
	)

	return str
}

func (c Connection) GetConnection(secure ...bool) (*sql.DB, error) {
	ssl := false
	if len(secure) > 0 {
		ssl = secure[0]
	}
	return sql.Open("postgres", c.ToSQLString(ssl))
}
