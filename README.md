# What is this
I wanted to learn more about C++ coroutines through a practical case. My goal was to be able to use coroutines to make gRPC calls, and have them interact with a frame based application.

# What's in here
## async_lib
The core of the design is as follows:
- Coroutines are in the form of Tasks that are bound to an executor
- Tasks are unstarted and unbound util it eithers:
-- Gets spawned on an executor through async_lib::Spawn
-- Gets co awaited from with a task that has been spawned on an executor

Tasks gets turned into Jobs, those are how an executor can interact with the task.

Users need to implement their own Executors (like the ones in async_grpc and game), they have the following responsibilities:
- When a job begins, by implementing a Spawn method on their executor class
- How a job gets suspended, by implementing custom awaitable types
- When a job gets resumed, by calling async_lib::Resume

Resuming jobs is a greedy operation, it continues executing as much of the job call stack until either a suspend point is reached or the root job (the one spawned on the executor) is finished

From within a task, you can co_await a StartSubroutine to spawn a Task that will be executed in parallel with the current task, on the same executor. The task calling StartSubroutine is responsible for making sure the task completes by co_await'ing it.

It is possible to co_await the result of a task that is made for another type of executor. You can achieve that by co_awaiting a SpawnCrossTask. This will suspend the current task until the cross task is done.

## async_grpc
A gRPC implementation of coroutines, relying on completion queues.

In the async_grpc lib is the implementation of the CompletionQueueExecutor. Within it:
- A job begins **immediatly**, meaning that the task begins as part of the async_lib::Spawn call.
- Jobs get suspended when it enqueues something on the executor's completion queue, the tag is the address of the Job.
- Once popped from the completion queue, the 'ok' flag gets injected in the Job's associated promise before it gets resumed.

The server and client contains example server and client implementation respectively, the protobuf files are included in the protos folder.

## game
A "game" implementation, basically a frame based program showing interactions with coroutines, even ones coming from other kind of executors like a gRPC one.

async_game defines an executor with the following properties:
- A job doesn't begin when it's spawned, it begins on the next call to the executor's Update func. This guarantees that the coroutine is only executed from within the thread updating the executor.
- Jobs only get suspended if they are awaiting on a task that does get suspended, like a gRPC task co_awaited by calling async_lib::SpawnCrossTask.
- To resume a job, you need to call Executor::MarkReady. The job will then be resumed on the next call to Executor::Update.

In that implementation of a "game", executors are bound to an entity and are garanteed to be updated on the same thread that the entity gets updated on. This removes all kind of need for thread safety concern around the use of those coroutines.