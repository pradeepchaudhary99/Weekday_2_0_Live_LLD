'use strict';

class BoundedBuffer {
    constructor(capacity) {
        this.queue = [];
        this.capacity = capacity;
        this.waiters = [];
    }

    async put(item) {
        while (this.queue.length === this.capacity) {
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        this.queue.push(item);
        this._notifyAll();
    }

    async take() {
        while (this.queue.length === 0) {
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        const item = this.queue.shift();
        this._notifyAll();
        return item;
    }

    _notifyAll() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }
}

async function producer(buffer) {
    for (let i = 0; i < 10; i++) {
        await buffer.put(i);
        console.log(`Produced: ${i}`);
    }
}

async function consumer(buffer) {
    for (let i = 0; i < 10; i++) {
        const item = await buffer.take();
        console.log(`Consumed: ${item}`);
    }
}

async function main() {
    const buffer = new BoundedBuffer(5);
    await Promise.all([producer(buffer), consumer(buffer)]);
}

main();
