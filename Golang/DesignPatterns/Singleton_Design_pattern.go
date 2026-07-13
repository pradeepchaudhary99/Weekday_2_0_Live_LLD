package main

import (
	"fmt"
	"sync"
)

type Logger struct {
	filePath string
}

var (
	instance *Logger
	once     sync.Once
)

func newLogger(filePath string) *Logger {
	fmt.Println("object Created")
	return &Logger{filePath: filePath}
}

func GetInstance() *Logger {
	fmt.Println("Thread")
	once.Do(func() {
		instance = newLogger("filePath")
	})
	return instance
}

func main() {
	logger := GetInstance()
	_ = logger

	task := func() {
		GetInstance()
	}

	var wg sync.WaitGroup
	wg.Add(3)
	for i := 0; i < 3; i++ {
		go func() {
			defer wg.Done()
			task()
		}()
	}
	wg.Wait()
}
