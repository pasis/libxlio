/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause
 */

/**
 * @file xlio_zerocopy_api.h
 * @brief XLIO Socket API - High-Performance Zero-Copy Networking Interface
 * 
 * This file documents the XLIO Socket API (also known as Storage API), which is a
 * performance-oriented event-based networking interface designed for high-throughput,
 * low-latency TCP applications. The API provides zero-copy capabilities, event-driven
 * architecture, and direct hardware access for maximum performance.
 * 
 * @author NVIDIA Corporation
 * @version 1.0
 * @date 2024-2025
 */

#ifndef XLIO_ZEROCOPY_API_H
#define XLIO_ZEROCOPY_API_H

#include <stdint.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <mellanox/xlio_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup xlio_socket_api XLIO Socket API
 * @brief High-performance zero-copy networking interface for TCP applications
 * 
 * The XLIO Socket API is a performance-oriented, event-based networking interface
 * designed for applications requiring maximum throughput and minimal latency.
 * It provides zero-copy capabilities, direct hardware access, and efficient
 * memory management for high-performance TCP networking.
 * 
 * @section features Key Features
 * - Zero-copy receive and transmit operations
 * - Event-driven architecture with callbacks
 * - Direct hardware access via InfiniBand verbs
 * - Memory management with user-provided allocators
 * - TCP socket abstraction with non-blocking operations
 * - Polling groups for efficient event handling
 * - Support for both IPv4 and IPv6
 * 
 * @section architecture Architecture Overview
 * The API is built around three main concepts:
 * 1. **Polling Groups**: Event management and callback registration
 * 2. **Sockets**: TCP socket abstraction with zero-copy capabilities
 * 3. **Buffers**: Memory management for zero-copy operations
 * 
 * @section workflow Typical Workflow
 * 1. Initialize XLIO with xlio_init_ex()
 * 2. Create polling group with xlio_poll_group_create()
 * 3. Create socket with xlio_socket_create()
 * 4. Configure socket (bind, connect, listen)
 * 5. Poll for events with xlio_poll_group_poll()
 * 6. Handle events via registered callbacks
 * 7. Send/receive data using zero-copy operations
 * 8. Clean up resources
 * 
 * @section limitations Current Limitations
 * - TCP sockets only (no UDP support)
 * - IPv4 and IPv6 support
 * - Linux-specific implementation
 * - Requires compatible network hardware
 * - Limited bonding support
 * 
 * @{
 */

/**
 * @defgroup xlio_init Initialization and Cleanup
 * @brief Functions for initializing and cleaning up the XLIO Socket API
 * @{
 */

/**
 * @brief Initialize the XLIO Socket API
 * 
 * This function must be called before using any other XLIO Socket API functions.
 * It's a heavy operation that sets up the internal state, allocates resources,
 * and configures the system for high-performance networking.
 * 
 * @note This function is not thread-safe. However, subsequent serialized calls
 * will exit successfully without performing any action.
 * 
 * @param attr Initialization attributes structure
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - EINVAL: Invalid parameters
 * - ENOMEM: Insufficient memory
 * - ENODEV: No compatible network devices found
 * 
 * @par Example:
 * @code
 * struct xlio_init_attr attr = {
 *     .flags = 0,
 *     .memory_cb = memory_callback,
 *     .memory_alloc = custom_alloc,    // Optional
 *     .memory_free = custom_free       // Optional
 * };
 * 
 * if (xlio_init_ex(&attr) != 0) {
 *     perror("xlio_init_ex failed");
 *     return -1;
 * }
 * @endcode
 * 
 * @see xlio_exit()
 * @see xlio_init_attr
 * @since XLIO 1.0
 */
int xlio_init_ex(const struct xlio_init_attr *attr);

/**
 * @brief Clean up and exit XLIO Socket API
 * 
 * This function should be called when the application is finished with the
 * XLIO Socket API. It cleans up resources and restores the system state.
 * 
 * @return 0 on success, -1 on error
 * 
 * @note All sockets and polling groups should be destroyed before calling this function.
 * 
 * @see xlio_init_ex()
 * @since XLIO 1.0
 */
int xlio_exit(void);

/** @} */ // end of xlio_init group

/**
 * @defgroup xlio_poll_group Polling Groups
 * @brief Functions for managing polling groups and event handling
 * 
 * Polling groups are the core event management mechanism in the XLIO Socket API.
 * They allow applications to register event callbacks and efficiently poll for
 * network events across multiple sockets.
 * 
 * @{
 */

/**
 * @brief Create a new polling group
 * 
 * Creates a new polling group with the specified attributes. Event callbacks
 * are registered per group, allowing applications to implement different
 * handling logic for different types of connections.
 * 
 * @param attr Polling group attributes
 * @param group_out Pointer to store the created group handle
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - EINVAL: Invalid parameters (group_out is NULL, attr is NULL, or socket_event_cb is NULL)
 * - ENOMEM: Insufficient memory
 * 
 * @par Example:
 * @code
 * xlio_poll_group_t group;
 * struct xlio_poll_group_attr attr = {
 *     .flags = XLIO_GROUP_FLAG_SAFE,
 *     .socket_event_cb = socket_event_callback,
 *     .socket_comp_cb = completion_callback,
 *     .socket_rx_cb = receive_callback,
 *     .socket_accept_cb = accept_callback
 * };
 * 
 * if (xlio_poll_group_create(&attr, &group) != 0) {
 *     perror("Failed to create polling group");
 *     return -1;
 * }
 * @endcode
 * 
 * @note Groups are expected to be long-lived objects. Frequent creation/destruction
 * has a performance penalty.
 * 
 * @see xlio_poll_group_destroy()
 * @see xlio_poll_group_attr
 * @since XLIO 1.0
 */
int xlio_poll_group_create(const struct xlio_poll_group_attr *attr, xlio_poll_group_t *group_out);

/**
 * @brief Destroy a polling group
 * 
 * Destroys the specified polling group and frees associated resources.
 * All sockets associated with this group should be destroyed or detached
 * before calling this function.
 * 
 * @param group The polling group to destroy
 * @return 0 on success, -1 on error
 * 
 * @note This function will block until all associated sockets are properly closed.
 * 
 * @see xlio_poll_group_create()
 * @since XLIO 1.0
 */
int xlio_poll_group_destroy(xlio_poll_group_t group);

/**
 * @brief Update polling group attributes
 * 
 * Updates the attributes of an existing polling group. This allows changing
 * callback functions or flags without recreating the group.
 * 
 * @param group The polling group to update
 * @param attr New attributes for the group
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - EINVAL: Invalid parameters (attr is NULL or socket_event_cb is NULL)
 * 
 * @see xlio_poll_group_create()
 * @since XLIO 1.0
 */
int xlio_poll_group_update(xlio_poll_group_t group, const struct xlio_poll_group_attr *attr);

/**
 * @brief Poll for events on a polling group
 * 
 * This is the main event processing function. It polls hardware for events,
 * executes TCP timers, and invokes registered callbacks. Most network events
 * are processed from the context of this call.
 * 
 * @param group The polling group to poll
 * 
 * @note This function should be called regularly in the main event loop.
 * It's non-blocking and will return immediately if no events are available.
 * 
 * @par Example:
 * @code
 * while (!quit) {
 *     xlio_poll_group_poll(group);
 *     // Optional: add small delay or other processing
 * }
 * @endcode
 * 
 * @see xlio_poll_group_create()
 * @since XLIO 1.0
 */
void xlio_poll_group_poll(xlio_poll_group_t group);

/**
 * @brief Flush all dirty sockets in a polling group
 * 
 * For polling groups created with XLIO_GROUP_FLAG_DIRTY, this function
 * flushes all sockets that have pending data to send. This provides
 * batch flushing capabilities for improved performance.
 * 
 * @param group The polling group to flush
 * 
 * @note This function should only be used with groups that have the
 * XLIO_GROUP_FLAG_DIRTY flag set.
 * 
 * @see xlio_socket_flush()
 * @since XLIO 1.0
 */
void xlio_poll_group_flush(xlio_poll_group_t group);

/** @} */ // end of xlio_poll_group group

/**
 * @defgroup xlio_socket Socket Management
 * @brief Functions for creating and managing XLIO sockets
 * 
 * XLIO sockets are high-performance TCP socket abstractions that provide
 * zero-copy capabilities and direct hardware access. They are represented
 * by opaque handles rather than file descriptors.
 * 
 * @{
 */

/**
 * @brief Create a new XLIO socket
 * 
 * Creates a new XLIO socket with the specified attributes. The socket is
 * automatically associated with the specified polling group and configured
 * for high-performance operation.
 * 
 * @param attr Socket attributes
 * @param sock_out Pointer to store the created socket handle
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - EINVAL: Invalid parameters (sock_out is NULL, attr is NULL, group is invalid, 
 *           or domain is not AF_INET/AF_INET6)
 * - ENOMEM: Insufficient memory
 * - EMFILE: Too many open files
 * 
 * @par Example:
 * @code
 * xlio_socket_t sock;
 * struct xlio_socket_attr attr = {
 *     .flags = 0,
 *     .domain = AF_INET,
 *     .group = group,
 *     .userdata_sq = (uintptr_t)my_socket_context
 * };
 * 
 * if (xlio_socket_create(&attr, &sock) != 0) {
 *     perror("Failed to create socket");
 *     return -1;
 * }
 * @endcode
 * 
 * @see xlio_socket_destroy()
 * @see xlio_socket_attr
 * @since XLIO 1.0
 */
int xlio_socket_create(const struct xlio_socket_attr *attr, xlio_socket_t *sock_out);

/**
 * @brief Destroy an XLIO socket
 * 
 * Initiates the socket closing procedure. The process may be asynchronous,
 * and socket events may continue to arrive until the XLIO_SOCKET_EVENT_TERMINATED
 * event is received.
 * 
 * @param sock The socket to destroy
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - EINVAL: Invalid socket handle
 * 
 * @note Zero-copy completion events may still arrive after calling this function
 * until the TERMINATED event is received.
 * 
 * @par Example:
 * @code
 * // Initiate socket destruction
 * xlio_socket_destroy(sock);
 * 
 * // Continue polling until TERMINATED event
 * while (!socket_terminated) {
 *     xlio_poll_group_poll(group);
 * }
 * @endcode
 * 
 * @see xlio_socket_create()
 * @since XLIO 1.0
 */
int xlio_socket_destroy(xlio_socket_t sock);

/**
 * @brief Update socket attributes
 * 
 * Updates the flags and user data associated with a socket. This allows
 * changing socket behavior and context without recreating the socket.
 * 
 * @param sock The socket to update
 * @param flags New flags for the socket
 * @param userdata_sq New user data for the socket
 * @return 0 on success, -1 on error
 * 
 * @see xlio_socket_create()
 * @since XLIO 1.0
 */
int xlio_socket_update(xlio_socket_t sock, unsigned flags, uintptr_t userdata_sq);

/** @} */ // end of xlio_socket group

/**
 * @defgroup xlio_socket_ops Socket Operations
 * @brief Standard socket operations for XLIO sockets
 * @{
 */

/**
 * @brief Set socket options
 * 
 * Sets socket options, similar to the standard setsockopt() function.
 * Supports standard socket options as well as XLIO-specific options.
 * 
 * @param sock The socket to configure
 * @param level The protocol level (SOL_SOCKET, IPPROTO_TCP, etc.)
 * @param optname The option name
 * @param optval Pointer to the option value
 * @param optlen Length of the option value
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @see setsockopt(2)
 * @since XLIO 1.0
 */
int xlio_socket_setsockopt(xlio_socket_t sock, int level, int optname, const void *optval,
                           socklen_t optlen);

/**
 * @brief Get socket name
 * 
 * Retrieves the local address of the socket, similar to getsockname().
 * 
 * @param sock The socket to query
 * @param addr Buffer to store the address
 * @param addrlen Pointer to the address length
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @see getsockname(2)
 * @since XLIO 1.0
 */
int xlio_socket_getsockname(xlio_socket_t sock, struct sockaddr *addr, socklen_t *addrlen);

/**
 * @brief Get peer name
 * 
 * Retrieves the remote address of the socket, similar to getpeername().
 * 
 * @param sock The socket to query
 * @param addr Buffer to store the address
 * @param addrlen Pointer to the address length
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @see getpeername(2)
 * @since XLIO 1.0
 */
int xlio_socket_getpeername(xlio_socket_t sock, struct sockaddr *addr, socklen_t *addrlen);

/**
 * @brief Bind socket to address
 * 
 * Binds the socket to a local address, similar to bind().
 * 
 * @param sock The socket to bind
 * @param addr The address to bind to
 * @param addrlen Length of the address
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @see bind(2)
 * @since XLIO 1.0
 */
int xlio_socket_bind(xlio_socket_t sock, const struct sockaddr *addr, socklen_t addrlen);

/**
 * @brief Connect socket to remote address
 * 
 * Initiates a connection to a remote address. The operation is non-blocking,
 * and the connection status is reported via the socket event callback.
 * 
 * @param sock The socket to connect
 * @param to The remote address to connect to
 * @param tolen Length of the remote address
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @note This function returns immediately. Connection establishment is
 * indicated by the XLIO_SOCKET_EVENT_ESTABLISHED event.
 * 
 * @see connect(2)
 * @since XLIO 1.0
 */
int xlio_socket_connect(xlio_socket_t sock, const struct sockaddr *to, socklen_t tolen);

/**
 * @brief Listen for incoming connections
 * 
 * Configures the socket to listen for incoming connections. Requires that
 * the polling group has a socket_accept_cb callback registered.
 * 
 * @param sock The socket to configure for listening
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - ENOTCONN: No accept callback registered in the polling group
 * 
 * @note The socket must be bound before calling this function.
 * 
 * @see listen(2)
 * @since XLIO 1.0
 */
int xlio_socket_listen(xlio_socket_t sock);

/**
 * @brief Get InfiniBand protection domain
 * 
 * Returns the InfiniBand protection domain associated with the socket.
 * This can be used for registering memory regions for zero-copy operations.
 * 
 * @param sock The socket to query
 * @return Pointer to ibv_pd structure, or NULL on error
 * 
 * @note The returned pointer is valid for the lifetime of the socket.
 * 
 * @since XLIO 1.0
 */
struct ibv_pd *xlio_socket_get_pd(xlio_socket_t sock);

/**
 * @brief Detach socket from polling group
 * 
 * Removes the socket from its current polling group. The socket becomes
 * inactive and will not generate events until attached to another group.
 * 
 * @param sock The socket to detach
 * @return 0 on success, -1 on error
 * 
 * @see xlio_socket_attach_group()
 * @since XLIO 1.0
 */
int xlio_socket_detach_group(xlio_socket_t sock);

/**
 * @brief Attach socket to polling group
 * 
 * Attaches a previously detached socket to a polling group. The socket
 * will begin generating events according to the group's configuration.
 * 
 * @param sock The socket to attach
 * @param group The polling group to attach to
 * @return 0 on success, -1 on error
 * 
 * @see xlio_socket_detach_group()
 * @since XLIO 1.0
 */
int xlio_socket_attach_group(xlio_socket_t sock, xlio_poll_group_t group);

/** @} */ // end of xlio_socket_ops group

/**
 * @defgroup xlio_tx Transmit Operations
 * @brief High-performance data transmission functions
 * 
 * The XLIO Socket API provides efficient transmission capabilities with
 * zero-copy support and flexible batching options.
 * 
 * @section tx_properties TX Flow Properties
 * - Non-blocking operation
 * - No partial write support - accepts all data unless memory allocation fails
 * - Each send call expects a complete or partial PDU/message
 * - Zero-copy completion callbacks for memory management
 * - Inline send operations for small data
 * - Data aggregation with flush control
 * 
 * @{
 */

/**
 * @brief Send data on a socket
 * 
 * Sends data on the specified socket using zero-copy techniques when possible.
 * The operation is non-blocking and accepts all data unless memory allocation fails.
 * 
 * @param sock The socket to send data on
 * @param data Pointer to the data to send
 * @param len Length of the data
 * @param attr Send attributes controlling the operation
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Error Codes:
 * - ENOMEM: Insufficient memory (recoverable by retrying later)
 * - Other errors are generally not recoverable
 * 
 * @par Example:
 * @code
 * char buffer[1024] = "Hello, World!";
 * struct xlio_socket_send_attr attr = {
 *     .flags = XLIO_SOCKET_SEND_FLAG_FLUSH,
 *     .mkey = mr->lkey,
 *     .userdata_op = (uintptr_t)my_completion_context
 * };
 * 
 * if (xlio_socket_send(sock, buffer, strlen(buffer), &attr) != 0) {
 *     perror("Send failed");
 * }
 * @endcode
 * 
 * @note For zero-copy operation, the memory must be registered with the
 * InfiniBand protection domain obtained from xlio_socket_get_pd().
 * 
 * @see xlio_socket_sendv()
 * @see xlio_socket_send_attr
 * @since XLIO 1.0
 */
int xlio_socket_send(xlio_socket_t sock, const void *data, size_t len,
                     const struct xlio_socket_send_attr *attr);

/**
 * @brief Send vectored data on a socket
 * 
 * Sends data from multiple buffers (scatter-gather) on the specified socket.
 * This is more efficient than multiple send calls when data is fragmented.
 * 
 * @param sock The socket to send data on
 * @param iov Array of iovec structures describing the data buffers
 * @param iovcnt Number of iovec structures
 * @param attr Send attributes controlling the operation
 * @return 0 on success, -1 on error (errno is set)
 * 
 * @par Example:
 * @code
 * struct iovec iov[2];
 * iov[0].iov_base = header;
 * iov[0].iov_len = sizeof(header);
 * iov[1].iov_base = payload;
 * iov[1].iov_len = payload_len;
 * 
 * struct xlio_socket_send_attr attr = {
 *     .flags = XLIO_SOCKET_SEND_FLAG_INLINE,  // For small data
 *     .mkey = 0,  // Not needed for inline
 *     .userdata_op = 0  // No completion callback
 * };
 * 
 * if (xlio_socket_sendv(sock, iov, 2, &attr) != 0) {
 *     perror("Vectored send failed");
 * }
 * @endcode
 * 
 * @see xlio_socket_send()
 * @since XLIO 1.0
 */
int xlio_socket_sendv(xlio_socket_t sock, const struct iovec *iov, unsigned iovcnt,
                      const struct xlio_socket_send_attr *attr);

/**
 * @brief Flush pending data on a socket
 * 
 * Forces transmission of any data queued on the socket. XLIO may batch
 * small sends for efficiency, and this function ensures immediate transmission.
 * 
 * @param sock The socket to flush
 * 
 * @note Avoid using this function with sockets in XLIO_GROUP_FLAG_DIRTY groups.
 * Use xlio_poll_group_flush() instead for better performance.
 * 
 * @see xlio_poll_group_flush()
 * @since XLIO 1.0
 */
void xlio_socket_flush(xlio_socket_t sock);

/** @} */ // end of xlio_tx group

/**
 * @defgroup xlio_rx Receive Operations
 * @brief Zero-copy receive buffer management
 * 
 * The XLIO Socket API provides zero-copy receive capabilities through
 * a buffer management system. Received data is delivered via callbacks
 * with buffer descriptors that must be returned to the system.
 * 
 * @{
 */

/**
 * @brief Free a receive buffer (socket-specific)
 * 
 * Returns a receive buffer to the system for reuse. This function should
 * be called for every buffer received via the RX callback.
 * 
 * @param sock The socket that received the buffer (may be unused)
 * @param buf The buffer descriptor to free
 * 
 * @note The buffer must not be accessed after calling this function.
 * 
 * @par Example:
 * @code
 * void rx_callback(xlio_socket_t sock, uintptr_t userdata_sq, 
 *                  void *data, size_t len, struct xlio_buf *buf) {
 *     // Process the received data
 *     process_data(data, len);
 *     
 *     // Return the buffer to XLIO
 *     xlio_socket_buf_free(sock, buf);
 * }
 * @endcode
 * 
 * @see xlio_poll_group_buf_free()
 * @since XLIO 1.0
 */
void xlio_socket_buf_free(xlio_socket_t sock, struct xlio_buf *buf);

/**
 * @brief Free a receive buffer (group-specific)
 * 
 * Returns a receive buffer to the system for reuse. This function can be
 * called from any context and is more efficient than the socket-specific version.
 * 
 * @param group The polling group (may be unused)
 * @param buf The buffer descriptor to free
 * 
 * @note The buffer must not be accessed after calling this function.
 * 
 * @see xlio_socket_buf_free()
 * @since XLIO 1.0
 */
void xlio_poll_group_buf_free(xlio_poll_group_t group, struct xlio_buf *buf);

/** @} */ // end of xlio_rx group

/**
 * @defgroup xlio_callbacks Event Callbacks
 * @brief Callback functions for handling socket events
 * 
 * The XLIO Socket API uses callbacks to notify applications of various
 * events including connection state changes, data arrival, and completion
 * of zero-copy operations.
 * 
 * @{
 */

/**
 * @brief Socket event callback function type
 * 
 * This callback is invoked when socket state changes occur, such as
 * connection establishment, errors, or termination.
 * 
 * @param sock The socket generating the event
 * @param userdata_sq User data associated with the socket
 * @param event The event type (XLIO_SOCKET_EVENT_*)
 * @param value Event-specific value (error code for ERROR events, 0 otherwise)
 * 
 * @par Event Types:
 * - XLIO_SOCKET_EVENT_ESTABLISHED: TCP connection established
 * - XLIO_SOCKET_EVENT_TERMINATED: Socket terminated, no further events
 * - XLIO_SOCKET_EVENT_CLOSED: Passive close by remote peer
 * - XLIO_SOCKET_EVENT_ERROR: Error occurred, see value for error code
 * 
 * @par Error Codes (for ERROR events):
 * - ECONNABORTED: Connection aborted by local side
 * - ECONNRESET: Connection reset by remote side
 * - ECONNREFUSED: Connection refused during handshake
 * - ETIMEDOUT: Connection timed out
 * 
 * @note Send operations are allowed only for the ESTABLISHED event.
 * 
 * @see xlio_poll_group_attr
 * @since XLIO 1.0
 */
typedef void (*xlio_socket_event_cb_t)(xlio_socket_t sock, uintptr_t userdata_sq, 
                                       int event, int value);

/**
 * @brief Zero-copy completion callback function type
 * 
 * This callback is invoked when a zero-copy send operation completes,
 * allowing the application to reclaim or reuse the transmitted buffers.
 * 
 * @param sock The socket that completed the operation
 * @param userdata_sq User data associated with the socket
 * @param userdata_op User data associated with the specific operation
 * 
 * @par Calling Contexts:
 * - xlio_poll_group_poll() (most common)
 * - xlio_socket_send() (if data is immediately flushed)
 * - xlio_socket_flush() / xlio_poll_group_flush()
 * - xlio_socket_destroy()
 * 
 * @note Send operations are allowed in this callback unless the socket
 * is being destroyed.
 * 
 * @see xlio_socket_send_attr
 * @since XLIO 1.0
 */
typedef void (*xlio_socket_comp_cb_t)(xlio_socket_t sock, uintptr_t userdata_sq, 
                                      uintptr_t userdata_op);

/**
 * @brief Receive data callback function type
 * 
 * This callback is invoked when TCP payload arrives on a socket.
 * Each call provides a single contiguous buffer containing received data.
 * 
 * @param sock The socket that received the data
 * @param userdata_sq User data associated with the socket
 * @param data Pointer to the received data
 * @param len Length of the received data
 * @param buf Buffer descriptor that must be returned via xlio_*_buf_free()
 * 
 * @note The data pointer is valid only until the buffer is freed.
 * The buffer's userdata field can be used during user ownership.
 * 
 * @see xlio_socket_buf_free()
 * @see xlio_poll_group_buf_free()
 * @since XLIO 1.0
 */
typedef void (*xlio_socket_rx_cb_t)(xlio_socket_t sock, uintptr_t userdata_sq, 
                                    void *data, size_t len, struct xlio_buf *buf);

/**
 * @brief Accept callback function type
 * 
 * This callback is invoked when a new connection is accepted on a
 * listening socket. The new socket is automatically created and
 * associated with the same polling group.
 * 
 * @param sock The newly accepted socket
 * @param parent_sock The listening socket that accepted the connection
 * @param parent_userdata_sq User data from the parent socket
 * 
 * @note The new socket inherits the polling group from the parent but
 * may need additional configuration (e.g., userdata_sq update).
 * 
 * @see xlio_socket_listen()
 * @since XLIO 1.0
 */
typedef void (*xlio_socket_accept_cb_t)(xlio_socket_t sock, xlio_socket_t parent_sock,
                                        uintptr_t parent_userdata_sq);

/**
 * @brief Memory allocation callback function type
 * 
 * This callback is invoked when XLIO allocates memory regions that
 * can be used for RX buffers. Applications can use this information
 * for memory management or preparation.
 * 
 * @param addr Base address of the allocated memory
 * @param len Size of the allocated memory
 * @param hugepage_size Page size if hugepages are used, 0 for regular pages
 * 
 * @note If hugepage_size is non-zero, both addr and len are aligned to
 * the page size boundary. For external allocators, hugepage_size is
 * always reported as zero.
 * 
 * @see xlio_init_attr
 * @since XLIO 1.0
 */
typedef void (*xlio_memory_cb_t)(void *addr, size_t len, size_t hugepage_size);

/**
 * @brief Standard receive callback function type
 * 
 * Traditional receive callback for packet notification on UDP sockets.
 * This is different from the XLIO Socket API RX callback and is used
 * with the standard socket API via xlio_register_recv_callback().
 * 
 * @param fd Socket file descriptor receiving the packet
 * @param sz_iov Number of iovec structures in the array
 * @param iov Array of iovec structures containing packet data
 * @param xlio_info Additional packet and socket information
 * @param context User-defined context provided during registration
 * @return Action for XLIO to take with the packet
 * 
 * @par Return Values:
 * - XLIO_PACKET_DROP: Drop packet and recycle buffer
 * - XLIO_PACKET_RECV: Queue packet for normal recv APIs
 * - XLIO_PACKET_HOLD: Application handles queuing, must return descriptor later
 * 
 * @par Callback Context:
 * - Called from user threads during: select, poll, epoll, recv, recvfrom, recvmsg, read, readv
 * - All socket control and send APIs can be called from within callback
 * - iov and xlio_info are only valid until callback returns
 * 
 * @note This callback is for UDP sockets with the standard API, not the XLIO Socket API.
 * 
 * @see xlio_register_recv_callback()
 * @see xlio_info_t
 * @since XLIO 1.0
 */
typedef xlio_recv_callback_retval_t (*xlio_recv_callback_t)(int fd, size_t sz_iov,
                                                            struct iovec iov[],
                                                            struct xlio_info_t *xlio_info,
                                                            void *context);

/** @} */ // end of xlio_callbacks group

/**
 * @defgroup xlio_structures Data Structures
 * @brief Key data structures used by the XLIO Socket API
 * @{
 */

/**
 * @defgroup xlio_socket_options Socket Options
 * @brief Socket option constants for setsockopt()/getsockopt()
 * @{
 */

/**
 * @brief Get XLIO extended API structure
 * 
 * Use with getsockopt() to retrieve the XLIO extended API function pointers.
 */
#define SO_XLIO_GET_API          2800

/**
 * @brief Set ring allocation logic
 * 
 * Use with setsockopt() and xlio_ring_alloc_logic_attr structure to control
 * how rings are allocated for this socket.
 */
#define SO_XLIO_RING_ALLOC_LOGIC 2810

/**
 * @brief Shutdown RX direction only
 * 
 * Use with setsockopt() to shutdown only the receive direction of the socket.
 */
#define SO_XLIO_SHUTDOWN_RX      2821

/**
 * @brief Extended VLAN tag support
 * 
 * Use with setsockopt() to enable extended VLAN tag processing.
 */
#define SO_XLIO_EXT_VLAN_TAG     2824

/**
 * @brief Socket isolation policy
 * 
 * Socket isolation option groups sockets under specified policy.
 * 
 * Supported policies:
 * - SO_XLIO_ISOLATE_DEFAULT: Default behavior according to XLIO configuration
 * - SO_XLIO_ISOLATE_SAFE: Isolate sockets and guarantee thread safety
 * 
 * @note Current limitations:
 * - Supported only by TCP sockets
 * - Must be called after socket() and before listen() or connect()
 * - Must follow thread safety model and XLIO configuration
 */
#define SO_XLIO_ISOLATE         2825
#define SO_XLIO_ISOLATE_DEFAULT 0  /**< Default isolation behavior */
#define SO_XLIO_ISOLATE_SAFE    1  /**< Safe isolation with thread safety */

/** @} */ // end of xlio_socket_options group

/**
 * @defgroup xlio_types Type Definitions
 * @brief Core type definitions for the XLIO Socket API
 * @{
 */

/**
 * @brief Polling group handle
 * 
 * Opaque handle representing a polling group for event management.
 */
typedef uintptr_t xlio_poll_group_t;

/**
 * @brief Socket handle
 * 
 * Opaque handle representing an XLIO high-performance socket.
 */
typedef uintptr_t xlio_socket_t;

/**
 * @brief Receive packet callback return values
 * 
 * Return values for the receive packet notification callback function.
 * These control how XLIO handles the received packet.
 */
typedef enum {
    XLIO_PACKET_DROP, /**< Drop the packet and recycle buffer if no other socket needs it */
    XLIO_PACKET_RECV, /**< Queue the packet on socket ready queue for normal recv APIs */
    XLIO_PACKET_HOLD  /**< Application handles queuing; must return descriptor later */
} xlio_recv_callback_retval_t;

/**
 * @brief Ring allocation logic types
 * 
 * Defines how hardware rings are allocated and shared among sockets.
 */
typedef enum {
    RING_LOGIC_PER_INTERFACE = 0,               /**< One ring per network interface */
    RING_LOGIC_PER_IP = 1,                     /**< One ring per IP address */
    RING_LOGIC_PER_SOCKET = 10,                /**< One ring per socket */
    RING_LOGIC_PER_USER_ID = 11,               /**< One ring per user-defined ID */
    RING_LOGIC_PER_THREAD = 20,                /**< One ring per thread */
    RING_LOGIC_PER_CORE = 30,                  /**< One ring per CPU core */
    RING_LOGIC_PER_CORE_ATTACH_THREADS = 31,   /**< Ring per core with thread attachment */
    RING_LOGIC_PER_OBJECT = 32,                /**< One ring per object */
    RING_LOGIC_ISOLATE = 33,                   /**< Isolated ring allocation */
    RING_LOGIC_LAST                            /**< Sentinel value */
} ring_logic_t;

/**
 * @brief Ring allocation attribute component mask
 * 
 * Bitmask values indicating which fields are valid in xlio_ring_alloc_logic_attr.
 */
typedef enum {
    XLIO_RING_ALLOC_MASK_RING_USER_ID = (1 << 0), /**< user_id field is valid */
    XLIO_RING_ALLOC_MASK_RING_INGRESS = (1 << 1), /**< ingress field is valid */
    XLIO_RING_ALLOC_MASK_RING_ENGRESS = (1 << 2), /**< engress field is valid */
} xlio_ring_alloc_logic_attr_comp_mask;

/** @} */ // end of xlio_types group

/**
 * @defgroup xlio_info_structures Information Structures
 * @brief Structures providing packet and socket information
 * @{
 */

/**
 * @brief Packet and socket information
 * 
 * Structure holding additional information about received packets and socket state.
 * Provided to receive callback functions for enhanced packet processing.
 * 
 * @note Check struct_sz field against sizeof(xlio_info_t) for version compatibility.
 * @note All pointer fields are valid only until the callback returns.
 */
struct __attribute__((packed)) xlio_info_t {
    size_t struct_sz;           /**< Structure size for version compatibility checking */
    void *packet_id;            /**< Handle to packet buffer for zero-copy logic */
    
    /* Packet addressing (network byte order) */
    const struct sockaddr *src; /**< Source address of the packet */
    const struct sockaddr *dst; /**< Destination address of the packet */
    
    /* Packet information */
    size_t payload_sz;          /**< Size of the packet payload in bytes */
    
    /* Socket state information */
    uint32_t socket_ready_queue_pkt_count;  /**< Packets waiting to be read */
    uint32_t socket_ready_queue_byte_count; /**< Bytes waiting to be read */
    
    /* Timestamping information */
    struct timespec hw_timestamp; /**< Hardware timestamp (if available) */
    struct timespec sw_timestamp; /**< Software timestamp */
};

/**
 * @brief Rate limiting configuration
 * 
 * Structure for configuring traffic rate limiting on sockets.
 */
struct xlio_rate_limit_t {
    uint32_t rate;              /**< Rate limit in Kbps */
    uint32_t max_burst_sz;      /**< Maximum burst size in bytes */
    uint16_t typical_pkt_sz;    /**< Typical packet size in bytes for calculations */
};

/**
 * @brief Ring allocation logic attributes
 * 
 * Structure for configuring how hardware rings are allocated for a socket.
 * Used with SO_XLIO_RING_ALLOC_LOGIC socket option.
 * 
 * @note ring_alloc_logic field is mandatory.
 */
struct xlio_ring_alloc_logic_attr {
    uint32_t comp_mask;          /**< Component mask indicating valid fields */
    ring_logic_t ring_alloc_logic; /**< Ring allocation strategy to use */
    uint32_t user_id;            /**< User ID for RING_LOGIC_PER_USER_ID */
    uint32_t ingress : 1;        /**< Apply to RX (ingress) rings */
    uint32_t engress : 1;        /**< Apply to TX (egress) rings */
    uint32_t reserved : 30;      /**< Reserved bits for future use */
};

/** @} */ // end of xlio_info_structures group

/**
 * @defgroup xlio_main_structures Main API Structures
 * @brief Core structures for XLIO Socket API configuration and operation
 * 
 * @note These structures are defined in mellanox/xlio_types.h and documented here
 * for reference. The actual declarations should be included from the main header.
 * @{
 */

/**
 * @typedef struct xlio_init_attr
 * @brief XLIO initialization attributes
 * 
 * Structure containing parameters for XLIO initialization with xlio_init_ex().
 * 
 * @par Memory Management:
 * - memory_cb: Called when XLIO allocates memory regions for RX buffers
 * - memory_alloc/memory_free: Optional external allocator functions
 * 
 * @par External Allocator Notes:
 * - When external allocator is provided, XLIO uses it instead of internal allocation
 * - Current implementation allocates a single memory block during xlio_init_ex()
 * - For external allocators, hugepage_size in memory_cb is always reported as zero
 * 
 * @par Structure Members:
 * - unsigned flags: Initialization flags (reserved for future use)
 * - xlio_memory_cb_t memory_cb: Memory allocation notification callback
 * - void *(*memory_alloc)(size_t): Optional external memory allocator function
 * - void (*memory_free)(void *): Optional external memory deallocator function
 */

/**
 * @typedef struct xlio_poll_group_attr
 * @brief Polling group attributes
 * 
 * Structure containing configuration for a polling group creation and updates.
 * Event callbacks are registered per group, allowing different handling logic
 * for different types of connections.
 * 
 * @par Required Callbacks:
 * - socket_event_cb: Must be provided (handles connection state changes)
 * 
 * @par Optional Callbacks:
 * - socket_comp_cb: Zero-copy completion notifications
 * - socket_rx_cb: Receive data notifications  
 * - socket_accept_cb: New connection acceptance (required for listening sockets)
 * 
 * @par Structure Members:
 * - unsigned flags: Group flags (XLIO_GROUP_FLAG_*)
 * - xlio_socket_event_cb_t socket_event_cb: Socket event callback (required)
 * - xlio_socket_comp_cb_t socket_comp_cb: Zero-copy completion callback (optional)
 * - xlio_socket_rx_cb_t socket_rx_cb: Receive data callback (optional)
 * - xlio_socket_accept_cb_t socket_accept_cb: Accept callback for listening sockets (optional)
 */

/**
 * @typedef struct xlio_socket_attr
 * @brief Socket creation attributes
 * 
 * Structure containing parameters for socket creation with xlio_socket_create().
 * The socket is automatically associated with the specified polling group.
 * 
 * @par Domain Support:
 * - AF_INET: IPv4 support
 * - AF_INET6: IPv6 support
 * 
 * @par User Data:
 * - userdata_sq: Application-defined value for socket identification in callbacks
 * - Can be updated later with xlio_socket_update()
 * 
 * @par Structure Members:
 * - unsigned flags: Socket flags (reserved for future use)
 * - int domain: Address family (AF_INET or AF_INET6)
 * - xlio_poll_group_t group: Polling group to associate socket with
 * - uintptr_t userdata_sq: User data for socket identification in callbacks
 */

/**
 * @typedef struct xlio_socket_send_attr
 * @brief Send operation attributes
 * 
 * Structure containing parameters for send operations (xlio_socket_send/sendv).
 * Controls zero-copy behavior, flushing, and completion tracking.
 * 
 * @par Zero-Copy Operation:
 * - mkey: InfiniBand memory key for registered memory regions
 * - userdata_op: User data provided to completion callback
 * - For zero-copy, memory must be registered with ibv_pd from xlio_socket_get_pd()
 * 
 * @par Inline vs Zero-Copy:
 * - INLINE flag: Data copied to internal buffers, no completion callback
 * - Zero-copy: Data sent directly from user buffer, completion callback invoked
 * 
 * @par Structure Members:
 * - unsigned flags: Send flags (XLIO_SOCKET_SEND_FLAG_*)
 * - uint32_t mkey: Memory key for zero-copy operation (ignored for inline)
 * - uintptr_t userdata_op: User data for completion callback (zero-copy only)
 */

/**
 * @typedef struct xlio_buf
 * @brief Buffer descriptor
 * 
 * Opaque structure representing a receive buffer in zero-copy RX operations.
 * Buffers are provided via RX callbacks and must be returned to XLIO.
 * 
 * @par Buffer Lifecycle:
 * 1. Buffer provided to application via xlio_socket_rx_cb_t
 * 2. Application processes data and optionally uses userdata field
 * 3. Application returns buffer via xlio_socket_buf_free() or xlio_poll_group_buf_free()
 * 
 * @par User Data Field:
 * - Available for application use during buffer ownership
 * - Can be used for reference counting, linking, or other purposes
 * - Not initialized by XLIO
 * 
 * @par Structure Members:
 * - uint64_t userdata: User data field available during buffer ownership
 */

/** @} */ // end of xlio_main_structures group

/** @} */ // end of xlio_structures group

/**
 * @defgroup xlio_constants Constants and Flags
 * @brief Constants and flag definitions for the XLIO Socket API
 * @{
 */

/**
 * @name Polling Group Flags
 * @{
 */
#define XLIO_GROUP_FLAG_SAFE  0x1   /**< Use locks regardless of XLIO configuration */
#define XLIO_GROUP_FLAG_DIRTY 0x2   /**< Keep dirty sockets for batch flushing */
/** @} */

/**
 * @name Send Flags
 * @{
 */
#define XLIO_SOCKET_SEND_FLAG_FLUSH  0x1  /**< Flush socket after queueing data */
#define XLIO_SOCKET_SEND_FLAG_INLINE 0x2  /**< Copy data to internal buffers */
/** @} */

/**
 * @name Socket Events
 * @{
 */
#define XLIO_SOCKET_EVENT_ESTABLISHED 1  /**< TCP connection established */
#define XLIO_SOCKET_EVENT_TERMINATED  2  /**< Socket terminated */
#define XLIO_SOCKET_EVENT_CLOSED      3  /**< Passive close */
#define XLIO_SOCKET_EVENT_ERROR       4  /**< Error occurred */
/** @} */

/**
 * @name Additional Constants
 * @{
 */
#define XLIO_SND_FLAGS_DUMMY MSG_SYN     /**< Dummy send flag (equals 0x400) */
#define CMSG_XLIO_IOCTL_USER_ALLOC 2900  /**< Control message type for user allocator */
/** @} */

/** @} */ // end of xlio_constants group

/**
 * @defgroup xlio_examples Usage Examples
 * @brief Complete examples demonstrating XLIO Socket API usage
 * @{
 */

/**
 * @brief Basic Client Example
 * 
 * This example demonstrates creating a basic TCP client using the XLIO Socket API:
 * 
 * @code
 * #include <mellanox/xlio.h>
 * #include <infiniband/verbs.h>
 * 
 * static bool connected = false;
 * static bool terminated = false;
 * 
 * void socket_event_cb(xlio_socket_t sock, uintptr_t userdata_sq, int event, int value) {
 *     switch (event) {
 *         case XLIO_SOCKET_EVENT_ESTABLISHED:
 *             printf("Connected!\n");
 *             connected = true;
 *             break;
 *         case XLIO_SOCKET_EVENT_TERMINATED:
 *             printf("Connection terminated\n");
 *             terminated = true;
 *             break;
 *         case XLIO_SOCKET_EVENT_ERROR:
 *             printf("Connection error: %d\n", value);
 *             break;
 *     }
 * }
 * 
 * void rx_callback(xlio_socket_t sock, uintptr_t userdata_sq, void *data, 
 *                  size_t len, struct xlio_buf *buf) {
 *     printf("Received %zu bytes: %.*s\n", len, (int)len, (char*)data);
 *     xlio_socket_buf_free(sock, buf);
 * }
 * 
 * int main() {
 *     // Initialize XLIO
 *     struct xlio_init_attr init_attr = {0};
 *     xlio_init_ex(&init_attr);
 * 
 *     // Create polling group
 *     xlio_poll_group_t group;
 *     struct xlio_poll_group_attr group_attr = {
 *         .socket_event_cb = socket_event_cb,
 *         .socket_rx_cb = rx_callback
 *     };
 *     xlio_poll_group_create(&group_attr, &group);
 * 
 *     // Create socket
 *     xlio_socket_t sock;
 *     struct xlio_socket_attr sock_attr = {
 *         .domain = AF_INET,
 *         .group = group,
 *         .userdata_sq = 1
 *     };
 *     xlio_socket_create(&sock_attr, &sock);
 * 
 *     // Connect to server
 *     struct sockaddr_in addr = {
 *         .sin_family = AF_INET,
 *         .sin_port = htons(8080),
 *         .sin_addr.s_addr = inet_addr("127.0.0.1")
 *     };
 *     xlio_socket_connect(sock, (struct sockaddr*)&addr, sizeof(addr));
 * 
 *     // Event loop
 *     while (!terminated) {
 *         xlio_poll_group_poll(group);
 * 
 *         if (connected) {
 *             // Send data
 *             const char *msg = "Hello, server!";
 *             struct xlio_socket_send_attr send_attr = {
 *                 .flags = XLIO_SOCKET_SEND_FLAG_INLINE | XLIO_SOCKET_SEND_FLAG_FLUSH
 *             };
 *             xlio_socket_send(sock, msg, strlen(msg), &send_attr);
 *             connected = false; // Send only once
 *         }
 *     }
 * 
 *     // Cleanup
 *     xlio_socket_destroy(sock);
 *     xlio_poll_group_destroy(group);
 *     xlio_exit();
 *     return 0;
 * }
 * @endcode
 */

/**
 * @brief Basic Server Example
 * 
 * This example demonstrates creating a basic TCP server using the XLIO Socket API:
 * 
 * @code
 * #include <mellanox/xlio.h>
 * #include <infiniband/verbs.h>
 * 
 * void socket_event_cb(xlio_socket_t sock, uintptr_t userdata_sq, int event, int value) {
 *     printf("Socket event: %d, value: %d, userdata: %lx\n", event, value, userdata_sq);
 * }
 * 
 * void accept_cb(xlio_socket_t sock, xlio_socket_t parent, uintptr_t parent_userdata) {
 *     printf("Accepted new connection\n");
 *     xlio_socket_update(sock, 0, (uintptr_t)sock); // Set unique userdata
 * }
 * 
 * void rx_callback(xlio_socket_t sock, uintptr_t userdata_sq, void *data, 
 *                  size_t len, struct xlio_buf *buf) {
 *     printf("Received: %.*s\n", (int)len, (char*)data);
 *     
 *     // Echo back
 *     struct xlio_socket_send_attr attr = {
 *         .flags = XLIO_SOCKET_SEND_FLAG_INLINE | XLIO_SOCKET_SEND_FLAG_FLUSH
 *     };
 *     xlio_socket_send(sock, data, len, &attr);
 *     
 *     xlio_socket_buf_free(sock, buf);
 * }
 * 
 * int main() {
 *     // Initialize XLIO
 *     struct xlio_init_attr init_attr = {0};
 *     xlio_init_ex(&init_attr);
 * 
 *     // Create polling group
 *     xlio_poll_group_t group;
 *     struct xlio_poll_group_attr group_attr = {
 *         .socket_event_cb = socket_event_cb,
 *         .socket_rx_cb = rx_callback,
 *         .socket_accept_cb = accept_cb
 *     };
 *     xlio_poll_group_create(&group_attr, &group);
 * 
 *     // Create listening socket
 *     xlio_socket_t listen_sock;
 *     struct xlio_socket_attr sock_attr = {
 *         .domain = AF_INET,
 *         .group = group,
 *         .userdata_sq = 0xdeadbeef
 *     };
 *     xlio_socket_create(&sock_attr, &listen_sock);
 * 
 *     // Bind and listen
 *     struct sockaddr_in addr = {
 *         .sin_family = AF_INET,
 *         .sin_port = htons(8080),
 *         .sin_addr.s_addr = INADDR_ANY
 *     };
 *     xlio_socket_bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
 *     xlio_socket_listen(listen_sock);
 * 
 *     printf("Server listening on port 8080\n");
 * 
 *     // Event loop
 *     while (true) {
 *         xlio_poll_group_poll(group);
 *     }
 * 
 *     return 0;
 * }
 * @endcode
 */

/** @} */ // end of xlio_examples group

/** @} */ // end of xlio_socket_api group

#ifdef __cplusplus
}
#endif

#endif /* XLIO_ZEROCOPY_API_H */ 