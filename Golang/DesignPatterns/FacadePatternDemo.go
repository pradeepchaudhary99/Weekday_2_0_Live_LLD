package main

import "fmt"

// Subsystem 1
type TV struct{}

func (t *TV) on() {
	fmt.Println("TV is ON")
}

func (t *TV) setInput() {
	fmt.Println("TV input set to HDMI")
}

// Subsystem 2
type SoundSystem struct{}

func (s *SoundSystem) on() {
	fmt.Println("Sound System is ON")
}

func (s *SoundSystem) setVolume(volume int) {
	fmt.Printf("Volume set to %d\n", volume)
}

// Subsystem 3
type StreamingDevice struct{}

func (d *StreamingDevice) on() {
	fmt.Println("Streaming Device is ON")
}

func (d *StreamingDevice) playMovie(movie string) {
	fmt.Printf("Playing movie: %s\n", movie)
}

// Facade
type HomeTheaterFacade struct {
	tv              *TV
	soundSystem     *SoundSystem
	streamingDevice *StreamingDevice
}

func NewHomeTheaterFacade() *HomeTheaterFacade {
	return &HomeTheaterFacade{
		tv:              &TV{},
		soundSystem:     &SoundSystem{},
		streamingDevice: &StreamingDevice{},
	}
}

func (h *HomeTheaterFacade) watchMovie(movie string) {
	fmt.Println("Preparing Home Theater...\n")

	h.tv.on()
	h.tv.setInput()

	h.soundSystem.on()
	h.soundSystem.setVolume(20)

	h.streamingDevice.on()
	h.streamingDevice.playMovie(movie)

	fmt.Println("\nEnjoy your movie!")
}

func main() {
	homeTheater := NewHomeTheaterFacade()
	homeTheater.watchMovie("Interstellar")
}
