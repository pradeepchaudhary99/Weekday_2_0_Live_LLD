package main

import "fmt"

type Request struct {
	priority int
}

type Handler interface {
	SetNextHandler(handler Handler) Handler
	GetNextHandler() Handler
	IsAllowed(request Request) bool
	ProcessRequest(request Request)
}

type BaseHandler struct {
	nextHandler Handler
}

func (h *BaseHandler) SetNextHandler(handler Handler) Handler {
	h.nextHandler = handler
	return h.nextHandler
}

func (h *BaseHandler) GetNextHandler() Handler {
	return h.nextHandler
}

type Level1Handler struct {
	BaseHandler
}

func (h *Level1Handler) IsAllowed(request Request) bool {
	return request.priority < 8
}

func (h *Level1Handler) ProcessRequest(request Request) {
	if h.IsAllowed(request) {
		fmt.Println("Processing the request : lvel1 is processing")
	} else {
		h.GetNextHandler().ProcessRequest(request)
	}
}

type Level2Handler struct {
	BaseHandler
}

func (h *Level2Handler) IsAllowed(request Request) bool {
	return request.priority < 15
}

func (h *Level2Handler) ProcessRequest(request Request) {
	if h.IsAllowed(request) {
		fmt.Println("Processing the request : lvel1 is processing")
	} else {
		h.GetNextHandler().ProcessRequest(request)
	}
}

type Level3Handler struct {
	BaseHandler
}

func (h *Level3Handler) IsAllowed(request Request) bool {
	return request.priority < 2
}

func (h *Level3Handler) ProcessRequest(request Request) {
	if h.IsAllowed(request) {
		fmt.Println("Processing the request : lvel1 is processing")
	} else {
		h.GetNextHandler().ProcessRequest(request)
	}
}

func main() {
	level1 := &Level1Handler{}
	level2 := &Level2Handler{}
	level3 := &Level3Handler{}

	level1.SetNextHandler(level2).SetNextHandler(level3)
}
