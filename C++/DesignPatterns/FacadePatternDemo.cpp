#include <iostream>
#include <string>

// Subsystem 1
class TV {
public:
    void on() {
        std::cout << "TV is ON" << std::endl;
    }

    void setInput() {
        std::cout << "TV input set to HDMI" << std::endl;
    }
};

// Subsystem 2
class SoundSystem {
public:
    void on() {
        std::cout << "Sound System is ON" << std::endl;
    }

    void setVolume(int volume) {
        std::cout << "Volume set to " << volume << std::endl;
    }
};

// Subsystem 3
class StreamingDevice {
public:
    void on() {
        std::cout << "Streaming Device is ON" << std::endl;
    }

    void playMovie(const std::string& movie) {
        std::cout << "Playing movie: " << movie << std::endl;
    }
};

// Facade
class HomeTheaterFacade {
private:
    TV tv;
    SoundSystem soundSystem;
    StreamingDevice streamingDevice;

public:
    void watchMovie(const std::string& movie) {
        std::cout << "Preparing Home Theater...\n" << std::endl;

        tv.on();
        tv.setInput();

        soundSystem.on();
        soundSystem.setVolume(20);

        streamingDevice.on();
        streamingDevice.playMovie(movie);

        std::cout << "\nEnjoy your movie!" << std::endl;
    }
};

int main() {
    HomeTheaterFacade homeTheater;
    homeTheater.watchMovie("Interstellar");
    return 0;
}
