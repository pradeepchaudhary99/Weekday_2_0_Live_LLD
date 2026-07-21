'use strict';

const { Worker, isMainThread, workerData } = require('worker_threads');

function runWorker() {
    while (true) {
        console.log('Pradeep is continously working');
        console.log(`Pradeep Running this in a new Thread: ${workerData.name}`);
    }
}

function main() {
    // JS has no way to run two truly infinite, blocking loops in parallel on
    // the main thread (it's single-threaded), so real OS threads are used
    // here to reproduce Java's two concurrently-running Thread objects.
    new Worker(__filename, { workerData: { name: 'worker-1' } });
    new Worker(__filename, { workerData: { name: 'worker-2' } });

    // Alternative approaches considered:
    //
    // A fixed-size worker pool running a single task:
    //   const task1 = () => { for (let i = 0; i < 20; i++) console.log('pradeep'); };
    //
    // Three tasks each run on their own worker, then joined:
    //   const task1 = () => { for (let i = 0; i < 20; i++) console.log('pradeep'); };
    //   const task2 = () => { for (let i = 0; i < 20; i++) console.log('ishita'); };
    //   const task3 = () => { for (let i = 0; i < 20; i++) console.log('Sahil'); };
    //   await Promise.all([task1, task2, task3].map((t) => new Promise((resolve) => {
    //       const w = new Worker(...);
    //       w.on('exit', resolve);
    //   })));
    //   console.log('main thread');
}

if (isMainThread) {
    main();
} else {
    runWorker();
}
