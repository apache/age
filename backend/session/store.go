package session

import (
	"os"

	"github.com/gorilla/sessions"
)

// Store is a global variable that holds the session store.
var Store = sessions.NewCookieStore([]byte(os.Getenv("GO_SECRET")))
