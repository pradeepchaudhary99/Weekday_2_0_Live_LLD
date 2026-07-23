'use strict';

class ReadWriteLockDemo {
    constructor() {
        this.activeReaders = 0;
        this.activeWriter = false; // if only 1 writer is allowed, other int
        this.waitingWriters = 0;
        this.waiters = [];
    }

    async lockRead() {
        // BLOCK ANY read locks if there is already a write lock
        while (this.activeWriter || this.waitingWriters > 0) {
            await this._wait();
        }
        this.activeReaders++;
    }

    unlockRead() {
        this.activeReaders--;
        if (this.activeReaders === 0) {
            this._notifyAll();
        }
    }

    async lockWrite() {
        this.waitingWriters++; // 500
        try {
            while (this.activeWriter || this.activeReaders > 0) {
                await this._wait();
            }
        } finally {
            this.waitingWriters--;
        }
        this.activeWriter = true;
    }

    unlockWrite() {
        this.activeWriter = false;
        this._notifyAll();
    }

    _wait() {
        return new Promise((resolve) => this.waiters.push(resolve));
    }

    _notifyAll() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }
}

async function main() {
    const lock = new ReadWriteLockDemo();
    await lock.lockRead();
    lock.unlockRead();
    await lock.lockWrite();
    lock.unlockWrite();
}

main();
