'use strict';

const { Worker, isMainThread, workerData, parentPort } = require('worker_threads');

const TOTAL_INCREMENTS = 1000;
const NUM_WORKERS = 8;

function runWorker() {
    const view = new Int32Array(workerData.sharedBuffer);
    for (let i = 0; i < workerData.increments; i++) {
        view[0] = view[0] + 1;   // NOT atomic — read-modify-write race
        Atomics.add(view, 1, 1); // atomic
    }
    parentPort.postMessage('done');
}

async function main() {
    // JS has no shared-memory mutable ints without SharedArrayBuffer/worker_threads,
    // so real OS threads are used here to reproduce the same race Java shows.
    const sharedBuffer = new SharedArrayBuffer(2 * Int32Array.BYTES_PER_ELEMENT);
    const view = new Int32Array(sharedBuffer);

    const baseIncrements = Math.floor(TOTAL_INCREMENTS / NUM_WORKERS);
    const remainder = TOTAL_INCREMENTS % NUM_WORKERS;

    const workers = [];
    for (let i = 0; i < NUM_WORKERS; i++) {
        const increments = baseIncrements + (i < remainder ? 1 : 0);
        workers.push(
            new Promise((resolve) => {
                const worker = new Worker(__filename, {
                    workerData: { sharedBuffer, increments },
                });
                worker.on('message', resolve);
            })
        );
    }

    await Promise.all(workers);

    console.log(`unsafe: ${view[0]}`); // almost never 1000
    console.log(`safe:   ${view[1]}`); // always 1000
}

if (isMainThread) {
    main();
} else {
    runWorker();
}
