/*

FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

*/

class Task {
  execute() {
    throw new Error("execute() must be implemented");
  }
}

class EmailTask extends Task {
  constructor(email = null) {
    super();
    this.email = email;
  }

  execute() {
  }
}

class PaymentTask extends Task {
  execute() {
  }
}

class ScheduledTask {
  constructor(id, task, executionTime, priority) {
    this.id = id;
    this.task = task;
    this.executionTime = executionTime;
    this.priority = priority;
  }
}

class TaskQueue {
  constructor() {
    this.queue = [];
  }

  add(task) {
    this.queue.push(task);
    this.queue.sort((a, b) => a.executionTime - b.executionTime || a.priority - b.priority);
  }

  peek() {
    return this.queue.length > 0 ? this.queue[0] : null;
  }
}

class TaskScheduler {
  constructor(taskQueue) {
    this.taskQueue = taskQueue;
  }

  schedule(task) {
    this.taskQueue.add(task);
  }
}

class Dispatcher {
  constructor(taskQueue) {
    this.taskQueue = taskQueue;
    this.isRunning = true;
  }

  run() {
    while (this.isRunning) {
      const task = this.taskQueue.peek();

      // what is the current time
      // delay =
    }
  }
}

class TaskSchedularDemo {
}

function main() {
}

main();
