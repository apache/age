import XCTest
@testable import age_swift

let host = "127.0.0.1";
let port = 5430 ;
let user = "fahadzaheer";
let db_name = "fahad";

final class age_swiftTests: XCTestCase {
    func testExample() throws {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        let age = Age()

        let connection_param:[String:Any] = ["host":host,"port":port, "user":user, "dbname":db_name]

        age.connect(connectionParam: connection_param, graph: "hello")
                
        let text = "SELECT * FROM weather;"
        
        //age.execSQL(statement: text)
    }
}
