package queryPackage

var query string = "select * from test_record"

// Using this function we can retrieve the query from Frontend.
func RetrieveQuery() string {

	return query
}
