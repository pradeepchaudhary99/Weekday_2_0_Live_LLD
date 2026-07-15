package main

import "fmt"

type Subscriber interface {
	NotifyMe(videoId string)
}

type Ananya struct{}

func (a *Ananya) NotifyMe(videoId string) {
	fmt.Printf("Ananaya: New Video uploaded%s\n", videoId)
}

type Amol struct{}

func (a *Amol) NotifyMe(videoId string) {
	fmt.Printf("Amol: New Video uploaded%s\n", videoId)
}

type YoutubeChannel struct {
	name        string
	subscribers []Subscriber
}

func (c *YoutubeChannel) addSubsriber(sub Subscriber) {
	c.subscribers = append(c.subscribers, sub)
}

func (c *YoutubeChannel) uploadVideo(videoId string) {
	// 10000 linee of code
	c.notifySubscriber(videoId)
}

func (c *YoutubeChannel) notifySubscriber(videoId string) {
	for _, subscriber := range c.subscribers {
		subscriber.NotifyMe(videoId)
	}
}

func main() {
	channel := &YoutubeChannel{}
	channel.addSubsriber(&Amol{})
	channel.addSubsriber(&Ananya{})

	channel.uploadVideo("how to avoid layoff in your company")
}
