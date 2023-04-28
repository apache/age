// some pre-requisites to run these functions on desktop enviroment
//
import fetch from 'node-fetch';
import readline from 'readline';


const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

// Test data which will come from the frontend

const conn = {
  port: "5432",
  host: "localhost",
  password: "1234",
  user: "linux",
  dbname: "demodb",
  ssl: "disable",
  graph_init: true,
  version: 11,
};

// cookies will be stored here, these are store whenever connect() fun is called 
// and will be used in subsequent calls
let cookies;

function connect() {
  fetch('http://localhost:8080/connect', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(conn),
  })
  .then(response => {
    console.log(response.status);

    // Store the cookies from the response
    cookies = response.headers.raw()['set-cookie'];

    return response.json();
  })
  .then(data => {
    console.log(data);
  })
  .catch(error => {
    console.error(error);
  });
}

function queryMetadata() {
  const graph = {
    name: "demo_graph",
  };

  fetch('http://localhost:8080/query/metadata', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Cookie': cookies.join('; '),
    },
    body: JSON.stringify(graph),
  })
  .then(response => {
    console.log(response.status);

    return response.json();
  })
  .then(data => {
    console.log(data);
  })
  .catch(error => {
    console.error(error);
  });
}

function query() {
  const payload = {
    query: "SELECT * FROM cypher('demo_graph', $$ MATCH (v) RETURN v $$) as (v agtype);",
  };

  fetch('http://localhost:8080/query', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Cookie': cookies.join('; '),
    },
    body: JSON.stringify(payload),
  })
  .then(response => {
    console.log(response.status);

    return response.json();
  })
  .then(data => {
    console.log(data);
  })
  .catch(error => {
    console.error(error);
  });
}

function disconnect() {
  fetch('http://localhost:8080/disconnect', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Cookie': cookies.join('; '), // Send the cookies from the previous connection
    },
  })
  .then(response => {
    console.log(response.status);
    cookies = []; // Clear the cookies variable
    return response.json();
  })
  .then(data => {
    console.log(data);
  })
  .catch(error => {
    console.error(error);
  });
}


// dummy function to test the above functions

let choice;
function chooseFunction() {  

rl.question('Enter the function number: ', (input) => {
    const choice = parseInt(input);
    if (Number.isNaN(choice) || choice < 1 || choice > 4) {
      console.log('Invalid choice. Please try again.');
    } else {
      switch (choice) {
        case 1:
          connect();
          break;
        case 2:
          queryMetadata();
          break;
        case 3:
          query();
          break;
        case 4:
          disconnect();
          break;
      }
    }
    // rl.close();
    chooseFunction()
  });

}

chooseFunction();