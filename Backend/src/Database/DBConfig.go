package Connectionpackage

import (
	"fmt"
)

type DBConfig struct {
	Host     string
	Port     int
	Username string
	Password string
	Database string
	SSLMode  string
}

// We can extend following function to retrieve the DBConfig from frontend.
func ToURL() string {

	//Dummy struct, to be retrieved form UI in future
	c := &DBConfig{
		Host:     "localhost",
		Port:     5433,
		Username: "postgres",
		Password: "Welcome@1",
		Database: "postgres",
		SSLMode:  "disable",
	}

	// Build the connection string
	connectionString := fmt.Sprintf("postgres://%s:%s@%s:%d/%s", c.Username, c.Password, c.Host, c.Port, c.Database)

	// Add the SSL mode parameter, if provided
	if c.SSLMode != "" {
		connectionString += fmt.Sprintf("?sslmode=%s", c.SSLMode)
	}
	return connectionString

}
