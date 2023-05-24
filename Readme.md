<br>
<p align="center">
    <img src="https://age.apache.org/age-manual/master/_static/logo.png" width="30%" height="30%"><br>
    <img src="https://img.shields.io/badge/Viewer-in%20GO-green" height="25" alt="Apache AGE">
</p>
<br>

# Apache AGE-Viewer in Golang

<h3 align="center">
    <a href="https://age.apache.org/age-manual/master/_static/logo.png" target="_blank">
        <img src="https://age.apache.org/age-manual/master/_static/logo.png" height="25" alt="Apache AGE">
    </a>
</h3>

<p>
Visualize graph data generated using Apache AGE with Apache AGE Viewer application.

**Ageviewer_go** Desktop application is a graphical user interface (**GUI**) for managing and developing **Apache-age databases**. It provides a unified and integrated experience for working with Apache-age databases, making it easier for developers and administrators to perform common tasks. Some of the key features and functionalities of Ageviewer_go Desktop will include:

- **Starting Application:** Starting window, where it checks system requirements, runtime environment, distributions, and other processing that is happening in our current Age-Viewer in cmd while it's running.
     
- **Authorization:** Home screen should be prompt after successful checks of the start window (having no conflicting errors, then can enter the home screen). Home screen should have features like:
  1. List of projects
  2. Example project (for demo purposes)
  3. DBMS
  4. Graph Apps
  5. Developer tools, etc.
  
- **Graph Data Visualization:** Ageviewer_go Desktop will provide a visual graph data browser for exploring and querying the data stored in your Apache-age databases.
  
- **Project Management:** The application will allow you to manage multiple projects, each with its own set of databases and associated configuration settings.
 
- **Database Management:** Ageviewer_go will provide a simple and intuitive interface for managing Apache-age databases, including starting and stopping the database, backing up and restoring the database, and managing users and permissions.
 
- **Query Editing:** Ageviewer_go desktop application will include a query editor with syntax highlighting and autocompletion, making it easier to write and test Cypher queries.
     
- **Frontend:** Responsive for all screen resolutions, correct scaling and alignment of buttons and tables, and should use appropriate colors.
   
- **Query Translation:** Entered query translated to give appropriate results.
     
- **Identification of SQL/Postgres:** Classification of SQL and Cypher must be present so we can clearly translate Cyphers.
    
- **Schema Management:** It will also provide a visual schema editor for creating and managing indexes, constraints, and labels in your Apache-age databases.
     
   **Overall cycle works as: Data gathered from Db, Query entered, Query Translation, Output in Graph format (In Frontend)**

</p>

<h2>Connect to Database</h2>
<img src="https://user-images.githubusercontent.com/67288224/217324853-2755019a-bb3a-435d-8eb5-c48fc18df9ce.png" alt="Connect to database">

<h2>Create a Query and Analyze Graph Data</h2>
<img src="https://user-images.githubusercontent.com/67288224/217334417-ff6e51ce-de51-46d5-bf32-098974967e33.gif" alt="Create a query and analyze graph data">

<h1>How to Run Backend and Frontend of Ageviewer Desktop</h1>

<h2>For macOS</h2>
 

     
***Download Go***
```bash

curl -O https://dl.google.com/go/go1.18.6.darwin-amd64.pkg
```
***Install Go***
```bash
sudo installer -pkg go1.18.6.darwin-amd64.pkg -target /
```
***Check version***
```bash
go version
```

***Download Wails***
```bash
go install github.com/wailsapp/wails/v2/cmd/wails@latest

```
***Exporting Go Binary Directory to the PATH Environment Variable***
```bash
   nano ~/.profile
```

<p>Copy these commands and paste them in the end</p>

```bash
export PATH=$PATH:$(go env GOPATH)/bin
```
<p>Save the file by pressing Ctrl+O, then exit the text editor by pressing control+X</p>

```bash
  source ~/.profile
```
***Check version***
```bash
 wails version
```

<h2>For Ubuntu</h2>


***Download Go***

```bash

wget https://dl.google.com/go/go1.18.6.linux-amd64.tar.gz

```
***Install Go***
```bash
tar -xvf go1.18.6.linux-amd64.tar.gz
sudo mv go /usr/local

```
***Set Environment*** 

```bash
   nano ~/.profile
```

<p>Copy these commands and paste them in the end</p>


```bash 

export PATH=$PATH:/usr/local/go/bin
```

```bash
export GOPATH=$HOME/go
```

<p>Save the file by pressing Ctrl+O, then exit the text editor by pressing control+X</p>

```bash
  source ~/.profile
```

***Check version***
```bash
  go version
```
***Download Wails***
```bash
go install github.com/wailsapp/wails/v2/cmd/wails@latest

```
***Exporting Go Binary Directory to the PATH Environment Variable***
```bash
   nano ~/.profile
```

<p>Copy these commands and paste them in the end</p>

```bash
export PATH=$PATH:$(go env GOPATH)/bin
```
<p>Save the file by pressing Ctrl+O, then exit the text editor by pressing CTRL+X</p>

```bash
  source ~/.profile
```
***Check version***

```bash
 wails version
```

<h4>Backend</h4>

<h4>Run using Docker
</h4>

<h5> Get the docker image </h5>

```bash
docker pull apache/age

```
<h5> Create AGE docker container </h5>

```bash
docker run \
    --name age  \
    -p 5455:5432 \
    -e POSTGRES_USER=postgresUser \
    -e POSTGRES_PASSWORD=postgresPW \
    -e POSTGRES_DB=postgresDB \
    -d \
    apache/age
```

<h6>Move to backend dir</h6>

```bash
cd backend
```
***Run backend***

```bash
go run .
```
    
***Postmaster***
    <h6>Generate a New request </h6>
       
       
```bash
    http://localhost:8080/connect
```    

    
    
<h6>Using Postman to Add a Raw JSON Body to a New Request</h6>

<ol>
  <li>Open Postman and create a new request by selecting the appropriate HTTP method (e.g., GET, POST, PUT, DELETE) and providing the request URL.</li>
  <li>In the request builder, click on the "Body" tab below the request URL.</li>
  <li>In the "Body" section, choose the "raw" option.</li>
  <li>From the dropdown menu next to "raw," select "JSON (application/json)" as the format for the raw text.</li>
  <li>Now, you can enter your JSON data directly in the text editor area below. Make sure it adheres to the JSON format with proper syntax (e.g., key-value pairs enclosed in curly braces, strings enclosed in double quotes).</li>
  <li>Once you have entered your JSON data, click the "Send" button to send the request with the specified JSON body.</li>
</ol>
<h6>Add the following json code in body</h6>
            
 ```bash
 {
    "dbname": "postgresDB",
    "graph_init":false,
    "host":"localhost",
    "password":"postgresPW",
    "port":"5455",
    "ssl":"disable",
    "user":"postgresUser",
    "version":11

}
```
<h6>To test the Cypher queries, you can follow these steps:</h6>
<ol>
    <li>Start the local server </li>
   

  <li> Send a POST request to retrieve the metadata of the graph.</li>
  <li>In the request body, provide the JSON payload with the following structure</li>
  
  
</ol>

```bash
http://localhost:8080/query/metadata
```    

    
```bash
    
    {
      "query": "SELECT * FROM cypher('demo_graph', $$ MATCH (v) RETURN v $$) as (v agtype);"
    }
```
<h4>Frontend</h4>
<h6>Go to age-viewer dir</h6>

```bash
cd age-viewer/frontend
```
***Run Frontend***
```bash 
wails init
```
