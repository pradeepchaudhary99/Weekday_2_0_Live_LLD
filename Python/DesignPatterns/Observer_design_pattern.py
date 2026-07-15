from abc import ABC, abstractmethod


class Subscriber(ABC):
    @abstractmethod
    def notify_me(self, video_id: str) -> None:
        pass


class Ananya(Subscriber):
    def notify_me(self, video_id: str) -> None:
        print(f"Ananaya: New Video uploaded{video_id}")


class Amol(Subscriber):
    def notify_me(self, video_id: str) -> None:
        print(f"Amol: New Video uploaded{video_id}")


class YoutubeChannel:
    def __init__(self) -> None:
        self.name: str = ""
        self.subscribers: list[Subscriber] = []

    def add_subsriber(self, sub: Subscriber) -> None:
        self.subscribers.append(sub)

    def upload_video(self, video_id: str) -> None:
        # 10000 linee of code
        self.notify_subscriber(video_id)

    def notify_subscriber(self, video_id: str) -> None:
        for subscriber in self.subscribers:
            subscriber.notify_me(video_id)


if __name__ == "__main__":
    channel = YoutubeChannel()
    channel.add_subsriber(Amol())
    channel.add_subsriber(Ananya())

    channel.upload_video("how to avoid layoff in your company")
