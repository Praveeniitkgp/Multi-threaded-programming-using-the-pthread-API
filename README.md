# Multi-threaded-programming-using-the-pthread-API
This project simulates a visitor-boating experience at FooBar, a botanical garden with multiple attractions.
Visitors tour the attractions and then take a boat ride on a lake. Each visitor and boat is modeled as a separate thread, synchronized using pthreads primitives.

The project demonstrates:

Thread creation and management

Synchronization with custom-built counting semaphores (using mutexes and condition variables)

Resource sharing and handshaking between visitor and boat threads