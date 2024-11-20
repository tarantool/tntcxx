# Tntcxx Client Design

## Scope

This document describes the design of the Tntcxx Client. First we state the 
requirements and use cases we have in mind, and then we propose a design that 
fulfills them.

## Requirements

### Functional Requirements

We envision Tntcxx to be used primarily as a part of network applications (e.g.,
an HTTP server), the Tntcxx Client backing requests to Tarantool. Such 
applications are usually built around a central event processing loop, which is 
responsible for multiplexing network I/O.

#### Application Event Processing Loop Integration

The first and foremost requirement is that there must be a convenient 
way to integrate the Tntcxx Client into the event processing loop used by the 
application. Moreover, the Tntcxx Client must never run the event processing 
loop itself.

At the same time, since event processing loops are inherently single threaded, 
we do expect the Tntcxx Client to be used in a multithreaded environment, i.e., 
when connections and reqeust futures are in different threads. So we do not aim
to design the Tntcxx Client to be thread-safe.

#### Asynchronous Request Processing

Since the Tntcxx Client is constrained from running the event processing loop, 
the Tntcxx Client must support asynchronous request processing through 
application-provided callbacks or futures.  

#### Connection State

The application must be able to check the state of a Tntcxx Client.

#### Connection Error Handling

There must be a convenient way for the application to handle errors arising
throughout the Tntcxx Client lifecycle. A connection error must be returned 
through the request callback and through the request object.

#### Request Handling

In order for the application to be able to manage a request, a request object is 
always returned. The application can check the request status, cancel the 
request, handle request errors and retrieve the response through this handle 
(only once). However, if the response was retrieved by other means (either 
returned through a callback or collected through scatter-gather), the handle
cannot return the response.

#### Request Status

The application must be able to check the status of a Tntcxx Client request.

#### Request Timeout

Since the Tntcxx Client does not have control over the application's event 
processing loop, the application must implement its own request timeouts. The 
application can cancel stale requests.

#### Request Cancelling

The application must be able to cancel a Tntcxx Client request. Cancelling a 
request explicitly ends the lifetime of the corresponding response.

#### Request Retrying

Since the Tntcxx Client does not have control over the application's event 
processing loop, the application must implement its own request retrying.

#### Request Fan-Out

A common Tntcxx Client use case is fan-out to multiple Tarantool instances, and 
collection of responses received after some deadline, and discarding of requests
that are not ready by the deadline.

#### Response Lifetime

The response lifetime is managed implicitly through the lifetime of the request.
The response is not copyable. It can be retrieved only once, and the response 
ownership is moved to the application.

#### Reconnection

The Tntcxx Client must support implicit reconnection with the same session 
settings it was created with.

#### Connection Pool

TBD.

#### Transactions

TBD.

#### Failover

TBD.

## Design

### I/O Event Providers

```c++
/** Callback called on a read event. */ 
using read_ready_cb_f = void (*)(int fd, Data *data);
/** Callback called on a write event. */
using write_ready_cb_f = void (*)(int fd, Data *data); 

/**
 * An I/O event provider encapsulates the notification about events for a 
 * collection of file descriptors.
 * 
 * `Data` is an opaque context type passed to the notification callbacks. 
 */
template<class Data *>
class IOEventProvider {
public:
    /**
     * Register a file descriptor. Returns 0 on success, -1 on failure.
     */
	int register(int fd, Data *data, read_ready_cb_f read_ready_cb, 
		         write_ready_cb_f write_ready_cb);
	
	/**
	 * Unregister a file descriptor. Returns 0 on success, -1 on failure.
	 */
	int unregister(int fd);
};
```

#### Epoll

