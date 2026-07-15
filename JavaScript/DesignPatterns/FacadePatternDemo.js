// Subsystem 1
class TV {
    on() {
        console.log("TV is ON");
    }

    setInput() {
        console.log("TV input set to HDMI");
    }
}

// Subsystem 2
class SoundSystem {
    on() {
        console.log("Sound System is ON");
    }

    setVolume(volume) {
        console.log(`Volume set to ${volume}`);
    }
}

// Subsystem 3
class StreamingDevice {
    on() {
        console.log("Streaming Device is ON");
    }

    playMovie(movie) {
        console.log(`Playing movie: ${movie}`);
    }
}

// Facade
class HomeTheaterFacade {
    constructor() {
        this.tv = new TV();
        this.soundSystem = new SoundSystem();
        this.streamingDevice = new StreamingDevice();
    }

    watchMovie(movie) {
        console.log("Preparing Home Theater...\n");

        this.tv.on();
        this.tv.setInput();

        this.soundSystem.on();
        this.soundSystem.setVolume(20);

        this.streamingDevice.on();
        this.streamingDevice.playMovie(movie);

        console.log("\nEnjoy your movie!");
    }
}

const homeTheater = new HomeTheaterFacade();
homeTheater.watchMovie("Interstellar");
