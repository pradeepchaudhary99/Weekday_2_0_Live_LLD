'use strict';

class BoundedBuffer {
    constructor(capacity) {
        this.queue = [];
        this.capacity = capacity;
        this.producerWaiters = [];
        this.consumerWaiters = [];
    }

    async put(item) {
        while (this.queue.length === this.capacity) {
            await new Promise((resolve) => this.producerWaiters.push(resolve));
        }
        this.queue.push(item);
        this._notifyConsumer();
    }

    async take() {
        while (this.queue.length === 0) {
            await new Promise((resolve) => this.consumerWaiters.push(resolve));
        }
        const item = this.queue.shift();
        this._notifyProducer();
        return item;
    }

    _notifyConsumer() {
        const waiter = this.consumerWaiters.shift();
        if (waiter) waiter();
    }

    _notifyProducer() {
        const waiter = this.producerWaiters.shift();
        if (waiter) waiter();
    }
}

async function produce(buffer) {
    for (let i = 0; i < 10; i++) {
        await buffer.put(i);
        console.log(`Produced: ${i}`);
    }
}

async function consume(buffer) {
    for (let i = 0; i < 10; i++) {
        const item = await buffer.take();
        console.log(`Consumed: ${item}`);
    }
}

async function main() {
    const buffer = new BoundedBuffer(5);
    await Promise.all([produce(buffer), consume(buffer)]);
}

main();
