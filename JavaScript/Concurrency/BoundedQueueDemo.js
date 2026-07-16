'use strict';

class BoundedQueue {
    constructor(capacity) {
        this.queue = [];
        this.capacity = capacity;
        this.waiters = [];
    }

    async put(item, name) {
        while (this.queue.length === this.capacity) {  // full — wait for a taker
            console.log(`${name} sees FULL (${this.queue.length}/${this.capacity}), waiting...`);
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        this.queue.push(item);
        console.log(`${name} put: ${item} [size=${this.queue.length}]`);
        this._notifyAll();  // wake possible waiting takers (and putters)
    }

    async take(name) {
        while (this.queue.length === 0) {  // empty — wait for a putter
            console.log(`${name} sees EMPTY, waiting...`);
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        const item = this.queue.shift();
        console.log(`${name} took: ${item} [size=${this.queue.length}]`);
        this._notifyAll();
        return item;
    }

    _notifyAll() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function producer(queue, name) {
    for (let i = 0; i < 5; i++) {
        await queue.put(`${name}-item${i}`, name);
        await sleep(30);
    }
}

async function consumer(queue, name) {
    for (let i = 0; i < 5; i++) {
        await queue.take(name);
        await sleep(60);
    }
}

async function main() {
    const queue = new BoundedQueue(3);  // capacity 3, not 1

    await Promise.all([
        producer(queue, 'producer-1'),
        producer(queue, 'producer-2'),
        consumer(queue, 'consumer-1'),
        consumer(queue, 'consumer-2'),
    ]);
}

main();
