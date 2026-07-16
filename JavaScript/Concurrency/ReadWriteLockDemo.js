'use strict';

class AsyncRWLock {
    constructor() {
        this.readers = 0;
        this.writerActive = false;
        this.waiters = [];
    }

    async acquireRead() {
        while (this.writerActive) {
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        this.readers++;
    }

    releaseRead() {
        this.readers--;
        if (this.readers === 0) this._wake();
    }

    async acquireWrite() {
        while (this.writerActive || this.readers > 0) {
            await new Promise((resolve) => this.waiters.push(resolve));
        }
        this.writerActive = true;
    }

    releaseWrite() {
        this.writerActive = false;
        this._wake();
    }

    _wake() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

class SharedCounter {
    constructor() {
        this.value = 0;
        this.lock = new AsyncRWLock();
    }

    async read(name) {
        await this.lock.acquireRead();
        try {
            console.log(`${name} reading: ${this.value}`);
            await sleep(100);  // simulate read taking some time
            return this.value;
        } finally {
            this.lock.releaseRead();
        }
    }

    async write(name, newValue) {
        await this.lock.acquireWrite();
        try {
            console.log(`${name} WRITING: ${newValue}`);
            await sleep(100);
            this.value = newValue;
        } finally {
            this.lock.releaseWrite();
        }
    }
}

async function main() {
    const counter = new SharedCounter();
    const tasks = [];

    // 4 readers — watch their timestamps overlap
    for (let i = 0; i < 4; i++) {
        tasks.push(counter.read(`reader-${i + 1}`));
    }

    await sleep(20);  // let readers start first

    // 1 writer — watch it wait for ALL readers to finish, then block everyone else
    tasks.push(counter.write('writer-1', 99));

    await Promise.all(tasks);
}

main();
