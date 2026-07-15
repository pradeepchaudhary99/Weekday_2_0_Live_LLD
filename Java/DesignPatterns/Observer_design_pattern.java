// package DesignPatterns;

import java.util.ArrayList;
import java.util.List;

interface Subscriber{
    void notifyMe(String videoId);
}

class Ananya implements Subscriber{
    @Override
    public void notifyMe(String videoId) {
        System.out.println("Ananaya: New Video uploaded"+videoId);
    }
}

class Amol implements Subscriber{
    @Override
    public void notifyMe(String videoId) {
        System.out.println("Amol: New Video uploaded"+videoId);

    }
}


class YoutubeChannel{
    String name;
    List<Subscriber> subscribers;
    public YoutubeChannel(){
        subscribers = new ArrayList<>();
    }
    void addSubsriber(Subscriber sub){
        subscribers.add(sub);
    }

    void uploadVideo(String videoId){
        //10000 linee of code
    notifySubscriber(videoId);
    }
    void notifySubscriber(String videoId){
        for(Subscriber subscriber : subscribers){
            subscriber.notifyMe(videoId);
        }
    }

}


public class Observer_design_pattern {
    public static void main(String[] args) {
        YoutubeChannel channel = new YoutubeChannel();
        channel.addSubsriber(new Amol());
        channel.addSubsriber(new Ananya());

        channel.uploadVideo("how to avoid layoff in your company");
    }
}
