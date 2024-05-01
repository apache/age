<p align="center">
     <img src="https://age.apache.org/age-manual/master/_static/logo.png" width="30%" height="30%">
<br>


</br>
<img src="https://img.shields.io/badge/Viewer-in%20GO-green"" height="25" height="30% alt="Apache AGE"/>
</p>


# Apache AGE-Viewer Desktop App 

## apache-age-viewer-go-1.0.0


## Introduction : 
Visualize graph data generated using Apache AGE with Apache AGE Viewer application.


**Age-Viewer-Go** Desktop application is a graphical user interface (**GUI**) for managing and developing **Apache-AGE databases**. It provides a unified and integrated experience for working with Apache-age databases, making it easier for developers and administrators to perform common tasks. 

<h2 align = 'center'> <u>Setting Up the Development Environment </u></h2>

### <u>Golang Installation</u>
### Installing and Setting up Golang in Windows

#### Step 1: Download Golang

The first step is to download Golang from the official website. Go to [https://golang.org/dl/](https://golang.org/dl/) and download the latest version of Golang for Windows.

#### Step 2: Install Golang

Once the download is complete, run the installer and follow the on-screen instructions to install Golang.
During the installation, you will be prompted to choose the installation directory. 
> We recommend using the default directory.`

- Check the golang version installed by : `go version`


#### Step 3: Set up Golang Environment Variables

After installing Golang, you need to set up the environment variables.
First Setup A Workspace Folder :
Create a new directory 
> `C:\Projects\Go`

**It can be changed to any folder of your choice.**
And then create 3 new folders inside Go Folder Created above : 
1. bin
2. pkg
3. src

Now, set up the environment variables as follows:
1. Right-click on the `This PC` icon on your desktop and select `Properties`.
2. Click on `Advanced system settings` on the left-hand side.
3. Click on the `Environment Variables` button.
4. Under `User Variables`, click on "New".
5. Enter `GOPATH` as the variable name and the path to the Golang Workspace. For example, if you Created Golang Workspace directory as mentioned above, the **variable value** should be `"C:\Projects\Go"`.
6. Click on "OK" to save the variable.
7. Under `User Variables` click on `Path`>`New` and enter the path to bin folder created above. 
For example, if you Created Golang Workspace directory as mentioned above, the **variable value** should be `C:\Projects\Go\bin`.
#### Step 4: Verify GOPATH Variable

On Command Prompt:
- To ensure that your path has been successfully specified, enter `echo %GOPATH%` . 

On Windows PowerShell
- To ensure that your path has been successfully specified, enter `$env:GOPATH` 

The Output should be : 
> C:\Projects\Go

### Installing Wails
####Dependencies

Wails has a number of common dependencies that are required before installation:

    Go 1.18+
    NPM (Node 15+)
- Run `go install github.com/wailsapp/wails/v2/cmd/wails@latest`to install the Wails CLI.
####System Check
Running `wails doctor` will check if you have the correct dependencies installed. If not, it will advise on what is missing and help on how to rectify any problems.

### <u>Building the Project</u>

- Go in the `C:\Projects\Go\src` clone the AGE repository 
`git clone https://github.com/apache/age`


- cd into `age` and Switch to ageviewer_go branch `git checkout ageviewer_go`
- cd into `frontend` and run `npm i`
- After installation of node modules, in the project root run
> wails dev

### <u>For Backend Testing and Development</u>

####To run the backend:

- cd to the `backend` folder.
- type `go run`. This will start the backend server.
- Open another terminal and run `node APIfunctions.js` this will provide you cmd menu to interact with the backend server.
<br>
- For the backend, uncomment code for the menu at the end of the file and also uncomment some require() lines at the start of the APIfunctions.js file.
>NOTE: It's Still in Development, may be changed in later release.

> To test the connectivity and working a running postgreSQL Server with AGE is required

#### Setting up the PostgreSQL server with AGE extension
-  Easiest way  for Windows, Mac-OS and Linux Environment using **Docker**
  
	> Install docker in advance (https://www.docker.com/get-started), install the version compatible with your OS from the provided link.
	
	 **Run Using Docker** :
   
	- Get the docker image - 
	```docker pull apache/age ```
	
	- Create AGE docker container
	```bash
	docker run --name myPostgresDb -p 5455:5432 -e POSTGRES_USER=postgresUser \
	-e POSTGRES_PASSWORD=postgresPW -e POSTGRES_DB=postgresDB -d apache/age
	```
	
	| Docker variables| Description |
	|--|--|
	| ``--name`` | Assign a name to the container |
	|	`-p` |	Publish a container’s port(s) to the host|
	|	``-e``|	Set environment variables|
	|	``-d``|	Run container in background and print container ID|
- To Get the running log of the docker container created - 
`` docker logs --follow myPostgresDb``
- To Get into postgreSQL Shell (There are two ways this can be done) -
	- First get into docker shell using -	`` docker exec -it myPostgresDb bash`` 
	<br>Then get into postgreSQL shell using - `` psql -U postgresUser postgresDB``
	
	OR
	
	- Alternatively postgres shell can also be assessed directly (without getting into the docker shell) -
		`` psql -U postgresUser -d postgresDB -p 5455 -h localhost``
		and put in ``postgresPW`` when prompted for password.
- After logging into postgreSQL shell follow the [Post-Installation](https://github.com/apache/age#post-installation) instruction to create a graph in the database.


## Features

Some of the key features and functionalities of Ageviewer_go Desktop will include:

- <b>Starting Application:</b> Starting window, where it checks system requirements,runtime environment,distributions and other processing that is in happening in our current Age-Viewer in cmd while it's running.
     
- <b>Authorization:</b> Home screen should be prompt after successful checks of start window (having no conflicting error then can enter to home screen).Home screen should have features like:
     <ol>
       <li>List of projects</li>
       <li> Example project(for demo purpose)</li>
       <li>DBMS</li>
       <li>Graph Apps</li>
       <li>Developer tools etc.</li>
     </ol>
  
- <b>Graph Data Visualization:</b> Ageviewer_go Desktop will provide a visual graph data browser for exploring and querying the data stored in your apache-age databases.
  
- <b>Project Management:</b> The application will allow you to manage multiple projects, each with its own set of databases and associated configuration settings.
 
- <b>Database Management:</b> Ageviewer_go will provide a simple and intuitive interface for managing apache-age databases, including starting and stopping the database, backing up and restoring the database, and managing users and permissions.
 
- <b>Query Editing:</b> Ageviewer_go desktop application will include a query editor with syntax highlighting and autocompletion, making it easier to write and test Cypher queries.
     
- <b>Frontend: </b> Responsive for all the screen resolution,correct scaling and alignment of buttons and tables and should use appropriate colours.
   
- <b>Query Translation: </b> Entered query translated to give appropriate results.
     
- <b> Identification of SQL/Postgres: </b> Classification of SQL and Cypher must be present so we can clearly translate cyphers.
    
- <b>Schema Management:</b> It will also provide a visual schema editor for creating and managing indexes, constraints, and labels in your apache-age databases.
     
   <b><i>Overall cycle work as : Data gathered from Db,Query entered,Query Translation, Output in Graph format(In Frontened)</b></i>


<h2>Connect to database</h2>
<img src="https://user-images.githubusercontent.com/67288224/217324853-2755019a-bb3a-435d-8eb5-c48fc18df9ce.png"/>
<h2>Create a query and analyze graph data</h2>
<img src="https://user-images.githubusercontent.com/67288224/217334417-ff6e51ce-de51-46d5-bf32-098974967e33.gif"/>
