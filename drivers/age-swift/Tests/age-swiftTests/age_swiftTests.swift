import XCTest
@testable import age_swift

let host = "127.0.0.1";
let port = 5432 ;
let user = "root";
let db_name = "demo";

final class age_swiftTests: XCTestCase {
    func testExample() throws {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        let age = Age()

        let connection_param:[String:Any] = ["host":host,"port":port, "user":user, "dbname":db_name]

        age.connect(connectionParam: connection_param, graph: "hello")
                
        let text = "SELECT 1;"
        
        age.query(statement: text, connection: age.connection!)
    }
}
