package miscellaneous

type ErrorHandle struct {
	Err error
	Msg string
}

func (e *ErrorHandle) Error() string {
	return e.Err.Error()
}
func (e *ErrorHandle) JSON() map[string]string {
	return map[string]string{
		"error":   e.Err.Error(),
		"message": e.Msg,
	}
}
