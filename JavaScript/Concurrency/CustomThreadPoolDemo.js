'use strict';

class CustomThreadPool {
    constructor(numWorkers) {
        this.taskQueue = [];
        this.isShutdown = false;
        this.waiters = [];
        this.workers = [];
        for (let i = 0; i < numWorkers; i++) {
            this.workers.push(this._work(`Worker-${i}`));
        }
    }

    submit(task) {
        if (this.isShutdown) {
            throw new Error('ThreadPool is shut down, cannot accept new tasks');
        }
        this.taskQueue.push(task);
        this._wake();
    }

    shutdown() {
        this.isShutdown = true;
        this._wake();
    }

    async awaitTermination() {
        await Promise.all(this.workers);
    }

    _wake() {
        const waiters = this.waiters;
        this.waiters = [];
        waiters.forEach((resolve) => resolve());
    }

    async _work(name) {
        while (true) {
            while (this.taskQueue.length === 0 && !this.isShutdown) {
                await new Promise((resolve) => this.waiters.push(resolve));
            }
            if (this.taskQueue.length === 0 && this.isShutdown) {
                return;  // no more work, pool is shutting down
            }

            const task = this.taskQueue.shift();
            try {
                await task(name);
            } catch (e) {
                console.log(`${name} task threw exception: ${e.message}`);
            }
        }
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function main() {
    const pool = new CustomThreadPool(3);

    for (let i = 0; i < 10; i++) {
        pool.submit(async (name) => {
            console.log(`${name} executing task ${i}`);
            await sleep(200);
        });
    }

    pool.shutdown();
    await pool.awaitTermination();
    console.log('All tasks completed, pool terminated.');
}

main();
