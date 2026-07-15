class Subscriber {
    notifyMe(videoId) {
        throw new Error("Not implemented");
    }
}

class Ananya extends Subscriber {
    notifyMe(videoId) {
        console.log(`Ananaya: New Video uploaded${videoId}`);
    }
}

class Amol extends Subscriber {
    notifyMe(videoId) {
        console.log(`Amol: New Video uploaded${videoId}`);
    }
}

class YoutubeChannel {
    constructor() {
        this.name = "";
        this.subscribers = [];
    }

    addSubsriber(sub) {
        this.subscribers.push(sub);
    }

    uploadVideo(videoId) {
        // 10000 linee of code
        this.notifySubscriber(videoId);
    }

    notifySubscriber(videoId) {
        for (const subscriber of this.subscribers) {
            subscriber.notifyMe(videoId);
        }
    }
}

const channel = new YoutubeChannel();
channel.addSubsriber(new Amol());
channel.addSubsriber(new Ananya());

channel.uploadVideo("how to avoid layoff in your company");
