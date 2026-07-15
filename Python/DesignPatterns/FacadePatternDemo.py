# Subsystem 1
class TV:
    def on(self) -> None:
        print("TV is ON")

    def set_input(self) -> None:
        print("TV input set to HDMI")


# Subsystem 2
class SoundSystem:
    def on(self) -> None:
        print("Sound System is ON")

    def set_volume(self, volume: int) -> None:
        print(f"Volume set to {volume}")


# Subsystem 3
class StreamingDevice:
    def on(self) -> None:
        print("Streaming Device is ON")

    def play_movie(self, movie: str) -> None:
        print(f"Playing movie: {movie}")


# Facade
class HomeTheaterFacade:
    def __init__(self) -> None:
        self.tv = TV()
        self.sound_system = SoundSystem()
        self.streaming_device = StreamingDevice()

    def watch_movie(self, movie: str) -> None:
        print("Preparing Home Theater...\n")

        self.tv.on()
        self.tv.set_input()

        self.sound_system.on()
        self.sound_system.set_volume(20)

        self.streaming_device.on()
        self.streaming_device.play_movie(movie)

        print("\nEnjoy your movie!")


if __name__ == "__main__":
    home_theater = HomeTheaterFacade()
    home_theater.watch_movie("Interstellar")