```c++
/**
 * Since `epoll` does not have a facility for storing callbacks, we delegate 
 * calling the notification callbacks to the application. 
 * 
 * In order to distinguish between application file descriptors and tntcxx 
 * sockets, we provide a wrapper class around the `Data`, which should be passed 
 * as the `ptr` argument of `epoll_data` to `epoll` by the application for its 
 * own file descriptors.
 */
template<class Data *>
class EpollIOEventProviderData {
public:
    EpollIOEventProviderData(int type, Data *data);
    
    /** 
     * Type of file descriptor, needed for distinguishing application file 
     * descriptors from tntcxx sockets. 
     */
    int type;
    Data *data;
};

/**
 * Encapsulates calling of notification callbacks from epoll for tntcxx. 
 */
template<class Data *>
class EpollIoEventProviderDataTntcxx : public EpollIOEventProviderData<Data *> {
public:
    EpollIoEventProviderData(int fd, Data *data, read_ready_cb_f read_ready_cb, write_ready_cb_f write_ready_cb);
    
    /** Needs to be called by the application on a read event. */
    void read_ready();
    /** Needs to be called by the application on a write event. */
    void write_ready();
};
```

### Connections

```c++
/**
 * A connection encapsulates sending requests and receiving responses from one 
 * Tarantool instance.
 */
template<class IOEventProvider>
class Connection {
public:
    Connection(IOEventProvider &net_provider, 
	           const ConnectionOptions &connection_options);
	
    /** 
	 * Return the connection's state.
	 */
	enum ConnectionState get_state() const;
	
	/** 
	 * If the connection is an erroneous state (see `status`), return the 
	 * connection error that caused it. The same error is also passed to 
	 * response callbacks, and the same error is returned by 
	 * `Request::get_error`. 
	 */
	std::optional<ConnectionError> &get_error() & const;
	
	/* An abstract request's interface. A request object is always returned. */
	Request some_request(/* options */);
};
```

#### Connection State

```c++
enum ConnectionState {
	CONNECTION_INITIAL = 0,
	CONNECTION_AUTH = 1,
	CONNECTION_ACTIVE = 2,
	CONNECTION_ERROR = 3,
	CONNECTION_ERROR_RECONNECT = 4,
};
```

#### Connect Options

```c++
/* Extend `ConnectionOptions` with an option for the reconnection feature. */
struct ConnectOptions {
	/* All existing options. */
	
	/** 
	 * In the event of a broken connection, the interval in which the stream 
	 * tries to re-establish the connection. 
	 */
	static constexpr size_t DEFAULT_RECONNECTION_INTERVAL = 2;
	size_t reconnection_interval = DEFAULT_RECONNECTION_INTERVAL;
};
```

### Requests

```c++
/** Encapsulates management of a request issued through a connection. */
class Request {
public:
	/** 
	 * Callback called when response is ready. Since the callback has a fixed 
	 * signature, we need to allow for capturing additional context using 
	 * lambdas. Hence, we use `std::function` for type erasure.
	 * 
	 * See `Connection::get_error` for details about the `error` parameter.
	 */ 
	using request_cb_f = 
		std::function<void(Request &&request, Connection &connection)>;
	
	/** Set a callback called when the response is ready. */
	void set_callback(request_cb_f request_cb) &&;
	
	/** Return the request's status. */
	enum RequestStatus get_status();
	
	/**
	 * Return a connection error, if any. See `Connection::get_error` for 
	 * details.
	 */
	std::optional<ConnectionError> &get_error() & const;
	
	/**
	 * Cancel the request, ending the lifetime of the corresponding response.
	 */
	void cancel();
	
	/** 
	 * Return the response, if any. A response is available, iff:
	 * 1. The response has been received by the connection.
	 * 2. The response has not been dispatched to a callback.
	 * 3. The response has not already been retrieved previously.
	 */
	std::optional<Response> get_response();
};
```

#### Request Status

```c++
enum RequestStatus {
	REQUEST_SUCCESS = 0,
	REQUEST_ERROR = -1,
	REQUEST_IN_PROGRESS = 1,
};
```

#### Request Fan-Out

```c++
/**
 * A fan-out encapsulates the collection of responses for a collection of 
 * requests.
 */
class FanOut {
	template<class InputIt>
	FanOut(MoveIt first, MoveIt last)
	
	/**
	 * Return a list of ready request, and cancel the requests for which a 
	 * response is not available. 
	 */
	tnt::List<Request> collect();
};
```
