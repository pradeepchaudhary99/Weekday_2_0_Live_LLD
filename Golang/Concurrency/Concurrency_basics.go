package main

import (
	"fmt"
	"sync"
)

func pradeep(wg *sync.WaitGroup, name string) {
	defer wg.Done()
	for {
		fmt.Println("Pradeep is continously working")
		fmt.Printf("Pradeep Running this in a new Thread: %s\n", name)
	}
}

func main() {
	var wg sync.WaitGroup
	wg.Add(2)
	go pradeep(&wg, "goroutine-1")
	go pradeep(&wg, "goroutine-2")

	// Alternative approaches considered:
	//
	// A fixed-size worker pool running a single task:
	//   task1 := func() {
	//       for i := 0; i < 20; i++ {
	//           fmt.Println("pradeep")
	//       }
	//   }
	//
	// Three tasks each run on their own goroutine, then joined:
	//   task1 := func() { for i := 0; i < 20; i++ { fmt.Println("pradeep") } }
	//   task2 := func() { for i := 0; i < 20; i++ { fmt.Println("ishita") } }
	//   task3 := func() { for i := 0; i < 20; i++ { fmt.Println("Sahil") } }
	//   var innerWg sync.WaitGroup
	//   for _, task := range []func(){task1, task2, task3} {
	//       innerWg.Add(1)
	//       go func(t func()) { defer innerWg.Done(); t() }(task)
	//   }
	//   innerWg.Wait()
	//   fmt.Println("main thread")

	wg.Wait()
}
