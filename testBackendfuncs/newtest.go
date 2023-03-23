package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/http/cookiejar"
)

type Connection struct {
	Port      string `json:"port"`
	Host      string `json:"host"`
	Password  string `json:"password"`
	User      string `json:"user"`
	DbName    string `json:"dbname"`
	SSL       string `json:"ssl"`
	GraphInit bool   `json:"graph_init"`
	Version   int    `json:"version"`
}

type Graph struct {
	name string `json:"name"`
}

func main() {
	// create a Connection struct
	conn := Connection{
		Port:      "5432",
		Host:      "localhost",
		Password:  "Welcome@1",
		User:      "kamleshk",
		DbName:    "testdb",
		SSL:       "disable",
		GraphInit: true,
		Version:   11,
	}

	// encode the Connection struct as JSON
	connBytes, err := json.Marshal(conn)
	if err != nil {
		panic(err)
	}

	// create a cookie jar to store cookies
	jar, err := cookiejar.New(nil)
	if err != nil {
		panic(err)
	}

	// create an http.Client with the cookie jar
	client := &http.Client{
		Jar: jar,
	}

	// send a POST request to http://localhost:8080/connect with the Connection struct as the body
	resp, err := client.Post("http://localhost:8080/connect", "application/json", bytes.NewBuffer(connBytes))
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()

	fmt.Println(resp.Status)

	// store the cookies from the response in the cookie jar
	cookies := resp.Cookies()
	// Read the response body
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		panic(err)
	}

	// Print the response body as a string
	fmt.Println(string(body))

	graph := Graph{
		name: "demo_graph",
	}

	// encode the data as JSON
	jsonData, err := json.Marshal(graph)
	if err != nil {
		panic(err)
	}

	// create a new POST request to http://localhost:8080/query with the payload as the body
	newreq, err := http.NewRequest("POST", "http://localhost:8080/query/metadata", bytes.NewBuffer(jsonData))

	if err != nil {
		panic(err)
	}

	newreq.Header.Set("Content-Type", "application/json")
	// attach the cookies to the request
	for _, cookie := range cookies {
		newreq.AddCookie(cookie)
	}

	// send the request using the same client
	resp, err = client.Do(newreq)
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()

	// print the response
	fmt.Println("2nd", resp.Status)

	// Read the response body
	body, err = ioutil.ReadAll(resp.Body)
	if err != nil {
		panic(err)
	}

	// Print the response body as a string
	fmt.Println(string(body))

	payload := map[string]string{
		"query": "SELECT * FROM cypher('demo_graph', $$ MATCH (v) RETURN v $$) as (v agtype);",
	}

	// encode the payload as JSON
	jsonPayload, err := json.Marshal(payload)
	if err != nil {
		panic(err)
	}

	// create a new POST request to http://localhost:8080/query with the payload as the body
	thirdrequ, err := http.NewRequest("POST", "http://localhost:8080/query", bytes.NewBuffer(jsonPayload))

	if err != nil {
		panic(err)
	}

	thirdrequ.Header.Set("Content-Type", "application/json")
	// attach the cookies to the request
	for _, cookie := range cookies {
		thirdrequ.AddCookie(cookie)
	}

	// send the request using the same client
	resp, err = client.Do(thirdrequ)
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()

	// print the response
	fmt.Println("3rd", resp.Status)

	// Read the response body
	body, err = ioutil.ReadAll(resp.Body)
	if err != nil {
		panic(err)
	}

	// Print the response body as a string
	fmt.Println(string(body))

}
