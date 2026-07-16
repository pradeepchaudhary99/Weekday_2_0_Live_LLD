from abc import ABC, abstractmethod
from typing import Optional


class Request:
    def __init__(self, priority: int = 0) -> None:
        self.priority = priority


class IHandler(ABC):
    def __init__(self) -> None:
        self._next_handler: Optional["IHandler"] = None

    def set_next_handler(self, handler: "IHandler") -> "IHandler":
        self._next_handler = handler
        return self._next_handler

    def get_next_handler(self) -> Optional["IHandler"]:
        return self._next_handler

    @abstractmethod
    def is_allowed(self, request: Request) -> bool:
        ...

    @abstractmethod
    def process_request(self, request: Request) -> None:
        ...


class Level1Handler(IHandler):
    def is_allowed(self, request: Request) -> bool:
        return request.priority < 8

    def process_request(self, request: Request) -> None:
        if self.is_allowed(request):
            print("Processing the request : lvel1 is processing")
        else:
            self.get_next_handler().process_request(request)


class Level2Handler(IHandler):
    def is_allowed(self, request: Request) -> bool:
        return request.priority < 15

    def process_request(self, request: Request) -> None:
        if self.is_allowed(request):
            print("Processing the request : lvel1 is processing")
        else:
            self.get_next_handler().process_request(request)


class Level3Handler(IHandler):
    def is_allowed(self, request: Request) -> bool:
        return request.priority < 2

    def process_request(self, request: Request) -> None:
        if self.is_allowed(request):
            print("Processing the request : lvel1 is processing")
        else:
            self.get_next_handler().process_request(request)


def main() -> None:
    level1 = Level1Handler()
    level2 = Level2Handler()
    level3 = Level3Handler()

    level1.set_next_handler(level2).set_next_handler(level3)


if __name__ == "__main__":
    main()
